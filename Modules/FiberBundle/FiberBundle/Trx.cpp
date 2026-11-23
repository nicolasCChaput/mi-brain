#include "Trx.h"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <iterator>
#include <set>
#include <vector>

#include <mitkLog.h>
#include <mitkProperties.h>

#include "FilteredFiberBundle.hpp"
#include "ScopedProgress.hpp"

#include <vtkCellArray.h>
#include <vtkFloatArray.h>
#include <vtkIdTypeArray.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>

#include <trx/trx.h>

namespace
{
  // Progress steps used by ReadTrx. Converting the positions is where the
  // time goes (the file is memory-mapped, so it's also where it's read), so
  // it gets almost all the steps.
  constexpr unsigned int LoadSteps = 1;
  constexpr unsigned int PositionsSteps = 100;
  constexpr unsigned int FinalizeSteps = 3; // cells, groups, fiber bundle

  // Converts `count` points (3 values of type T each) from TRX to MITK
  // coordinates into `out`, which must have room for 3 * count floats.
  template <typename T>
  void ConvertPositions(const void* data, size_t count, float* out)
  {
    // Positions inside an uncompressed .trx zip are not guaranteed to be
    // aligned, so copy bytes instead of dereferencing a T pointer.
    const auto* src = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < 3 * count; i += 3)
    {
      T p[3];
      std::memcpy(p, src + i * sizeof(T), sizeof(p));

      // TRX positions are in RAS+ mm, MITK world is LPS mm
      out[i + 0] = -static_cast<float>(p[0]);
      out[i + 1] = -static_cast<float>(p[1]);
      out[i + 2] =  static_cast<float>(p[2]);
    }
  }

  // Appends the integers of type T stored in `bytes` to `out`
  template <typename T>
  void AppendIndices(
    const trx::TypedArray::ByteView& bytes, std::vector<vtkIdType>& out)
  {
    // Same alignment concern as for the positions
    const size_t count = bytes.size / sizeof(T);
    out.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
      T value;
      std::memcpy(&value, bytes.data + i * sizeof(T), sizeof(T));
      out.push_back(static_cast<vtkIdType>(value));
    }
  }

  // TRX groups are lists of streamline indices, usually uint32 but any
  // integer type is allowed by the spec. Groups with an unsupported dtype are
  // skipped and indices out of [0, nbStreamlines) are dropped, with a warning.
  mitk::FilteredFiberBundle::StreamlineGroups ReadGroups(
    const trx::AnyTrxFile& trxFile)
  {
    const auto nbStreamlines = static_cast<vtkIdType>(trxFile.num_streamlines());

    mitk::FilteredFiberBundle::StreamlineGroups groups;
    for (const auto& nameAndArray : trxFile.groups)
    {
      const auto& name = nameAndArray.first;
      const auto& array = nameAndArray.second;

      std::vector<vtkIdType> indices;
      const auto bytes = array.to_bytes();
      const auto& dtype = array.dtype;
      if      (dtype == "uint8")  { AppendIndices<uint8_t>(bytes, indices); }
      else if (dtype == "uint16") { AppendIndices<uint16_t>(bytes, indices); }
      else if (dtype == "uint32") { AppendIndices<uint32_t>(bytes, indices); }
      else if (dtype == "uint64") { AppendIndices<uint64_t>(bytes, indices); }
      else if (dtype == "int8")   { AppendIndices<int8_t>(bytes, indices); }
      else if (dtype == "int16")  { AppendIndices<int16_t>(bytes, indices); }
      else if (dtype == "int32")  { AppendIndices<int32_t>(bytes, indices); }
      else if (dtype == "int64")  { AppendIndices<int64_t>(bytes, indices); }
      else
      {
        MITK_WARN << "TRX group '" << name << "' ignored: unsupported dtype "
                  << dtype;
        continue;
      }

      const auto invalid = std::remove_if(indices.begin(), indices.end(),
        [nbStreamlines](vtkIdType i) { return i < 0 || i >= nbStreamlines; });
      if (invalid != indices.end())
      {
        MITK_WARN << "TRX group '" << name << "': "
                  << std::distance(invalid, indices.end())
                  << " out of range streamline indices ignored";
        indices.erase(invalid, indices.end());
      }

      groups[name] = std::move(indices);
    }
    return groups;
  }

  // TRX groups are stored as files named "groups/<name>.uint32", so the name
  // can't contain a dot (it would be read as a dimension or a dtype) nor
  // anything that isn't allowed in a file name.
  std::string TrxGroupName(const std::string& name)
  {
    std::string trxName = name;
    for (auto& c : trxName)
    {
      if (c == '.' || c == '/' || c == '\\' || c == ':' || c == '*' ||
          c == '?' || c == '"' || c == '<' || c == '>' || c == '|' ||
          static_cast<unsigned char>(c) < 0x20)
      {
        c = '_';
      }
    }
    if (trxName.empty()) { trxName = "group"; }
    return trxName;
  }

  // Adds the groups to the stream. TRX groups are written as uint32, sorted
  // and without duplicates. Indices out of [0, nbStreamlines) are dropped and
  // groups left empty are not written.
  void WriteGroups(
    trx::TrxStream& stream,
    const std::map<std::string, std::vector<vtkIdType>>& groups,
    const vtkIdType nbStreamlines)
  {
    std::set<std::string> usedNames;
    for (const auto& nameAndIndices : groups)
    {
      std::vector<uint32_t> indices;
      indices.reserve(nameAndIndices.second.size());
      for (const auto idx : nameAndIndices.second)
      {
        if (idx >= 0 && idx < nbStreamlines)
        {
          indices.push_back(static_cast<uint32_t>(idx));
        }
      }
      std::sort(indices.begin(), indices.end());
      indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
      if (indices.empty()) { continue; }

      // Two names can become identical once sanitized
      const auto baseName = TrxGroupName(nameAndIndices.first);
      auto trxName = baseName;
      for (int i = 2; usedNames.count(trxName); ++i)
      {
        trxName = baseName + "_" + std::to_string(i);
      }
      usedNames.insert(trxName);
      if (trxName != nameAndIndices.first)
      {
        MITK_INFO << "TRX group '" << nameAndIndices.first
                  << "' written as '" << trxName << "'";
      }

      stream.push_group_from_indices(trxName, indices);
    }
  }

  // Builds a polydata with one line cell per streamline from the converted
  // points and the VTK offsets (first point of each line + a sentinel equal
  // to the number of points). Streamlines are stored contiguously, so point
  // i is simply the i-th point of the connectivity.
  vtkSmartPointer<vtkPolyData> LinesPolyData(
    vtkFloatArray* coords, vtkIdTypeArray* offsets)
  {
    const vtkIdType nbPoints = coords->GetNumberOfTuples();
    auto connectivity = vtkSmartPointer<vtkIdTypeArray>::New();
    connectivity->SetNumberOfValues(nbPoints);
    for (vtkIdType i = 0; i < nbPoints; ++i)
    {
      connectivity->SetValue(i, i);
    }

    auto lines = vtkSmartPointer<vtkCellArray>::New();
    lines->SetData(offsets, connectivity);

    auto points = vtkSmartPointer<vtkPoints>::New();
    points->SetData(coords);

    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetLines(lines);
    polyData->ComputeBounds();
    return polyData;
  }

  // Builds the polydata (points + one line cell per streamline) from the
  // positions and offsets of the TRX file. Advances `progress` from LoadSteps
  // up to LoadSteps + PositionsSteps + 1.
  vtkSmartPointer<vtkPolyData> TrxToPolyData(
    const trx::AnyTrxFile& trxFile, ScopedProgress& progress)
  {
    const auto nbPoints = static_cast<vtkIdType>(trxFile.num_vertices());

    auto coords = vtkSmartPointer<vtkFloatArray>::New();
    coords->SetNumberOfComponents(3);
    coords->SetNumberOfTuples(nbPoints);
    float* dst = coords->GetPointer(0);

    // Read by chunks to avoid materializing a converted copy of the whole
    // file. The positions are memory-mapped, so this is also where most of
    // the disk reading happens: use enough chunks to get a smooth progress.
    const size_t totalBytes = trxFile.positions.to_bytes().size;
    const size_t chunkBytes = std::clamp<size_t>(
      totalBytes / PositionsSteps, 1024 * 1024, 64 * 1024 * 1024);
    const auto totalPoints = static_cast<size_t>(nbPoints);
    trxFile.for_each_positions_chunk(chunkBytes,
      [dst, totalPoints, &progress](
        trx::TrxScalarType dtype, const void* data, size_t offset, size_t count)
      {
        float* out = dst + 3 * offset;
        switch (dtype)
        {
          case trx::TrxScalarType::Float16: ConvertPositions<Eigen::half>(data, count, out); break;
          case trx::TrxScalarType::Float32: ConvertPositions<float>(data, count, out); break;
          case trx::TrxScalarType::Float64: ConvertPositions<double>(data, count, out); break;
        }
        const auto done = static_cast<unsigned int>(
          (offset + count) * PositionsSteps / totalPoints);
        progress.AdvanceTo(LoadSteps + done);
      });

    // TRX offsets map directly onto the VTK offsets + connectivity layout.
    // Some files omit the trailing sentinel, so add it if it is missing.
    std::vector<uint64_t> trxOffsets = trxFile.offsets_u64;
    if (trxOffsets.empty() || trxOffsets.back() != static_cast<uint64_t>(nbPoints))
    {
      trxOffsets.push_back(static_cast<uint64_t>(nbPoints));
    }
    const auto nbOffsets = static_cast<vtkIdType>(trxOffsets.size());

    auto offsets = vtkSmartPointer<vtkIdTypeArray>::New();
    offsets->SetNumberOfValues(nbOffsets);
    for (vtkIdType i = 0; i < nbOffsets; ++i)
    {
      offsets->SetValue(i, static_cast<vtkIdType>(trxOffsets[i]));
    }

    auto polyData = LinesPolyData(coords, offsets);
    progress.Advance();
    return polyData;
  }

  // Same as TrxToPolyData, but only for the `selected` streamlines (sorted,
  // without duplicates): new streamline i is streamline selected[i] of the
  // file. The memory-mapped positions of the selected streamlines are read
  // directly, so the other streamlines are never read nor converted.
  vtkSmartPointer<vtkPolyData> TrxSubsetToPolyData(
    const trx::AnyTrxFile& trxFile,
    const std::vector<vtkIdType>& selected,
    ScopedProgress& progress)
  {
    const auto& trxOffsets = trxFile.offsets_u64;
    const auto nbFilePoints = static_cast<uint64_t>(trxFile.num_vertices());
    // Some files omit the trailing sentinel
    const auto first = [&](vtkIdType s) { return trxOffsets[s]; };
    const auto last = [&](vtkIdType s) {
      return static_cast<size_t>(s) + 1 < trxOffsets.size()
        ? trxOffsets[s + 1] : nbFilePoints;
    };

    vtkIdType nbPoints = 0;
    for (const auto s : selected)
    {
      nbPoints += static_cast<vtkIdType>(last(s) - first(s));
    }

    auto coords = vtkSmartPointer<vtkFloatArray>::New();
    coords->SetNumberOfComponents(3);
    coords->SetNumberOfTuples(nbPoints);
    float* dst = coords->GetPointer(0);

    auto offsets = vtkSmartPointer<vtkIdTypeArray>::New();
    offsets->SetNumberOfValues(static_cast<vtkIdType>(selected.size()) + 1);

    // float16, float32 or float64, deduced from the size of the positions
    const auto bytes = trxFile.positions.to_bytes();
    const size_t valueBytes = bytes.size / (3 * nbFilePoints);

    vtkIdType written = 0;
    for (size_t i = 0; i < selected.size(); ++i)
    {
      const auto s = selected[i];
      const auto count = static_cast<size_t>(last(s) - first(s));
      const void* data = bytes.data + first(s) * 3 * valueBytes;
      float* out = dst + 3 * written;
      switch (valueBytes)
      {
        case 2: ConvertPositions<Eigen::half>(data, count, out); break;
        case 4: ConvertPositions<float>(data, count, out); break;
        case 8: ConvertPositions<double>(data, count, out); break;
        default: throw std::runtime_error("Unsupported TRX positions dtype.");
      }
      offsets->SetValue(static_cast<vtkIdType>(i), written);
      written += static_cast<vtkIdType>(count);

      const auto done = static_cast<unsigned int>(
        (i + 1) * PositionsSteps / selected.size());
      progress.AdvanceTo(LoadSteps + done);
    }
    offsets->SetValue(static_cast<vtkIdType>(selected.size()), written);

    auto polyData = LinesPolyData(coords, offsets);
    progress.Advance();
    return polyData;
  }

  // Keeps only the streamlines of the chosen groups. `groups` becomes the
  // chosen groups, with indices in the kept streamlines. Returns the kept
  // streamlines (indices in the file, sorted, without duplicates).
  std::vector<vtkIdType> SelectGroups(
    mitk::FilteredFiberBundle::StreamlineGroups& groups,
    const std::set<std::string>& chosen,
    const size_t nbStreamlines)
  {
    mitk::FilteredFiberBundle::StreamlineGroups kept;
    for (const auto& name : chosen)
    {
      const auto it = groups.find(name);
      if (it != groups.end())
      {
        kept[name] = std::move(it->second);
      }
    }

    std::vector<vtkIdType> selected;
    for (const auto& nameAndIndices : kept)
    {
      selected.insert(selected.end(),
        nameAndIndices.second.begin(), nameAndIndices.second.end());
    }
    std::sort(selected.begin(), selected.end());
    selected.erase(std::unique(selected.begin(), selected.end()), selected.end());

    // ReadGroups dropped the out of range indices, so all are mapped
    std::vector<vtkIdType> newIndex(nbStreamlines, -1);
    for (size_t i = 0; i < selected.size(); ++i)
    {
      newIndex[selected[i]] = static_cast<vtkIdType>(i);
    }
    for (auto& nameAndIndices : kept)
    {
      for (auto& idx : nameAndIndices.second)
      {
        idx = newIndex[idx];
      }
    }

    groups = std::move(kept);
    return selected;
  }

  TrxFileInfo MakeInfo(
    const std::string& filename,
    const trx::AnyTrxFile& trxFile,
    const mitk::FilteredFiberBundle::StreamlineGroups& groups)
  {
    TrxFileInfo info;
    const auto slash = filename.find_last_of("/\\");
    info.filename = slash == std::string::npos
      ? filename : filename.substr(slash + 1);
    info.nbPoints = trxFile.num_vertices();
    info.nbStreamlines = trxFile.num_streamlines();
    for (const auto& nameAndIndices : groups)
    {
      info.groups.emplace_back(
        nameAndIndices.first, nameAndIndices.second.size());
    }
    return info;
  }

  TrxLoadPrompt& LoadPrompt()
  {
    static TrxLoadPrompt prompt;
    return prompt;
  }
}

