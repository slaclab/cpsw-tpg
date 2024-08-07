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
#include "app.hh"
#include "tpg_yaml.hh"
#include "sequence_engine_yaml.hh"
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>

#include <cpsw_api_user.h>
#include <cpsw_yaml_keydefs.h>
#include <cpsw_yaml.h>

#include "hps_utils.hh"

static void usage(const char* p)
{
  printf("Usage: %s -y <yaml_file> [options]\n"
         "  -a <ip>\n",p);
  printf("%s",TPGen::usage());
}

#define CPSW_TRY_CATCH(X)       try {   \
        (X);                            \
    } catch (CPSWError &e) {            \
        fprintf(stderr,                 \
                "CPSW Error: %s at %s, line %d\n",     \
                e.getInfo().c_str(),    \
                __FILE__, __LINE__);    \
        throw e;                        \
    }

int main(int argc, char* argv[])
{
  const char* ip = "192.168.2.10";
  const char* yaml = 0;
  unsigned verbosity = 0;

  opterr = 0;

  char opts[32];
  sprintf(opts,"a:y:vh%s",TPGen::opts());

  int c;
  while( (c=getopt(argc,argv,opts))!=-1 ) {
    switch(c) {
    case 'a':
      ip = optarg;
      break;
    case 'y':
      yaml = optarg;
      break;
    case 'v':
      verbosity++;
      break;
    case 'h':
      usage(argv[0]); return 1;
    default:
      break;
    }
  }

  TPGen::TPG* p;
  if (yaml) {
    TPGen::SequenceEngineYaml::verbosity(verbosity);
    IYamlFixup* fixup = new Cphw::IpAddrFixup(ip);
    Path path = IPath::loadYamlFile(yaml,"NetIODev",0,fixup);
    CPSW_TRY_CATCH ( p = new TPGen::TPGYaml(path) );
  }
  else {
    usage(argv[0]);
    exit(1);
  }

  optind = 0;
  //  return execute(argc, argv, p, false);
  CPSW_TRY_CATCH ( execute(argc, argv, p, true) );
  return 0;
}
