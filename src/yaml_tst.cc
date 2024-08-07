//////////////////////////////////////////////////////////////////////////////
// This file is part of 'tpg'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'tpg', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
//
//  List of features to test:
//
//    Sequence engine - sync (fixed, AC+timeslot mask), branch, jump
//                    - reset, multi-engine reset
//                    - destination priority
//                    - triggered jump (triggered sync?)
//                    - bcs/mps fault jump
//                    - notify callback
//            
//    Counters        - interval updates
//                    - interval callback
//
//    Frame data      - content
//                    - FIFO dump
//
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include <cpsw_api_user.h>

static void usage(const char* p)
{
  printf("Usage: %s [options]\n"
         "  -f <yaml file>\n",p);
}

int main(int argc, char* argv[])
{
  const char* yaml = 0;

  int c;
  while( (c=getopt(argc,argv,"f:h"))!=-1 ) {
    switch(c) {
    case 'f':
      yaml = optarg;
      break;
    case 'h':
    default:
      usage(argv[0]); return 1;
      break;
    }
  }

  try {
    Path path = IPath::loadYamlFile(yaml,"NetIODev");
  } catch ( CPSWError e ) {
    fprintf(stderr,"%s", e.getInfo().c_str());
  }

  return 0;
}
