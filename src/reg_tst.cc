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
         "  -a <ip> -r <addr>\n",p);
}

int main(int argc, char* argv[])
{
  const char* ip = "192.168.2.10";
  unsigned addr = 0;

  opterr = 0;

  char opts[32];
  sprintf(opts,"a:r:h");

  int c;
  while( (c=getopt(argc,argv,opts))!=-1 ) {
    switch(c) {
    case 'a':
      ip = optarg;
      break;
    case 'r':
      addr = strtoul(optarg,NULL,0);
      break;
    case 'h':
      usage(argv[0]); return 1;
    default:
      break;
    }
  }

  Path path = build(ip, addr);

  unsigned v;
  IScalVal::create(path->findByName("mmio/reg"))->getVal(&v,1);
  printf("[%08x] %08x\n",addr,v);
  
  return 0;
}
