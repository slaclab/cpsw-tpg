//////////////////////////////////////////////////////////////////////////////
// This file is part of 'tpg'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'tpg', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#ifndef TPG_APP_HH
#define TPG_APP_HH

namespace TPGen {
  class TPG;	
  int execute(int argc, char* argv[], TPGen::TPG*, bool lAsync=true);
  const char* usage();
  const char* opts();
};

#endif
