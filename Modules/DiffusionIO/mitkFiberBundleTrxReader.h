
#ifndef __mitkFiberBundleTrxReader_h
#define __mitkFiberBundleTrxReader_h

#include <mitkCommon.h>
#include "FiberBundle/mitkFiberBundle.h"
#include <vtkSmartPointer.h>

#include <mitkAbstractFileReader.h>


namespace mitk
{

  /** \brief Reads TRX tractograms (.trx) into a FilteredFiberBundle.
  *
  * The streamlines are converted to MITK world (LPS mm) and the TRX groups
  * are kept as streamline groups, which are shown as tract groups under the
  * dataset node. See FiberBundle/Trx.h for what is (not) read.
  */

  class FiberBundleTrxReader : public AbstractFileReader
  {
  public:

    FiberBundleTrxReader();
    ~FiberBundleTrxReader() override{}
    FiberBundleTrxReader(const FiberBundleTrxReader& other);
    FiberBundleTrxReader * Clone() const override;

    std::vector<itk::SmartPointer<BaseData> > DoRead() override;

  private:

    us::ServiceRegistration<mitk::IFileReader> m_ServiceReg;
  };

} //namespace MITK

#endif // __mitkFiberBundleTrxReader_h