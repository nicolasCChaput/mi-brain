#ifndef IMEKA_TRX_WRITER_HPP_INCLUDED
#define IMEKA_TRX_WRITER_HPP_INCLUDED

#include <mitkAbstractFileWriter.h>

namespace mitk
{

/** \brief Writes fiber bundles as TRX tractograms (.trx).
*
* The streamlines are converted from MITK world (LPS mm) to TRX RAS+ mm. The
* streamline groups of a FilteredFiberBundle are written as TRX groups.
* Colors and other per-point/per-streamline data are not written.
*/
class FiberBundleTrxWriter : public mitk::AbstractFileWriter
{
public:
  FiberBundleTrxWriter();
  FiberBundleTrxWriter(const FiberBundleTrxWriter & other);
  virtual FiberBundleTrxWriter* Clone() const override;
  virtual ~FiberBundleTrxWriter() {}

  using mitk::AbstractFileWriter::Write;
  virtual void Write() override;
};

} // namespace mitk

#endif // IMEKA_TRX_WRITER_HPP_INCLUDED
