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
#include <cpsw_proto_mod_depack.h>
#include <TPG.hh>
#include <RamControl.hh>
#include <AxiStreamDmaRingWrite.hh>

class AsyncMsg {
public:
  uint32_t d[3];
public:
  uint32_t irqStatus() const { return d[0]; }
  uint64_t bsaComplete() const { uint64_t v=d[2]; v<<=32; v|=d[1]; return v; }
};

using namespace TPGen;

static Path build(const char* ip)
{
  //
  //  Build
  //
  NetIODev  root = INetIODev::create("fpga", ip);

  {  //  Register access
#ifndef FRAMEWORK_R3_4
    INetIODev::PortBuilder bldr = INetIODev::createPortBuilder();
    bldr->setSRPVersion              ( INetIODev::SRP_UDP_V3 );
#else
    ProtoStackBuilder bldr = IProtoStackBuilder::create();
    bldr->setSRPVersion              ( IProtoStackBuilder::SRP_UDP_V3 );
#endif
    bldr->setUdpPort                 (                  8193 );
    bldr->setSRPTimeoutUS            (                 10000 );
    bldr->setSRPRetryCount           (                     5 );
    bldr->setSRPMuxVirtualChannel    (                     0 );
    bldr->useDepack                  (                  true );
    bldr->useRssi                    (                  true );
    bldr->setTDestMuxTDEST           (                     0 );

    MMIODev   mmio = IMMIODev::create ("mmio", (1ULL<<32));
    {
      //  Build RamControl
      RamControl bsa = IRamControl::create("bsa",64);
      mmio->addAtAddress( bsa, 0x09000000);

      AxiStreamDmaRingWrite raw = IAxiStreamDmaRingWrite::create("raw",2);
      mmio->addAtAddress( raw, 0x09010000);

      //  Build TPG
      ::TPG tpg = ITPG::create("tpg");
      mmio->addAtAddress( tpg, 0x80000000);
    }
    root->addAtAddress( mmio, bldr);
  }

  {  // Streaming access
#ifndef FRAMEWORK_R3_4
    INetIODev::PortBuilder bldr = INetIODev::createPortBuilder();
    bldr->setSRPVersion              ( INetIODev::SRP_UDP_V3 );
#else
    ProtoStackBuilder bldr = IProtoStackBuilder::create();
    bldr->setSRPVersion              ( IProtoStackBuilder::SRP_UDP_V3 );
#endif
    bldr->setUdpPort                 (                  8194 );
    bldr->setSRPTimeoutUS            (                 90000 );
    bldr->setSRPRetryCount           (                     5 );
    bldr->setSRPMuxVirtualChannel    (                     0 );
    bldr->useDepack                  (                  true );
    bldr->useRssi                    (                  true );
    bldr->setTDestMuxTDEST           (                     4 );

    MMIODev   mmio = IMMIODev::create ("strm", (1ULL<<31));
    {
      //  Build DRAM
      Field      f = IIntField::create("dram", 8, false, 0);
      mmio->addAtAddress( f, 0, (1<<30));  // this limits to 2GB
    }
    root->addAtAddress( mmio, bldr);
  }

  {  // Asynchronous messaging
#ifndef FRAMEWORK_R3_4
    INetIODev::PortBuilder bldr = INetIODev::createPortBuilder();
    bldr->setSRPVersion              ( INetIODev::SRP_UDP_NONE );
#else
    ProtoStackBuilder bldr = IProtoStackBuilder::create();
    bldr->setSRPVersion              ( IProtoStackBuilder::SRP_UDP_NONE );
#endif
    bldr->setUdpPort             (                    8193 );
    bldr->setUdpOutQueueDepth    (                      40 );
    bldr->setUdpNumRxThreads     (                       4 );
    bldr->setDepackOutQueueDepth (                       1 );
    bldr->setDepackLdFrameWinSize(                       5 );
    bldr->setDepackLdFragWinSize (                       5 );
    bldr->useRssi                (                       0 );
    bldr->setTDestMuxTDEST       (                    0xc0 );

    Field    irq = IField::create("irq");
    root->addAtAddress( irq, bldr );
  }
    
  return IPath::create( root );
}

static void usage(const char* p)
{
  printf("Usage: %s [options]\n"
         "  -a <ip>\n",p);
}

int main(int argc, char* argv[])
{
  const char* ip = "192.168.2.10";

  opterr = 0;

  char opts[32];
  sprintf(opts,"a:h");

  int c;
  while( (c=getopt(argc,argv,opts))!=-1 ) {
    switch(c) {
    case 'a':
      ip = optarg;
      break;
    case 'h':
      usage(argv[0]); return 1;
    default:
      break;
    }
  }

  Path path = build(ip);

#define SET_REG(name,v) IScalVal::create(path->findByName("mmio/tpg/"#name))->setVal(&v,1)
#define GET_REG(name,v) IScalVal_RO::create(path->findByName("mmio/tpg/"#name))->getVal(&v,1)

  { unsigned v=0;     SET_REG(IrqControl ,v); }
  { unsigned v=2;     SET_REG(IrqStatus  ,v); }

  timespec tvo;
  clock_gettime(CLOCK_REALTIME,&tvo);

  while(1) {
    unsigned v; GET_REG(IrqStatus,v);

    if (v&0x2) {
      timespec tv;
      clock_gettime(CLOCK_REALTIME,&tv);
      
      double dt = double(tv.tv_sec-tvo.tv_sec)+1.e-9*(double(tv.tv_nsec)-double(tvo.tv_nsec));
      tvo=tv;
      printf("%04x  %f\n",v,dt);
    }
    else {
      usleep(10000);
    }
    { unsigned v=2;     SET_REG(IrqStatus  ,v); }
  }
}