void FIBERBUNDLE_EXPORT SetTrxLoadPrompt(TrxLoadPrompt prompt)
{
  LoadPrompt() = std::move(prompt);
}

vtkIdType FIBERBUNDLE_EXPORT ReadTrx(
  const std::string& filename, mitk::FilteredFiberBundle* fiber)
{
  ScopedProgress progress(LoadSteps + PositionsSteps + FinalizeSteps);

  // Fast for uncompressed files (memory-mapped), but compressed ones are
  // extracted to a temporary directory here.
  auto trxFile = trx::load_any(filename);
  progress.Advance(LoadSteps);
  if (trxFile.num_vertices() == 0)
  {
    return 0;
  }

  auto groups = ReadGroups(trxFile);

  TrxLoadChoice choice;
  if (LoadPrompt())
  {
    const auto answer = LoadPrompt()(MakeInfo(filename, trxFile, groups));
    if (!answer)
    {
      // trxFile's destructor removes the temporary directory of zipped files
      throw TrxLoadCancelled();
    }
    choice = *answer;
  }

  vtkSmartPointer<vtkPolyData> polyData;
  if (!choice.groups)
  {
    polyData = TrxToPolyData(trxFile, progress);
  }
  else
  {
    const auto selected =
      SelectGroups(groups, *choice.groups, trxFile.num_streamlines());
    if (selected.empty())
    {
      return 0;
    }
    polyData = TrxSubsetToPolyData(trxFile, selected, progress);
  }
  progress.Advance();

  // SetFiberPolyData deep copies, so the mapped file (and the temporary
  // directory created for zipped .trx files) can be released now.
  trxFile.close();

  fiber->SetFiberPolyData(polyData);
  fiber->SetStreamlineGroups(groups);
  if (LoadPrompt())
  {
    fiber->SetProperty(TrxInitialColoringProperty,
      mitk::IntProperty::New(static_cast<int>(choice.coloring)));
  }

  return polyData->GetNumberOfPoints();
}

