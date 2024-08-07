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
#include <string>

#include "tpg_cpsw.hh"
#include "event_selection.hh"
#include "Processor.hh"

#include <cpsw_api_builder.h>
#include <TPG.hh>
#include <RamControl.hh>
#include <Sy56040.hh>
#include <AxiStreamDmaRingWrite.hh>
#include <AmcCarrier.hh>
#include <cpsw_proto_mod_depack.h>

using namespace TPGen;

static Path _build(const char* ip)
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
    bldr->setSRPTimeoutUS            (                 90000 );
    bldr->setSRPRetryCount           (                     5 );
    bldr->setSRPMuxVirtualChannel    (                     0 );
    bldr->useDepack                  (                  true );
    bldr->useRssi                    (                  true );
    bldr->setTDestMuxTDEST           (                     0 );

    MMIODev   mmio = IMMIODev::create ("mmio", (1ULL<<32));
    {
      //  Timing Crossbar
      Sy56040 xbar = ISy56040::create("xbar");
      mmio->addAtAddress( xbar, 0x03000000);

#if 1
      Bsa::AmcCarrier::build(mmio);
#else
      //  Build RamControl
      RamControl bsa = IRamControl::create("bsa",64);
      mmio->addAtAddress( bsa, 0x09000000);

      AxiStreamDmaRingWrite raw = IAxiStreamDmaRingWrite::create("raw",2);
      mmio->addAtAddress( raw, 0x09010000);
#endif
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

    MMIODev   mmio = IMMIODev::create ("strm", (1ULL<<33));
    {
      //  Build DRAM
      Field      f = IIntField::create("dram", 64, false, 0);
      mmio->addAtAddress( f, 0, (1<<30));
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
    bldr->setUdpNumRxThreads     (                       1 );
    bldr->setDepackOutQueueDepth (                       5 );
    bldr->setDepackLdFrameWinSize(                       5 );
    bldr->setDepackLdFragWinSize (                       5 );
    bldr->useRssi                (                       0 );
    bldr->setTDestMuxTDEST       (                    0xc0 );

    Field    irq = IField::create("irq");
    root->addAtAddress( irq, bldr );
  }
    
  return IPath::create( root );
}

class TextPv : public Bsa::Pv {
public:
  TextPv(const char* name) : _name(name), _n(0), _mean(0), _rms2(0) {}
  void clear() { _n.clear(); _mean.clear(); _rms2.clear(); printf("clear %s\n", _name.c_str()); }
  void setTimestamp(uint32_t,uint32_t) {}
  void append() {}
  void append(unsigned n,
              double   mean,
              double   rms2)
  {
    _n   .push_back(double(n)); 
    _mean.push_back(mean); 
    _rms2.push_back(rms2); 
  }
  void flush() {
    printf("-- %20.20s [%zu] --\n", _name.c_str(), _n.size());
    unsigned n = _n.size() < 50 ? _n.size() : 50;
    for(unsigned i=0; i<n; i++)
      printf("%f:%f:%f\n", _n[i], _mean[i], _rms2[i]);
  }
private:
  std::string _name;
  std::vector<double> _n;
  std::vector<double> _mean;
  std::vector<double> _rms2;
};

class TextPvArray : public Bsa::PvArray {
public:
  TextPvArray(unsigned                array,
              const std::vector<Bsa::Pv*>& pvs) :
    _array(array),
    _pvs  (pvs)
  {
  }
public:
  unsigned array() const { return _array; }
  void     reset(unsigned sec,
                 unsigned nsec) {
    _pid.clear();
    _ts_sec = sec;
    _ts_nsec = nsec;
    for(unsigned i=0; i<_pvs.size(); i++) {
      _pvs[i]->clear();
      _pvs[i]->setTimestamp(sec,nsec);
    }
  }
  void     append(uint64_t pulseId) { _pid.push_back(pulseId); }
  std::vector<Bsa::Pv*> pvs() { return std::vector<Bsa::Pv*>(_pvs); }
  void flush() {
    unsigned n = _pid.size() < 50 ? _pid.size() : 50;
    printf("-- %20.20s [%u.%09u] [%zu] --\n", "PulseID", _ts_sec, _ts_nsec, _pid.size());
    for(unsigned i=0; i<n; i++)
      printf("%016lx\n", _pid[i]);
  }
private:
  unsigned _array;
  unsigned _ts_sec;
  unsigned _ts_nsec;
  std::vector<uint64_t> _pid;
  const std::vector<Bsa::Pv*>& _pvs;
};

static void usage(const char* p)
{
  printf("Usage: %s [options]\n"
         "  -a <ip> -g (generate)\n",p);
}

int main(int argc, char* argv[])
{
  const char* ip = "192.168.2.10";

  opterr = 0;

  char opts[32];
  sprintf(opts,"a:gh");
  bool lgen = false;

  int c;
  while( (c=getopt(argc,argv,opts))!=-1 ) {
    switch(c) {
    case 'a':
      ip = optarg;
      break;
    case 'g':
      lgen = true;
      break;
    case 'h':
      usage(argv[0]); return 1;
    default:
      break;
    }
  }

  Bsa::Processor* p;
  unsigned array=0;

  if (lgen) {
    TPGen::TPG* g = new TPGen::TPGCpsw(ip);
    p = Bsa::Processor::create();
    g->startBSA(array,1,8,new TPGen::FixedRateSelect(0));
  }
  else {
    Bsa::AmcCarrier::instance(_build(ip));
  }

  //  We only need this vector for the "dump" function call
  std::vector<TextPv*> pvs;
  pvs.push_back(new TextPv("field 0"));
  pvs.push_back(new TextPv("field 1"));

  std::vector<Bsa::Pv*> cpvs;
  for(unsigned i=0; i<pvs.size(); i++)
    cpvs.push_back(pvs[i]);
  
  TextPvArray pva(array, cpvs);

  unsigned uinterval = 1000000;
  //
  //  We simulate periodic update requests here
  //
  while(1) {
    if (p->update(pva)) {
      for(unsigned i=0; i<pvs.size(); i++)
        pvs[i]->flush();
    }
    usleep(uinterval);
  }

  return 1;
}
