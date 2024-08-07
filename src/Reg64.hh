//////////////////////////////////////////////////////////////////////////////
// This file is part of 'tpg'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'tpg', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#ifndef Cphw_Reg64_hh
#define Cphw_Reg64_hh

#include "pds/cphw/Reg.hh"

namespace Pds {
  namespace Cphw {
    class Reg64 {
    public:
      Reg64& operator=(const uint32_t v) { low=unsigned(v); return *this; }
      operator uint64_t() const { uint64_t v=unsigned(high); v<<=32; v|=unsigned(low); return v; }
      operator uint32_t() const { return unsigned(low); }
    public:
      Reg low;
      Reg high;
    };
  };
};

#endif
