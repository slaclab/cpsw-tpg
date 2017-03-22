//
//  Builder for the AxiStreamDmaRingWrite module registers
//
#ifndef AxiStreamDmaRingWrite_hh
#define AxiStreamDmaRingWrite_hh

#include <cpsw_api_builder.h>
#include <cpsw_mmio_dev.h>
namespace TPGen {
  class IAxiStreamDmaRingWrite;
  typedef shared_ptr<IAxiStreamDmaRingWrite> AxiStreamDmaRingWrite;
                                                             
  class CAxiStreamDmaRingWriteImpl;
  typedef shared_ptr<CAxiStreamDmaRingWriteImpl> AxiStreamDmaRingWriteImpl;

  class IAxiStreamDmaRingWrite : public virtual IMMIODev {
  public:
    static AxiStreamDmaRingWrite create(const char*,unsigned nbuf=64);
  };

  class CAxiStreamDmaRingWriteImpl : public CMMIODevImpl, public virtual IAxiStreamDmaRingWrite {
  public:
    CAxiStreamDmaRingWriteImpl(Key &k, const char *name);
  };
};

#endif
