// TRX writing: WriteTrx (see Trx.h). The streamlines of a vtkPolyData and
// their groups are written with trx-cpp's TrxStream.

#include "Trx.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <mitkLog.h>

#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>

#include <trx/trx.h>

namespace
{
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
