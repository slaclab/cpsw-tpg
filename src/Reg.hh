//////////////////////////////////////////////////////////////////////////////
// This file is part of 'tpg'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'tpg', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#ifndef Cphw_Reg_hh
#define Cphw_Reg_hh

#include <stdint.h>

namespace Pds {
  namespace Cphw {
    class Reg {
    public:
      Reg& operator=(const unsigned);
      operator unsigned() const;
    public:
      void setBit  (unsigned);
      void clearBit(unsigned);
    public:
      static void set(const char* ip,
                      unsigned short port,
                      unsigned mem, 
                      unsigned long long memsz=(1ULL<<32));
    private:
      uint32_t _reserved;
    };
  };
};

#endif