void FIBERBUNDLE_EXPORT WriteTrx(
  const std::string& filename,
  vtkPolyData* polydata,
  const std::map<std::string, std::vector<vtkIdType>>& groups)
{
  vtkCellArray* lines = polydata->GetLines();
  vtkPoints* points = polydata->GetPoints();
  if (lines == nullptr || points == nullptr || lines->GetNumberOfCells() == 0)
  {
    throw std::runtime_error("Cannot write an empty fiber bundle as TRX.");
  }

  // TrxStream spools positions to a temporary file, so we never hold a
  // second full copy of the bundle in memory.
  trx::TrxStream stream("float32");

  // VOXEL_TO_RASMM and DIMENSIONS are mandatory in the TRX header but
  // TrxStream does not set them. No reference image is known here, so use
  // the same defaults as trx::TrxFile.
  stream.set_voxel_to_rasmm(Eigen::Matrix4f::Identity());
  stream.set_dimensions({1, 1, 1});

  // Reused for all streamlines, re-allocated only for longer ones
  std::vector<float> pointsOnLine;
  pointsOnLine.reserve(3 * 500);

  vtkIdType nbPoints;
  const vtkIdType* ids;
  lines->InitTraversal();
  while (lines->GetNextCell(nbPoints, ids))
  {
    pointsOnLine.clear(); // Not supposed to free memory
    for (vtkIdType i = 0; i < nbPoints; ++i)
    {
      double p[3];
      points->GetPoint(ids[i], p);

      // MITK world is LPS mm, TRX positions are in RAS+ mm
      pointsOnLine.push_back(-static_cast<float>(p[0]));
      pointsOnLine.push_back(-static_cast<float>(p[1]));
      pointsOnLine.push_back( static_cast<float>(p[2]));
    }
    stream.push_streamline(pointsOnLine.data(), static_cast<size_t>(nbPoints));
  }

  WriteGroups(stream, groups, lines->GetNumberOfCells());

  // Writes the header, positions, offsets and groups. trx-cpp picks a zipped
  // or directory output from the file name.
  stream.finalize<float>(filename);
}
