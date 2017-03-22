//
//  Builder for the RssiCore module registers
//
#ifndef RssiCore_hh
#define RssiCore_hh

#include <cpsw_api_builder.h>
#include <cpsw_mmio_dev.h>

namespace TPGen {
  class IRssiCore;
  typedef shared_ptr<IRssiCore> RssiCore;
                                                             
  class CRssiCoreImpl;
  typedef shared_ptr<CRssiCoreImpl> RssiCoreImpl;

  class IRssiCore : public virtual IMMIODev {
  public:
    static RssiCore create(const char*);
  };

  class CRssiCoreImpl : public CMMIODevImpl, public virtual IRssiCore {
  public:
    CRssiCoreImpl(Key &k, const char *name);
  };
};

#endif
