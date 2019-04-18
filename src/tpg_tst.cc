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
#include "tpg_cpsw.hh"
#include "tpg_yaml.hh"
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
  printf("Usage: %s [options]\n"
         "  -a <ip>\n",p);
  printf("%s",TPGen::usage());
}

int main(int argc, char* argv[])
{
  const char* ip = "192.168.2.10";
  const char* yaml = 0;

  opterr = 0;

  char opts[32];
  sprintf(opts,"a:y:h%s",TPGen::opts());

  int c;
  while( (c=getopt(argc,argv,opts))!=-1 ) {
    switch(c) {
    case 'a':
      ip = optarg;
      break;
    case 'y':
      yaml = optarg;
      break;
    case 'h':
      usage(argv[0]); return 1;
    default:
      break;
    }
  }

  TPGen::TPG* p;
  if (yaml) {
    IYamlFixup* fixup = new Cphw::IpAddrFixup(ip);
    Path path = IPath::loadYamlFile(yaml,"NetIODev",0,fixup);
    p = new TPGen::TPGYaml(path);
  }
  else 
    p = new TPGen::TPGCpsw(ip);

  optind = 0;
  //  return execute(argc, argv, p, false);
  return execute(argc, argv, p, true);
}
