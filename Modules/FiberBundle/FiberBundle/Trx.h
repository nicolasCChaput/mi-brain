#ifndef IMEKA_TRX_READER_HPP_INCLUDED
#define IMEKA_TRX_READER_HPP_INCLUDED

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <vtkType.h>

#include "FiberBundleExports.h"
#include "FilteredFiberBundle.hpp"

class vtkPolyData;

// What is known about a TRX file before its streamlines are converted. Shown
// to the user by the load prompt (see SetTrxLoadPrompt).
struct TrxFileInfo
{
  std::string filename; // Without the directory
  size_t nbPoints = 0;
  size_t nbStreamlines = 0;
  // Name and number of (valid) streamlines of each group, sorted by name
  std::vector<std::pair<std::string, size_t>> groups;
};

// What the user chose to load.
struct TrxLoadChoice
{
  mitk::FilteredFiberBundle::Coloring coloring =
    mitk::FilteredFiberBundle::Orientation;
  // Names of the groups to load. No value: load the whole file. Otherwise,
  // only the streamlines of these groups (and only these groups) are loaded.
  std::optional<std::set<std::string>> groups;
};

// Asks the user how to load a TRX file. Returns std::nullopt to cancel.
using TrxLoadPrompt =
  std::function<std::optional<TrxLoadChoice>(const TrxFileInfo&)>;

// Sets the prompt called by ReadTrx once the file is opened, before any
// streamline is converted. Registered by the GUI (org.imeka.mibrain.eager).
// Without a prompt (tests, command line), the whole file is loaded with the
// default coloring. Pass nullptr to remove it.
void FiberBundle_EXPORT SetTrxLoadPrompt(TrxLoadPrompt);

// Thrown by ReadTrx when the user cancels the load prompt.
struct TrxLoadCancelled : std::runtime_error
{
  TrxLoadCancelled() : std::runtime_error("Loading cancelled by the user.") {}
};

// IntProperty set on the fiber bundle by ReadTrx: the coloring chosen in the
// load prompt (a FilteredFiberBundle::Coloring). Applied and removed by
// FibersManager::FibersAdded once the bundle has a node.
constexpr const char* TrxInitialColoringProperty = "trx.InitialColoring";

// Reads a .trx file (zipped or not) into `fiber`. Positions are converted
// from TRX RAS+ mm to MITK world (LPS mm) and the TRX groups are stored as
// the streamline groups of the fiber bundle. Other TRX data (dpv, dps, dpg,
// header affine) are not loaded. Drives the MITK progress bar.
// If a load prompt is set, it is called once the file is opened; only the
// streamlines of the chosen groups are then loaded, and the chosen coloring
// is stored in the TrxInitialColoringProperty of `fiber`.
// Returns the number of points read (0 for an empty file or an empty
// selection, in which case `fiber` is left untouched). Throws if trx-cpp
// can't load the file, and TrxLoadCancelled if the user cancels.
vtkIdType FiberBundle_EXPORT ReadTrx(
  const std::string&, mitk::FilteredFiberBundle*);

// Writes the streamlines of the polydata (MITK world, LPS mm) to a .trx file.
// Each group (name -> streamline indices in the polydata) is written as a TRX
// group. Characters that TRX can't store in a group name ('.', '/', '\\', ...)
// are replaced by '_'. Positions are always written as float32, and only the
// streamlines and groups are written (no colors nor TRK properties).
// Throws std::runtime_error if the polydata has no streamline.
void FiberBundle_EXPORT WriteTrx(
  const std::string&,
  vtkPolyData*,
  const std::map<std::string, std::vector<vtkIdType>>& groups = {});

#endif // IMEKA_TRX_READER_HPP_INCLUDED
