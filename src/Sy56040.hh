//
//  Builder for the RamControl module registers
//
#ifndef Sy56040_hh
#define Sy56040_hh

#include <cpsw_api_builder.h>
#include <cpsw_mmio_dev.h>

namespace TPGen {
  class ISy56040;
  typedef shared_ptr<ISy56040> Sy56040;
                                                             
  class CSy56040Impl;
  typedef shared_ptr<CSy56040Impl> Sy56040Impl;

  class ISy56040 : public virtual IMMIODev {
  public:
    static Sy56040 create(const char*);
  };

  class CSy56040Impl : public CMMIODevImpl, public virtual ISy56040 {
  public:
    CSy56040Impl(Key &k, const char *name);
  };
};

#endif
