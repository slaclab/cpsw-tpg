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

#include <cpsw_api_builder.h>
#include <cpsw_api_user.h>

static Path build(const char* ip, unsigned addr)
{
  //
  //  Build
  //

  ProtoStackBuilder bldr = IProtoStackBuilder::create();
  bldr->setSRPVersion              ( IProtoStackBuilder::SRP_UDP_V2 );
  bldr->setUdpPort                 (                  8192 );
  bldr->setSRPTimeoutUS            (                 90000 );
  bldr->setSRPRetryCount           (                     5 );
  // bldr->setSRPMuxVirtualChannel    (                     0 );
  // bldr->useDepack                  (                  true );
  // bldr->useRssi                    (                  true );
  // bldr->setTDestMuxTDEST           (                     0 );

  MMIODev   mmio = IMMIODev::create ("mmio", (1ULL<<32));

  Field f = IIntField::create("reg");
  mmio->addAtAddress( f, addr );

  NetIODev  root = INetIODev::create("fpga", ip);
  root->addAtAddress( mmio, bldr);

  return IPath::create( root );
}

static void usage(const char* p)
{
  printf("Usage: %s [options]\n"
         "  -a <ip> -r <addr>[,value] -s <script>\n",p);
}

int main(int argc, char* argv[])
{
  const char* ip = "192.168.2.10";
  const char* script = 0;
  unsigned addr = 0;
  bool lvalue = false;
  unsigned value;
  char* endptr;

  opterr = 0;

  char opts[32];
  sprintf(opts,"a:r:s:h");

  int c;
  while( (c=getopt(argc,argv,opts))!=-1 ) {
    switch(c) {
    case 'a':
      ip = optarg;
      break;
    case 's':
      script = optarg;
      break;
    case 'r':
      addr = strtoul(optarg,&endptr,0);
      if (*endptr==',') {
        lvalue = true;
        value = strtoul(endptr+1,&endptr,0);
      }
      break;
    case 'h':
      usage(argv[0]); return 1;
    default:
      break;
    }
  }

  Path path = build(ip, addr);

  if (lvalue) { // read, write
    unsigned v;
    IScalVal::create(path->findByName("mmio/reg"))->getVal(&v,1);
    printf("[%08x] %08x\n",addr,v);

    IScalVal::create(path->findByName("mmio/reg"))->setVal(&value,1);
    printf("[%08x]=%08x\n",addr,value);
  }

  unsigned v;
  IScalVal::create(path->findByName("mmio/reg"))->getVal(&v,1);
  printf("[%08x] %08x\n",addr,v);

  return 0;
}
