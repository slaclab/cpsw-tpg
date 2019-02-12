#include "tpg_cpsw.hh"
#include "sequence_engine_cpsw.hh"
#include "event_selection.hh"
//#include "device_cpsw.hh"
//#include "frame_cpsw.hh"

#include <cpsw_api_builder.h>
#include <TPG.hh>
#include <RamControl.hh>
#include <RssiCore.hh>
#include <Sy56040.hh>
#include <AxiStreamDmaRingWrite.hh>
#include <AmcCarrier.hh>
#include <cpsw_proto_mod_depack.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <pthread.h>

#include <map>
#include <vector>

static const unsigned SEQ_SIZE   = 0x800;
static const unsigned FRAME_SIZE = 848/8;
static const unsigned MAXACQBSA  = (1<<16)-1;
static const unsigned MAXAVGBSA  = (1<<13)-1;
static const unsigned MAX_AC_DELAY    = (1<<16)-1;
static const unsigned NRATECOUNTERS = 24;

enum { IRQ_CHECKPOINT=0,
       IRQ_INTERVAL  =1,
       IRQ_BSA       =2,
       IRQ_FAULT     =3};

static uint64_t GET_U1(Path pre)
{
  uint64_t r=0;
  ScalVal_RO s = IScalVal_RO::create( pre );
  for(unsigned i=0; i<2; i++) {
    IndexRange rng(i);
    unsigned v;
    s->getVal(&v,1,&rng);
    if (v)
      r |= 1ULL<<i;
  }
  return r;
}

static unsigned _GET_U8(const char* name, Path path, unsigned index) 
{
  IndexRange rng(index);
  unsigned v;
  IScalVal_RO::create(path->findByName(name))->getVal(&v,1,&rng);
  return v;
}

static unsigned _GET_U32(const char* name, Path path) 
{
  unsigned v;
  IScalVal_RO::create(path->findByName(name))->getVal(&v,1);
  return v;
}

static unsigned _GET_U32(const char* name, Path path, unsigned index) 
{
  IndexRange rng(index);
  unsigned v;
  IScalVal_RO::create(path->findByName(name))->getVal(&v,1,&rng);
  return v;
}

static uint64_t _GET_U64(const char* name, Path path) 
{
  uint64_t v;
  IScalVal_RO::create(path->findByName(name))->getVal(&v,1);
  return v;
}

static void _SET_U32(const char* name, Path path, unsigned r) 
{
  unsigned v(r);
  IScalVal::create(path->findByName(name))->setVal(&v,1);
}

#define GET_U8I(name,i) _GET_U8("mmio/tpg/"#name,_private->path,i)
#define GET_U32(name)   _GET_U32("mmio/tpg/"#name,_private->path)
#define GET_U32I(name,i) _GET_U32("mmio/tpg/"#name,_private->path,i)
#define GET_U64(name)   _GET_U64("mmio/tpg/"#name,_private->path)
#define SET_U32(name,v) _SET_U32("mmio/tpg/"#name,_private->path,v)
#define SET_REG(name,v) IScalVal::create(_private->path->findByName("mmio/tpg/"#name))->setVal(&v,1)

namespace TPGen {

  static void* poll_irq(void* arg)
  {
    TPGCpsw* p = reinterpret_cast<TPGCpsw*>(arg);
    p->handleIrq();
    return 0;
  }

  static void remove(std::map<unsigned,Callback*>& map,
		     Callback* cb)
  {
    while(1) {
      for(std::map<unsigned,Callback*>::iterator it=map.begin();
	  it!=map.end(); it++)
	if (it->second==cb) {
	  map.erase(it);
	  continue;
	}
      break;
    }
  }

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

        //  RSSI
        RssiCore rssi = IRssiCore::create("rssi");
        mmio->addAtAddress( rssi, 0x0A020000);

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
      bldr->setSRPVersion              ( INetIODev::SRP_UDP_V3 );
#else
      ProtoStackBuilder bldr = IProtoStackBuilder::create();
      bldr->setSRPVersion              ( IProtoStackBuilder::SRP_UDP_V3 );
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

  class TPGCpsw::PrivateData {
  public:
    PrivateData() : intervalCallback(0), faultCallback(0) {}
    ~PrivateData() {}
  public:
    Path                             path;
    ScalVal_RO                       bsaComplete;
    std::vector<SequenceEngineCpsw*> sequences;
    std::map<unsigned,Callback*>     bsaCallback;
    Callback*                        intervalCallback;
    Callback*                        faultCallback;
  };

  TPGCpsw::TPGCpsw(const char* ip) :
    _private(new PrivateData)
  {
    _private->path        = build(ip);
    _private->bsaComplete = IScalVal_RO::create(_private->path->findByName("mmio/tpg/BsaComplete"));

    const unsigned NBEAMSEQ   = nAllowEngines()+nBeamEngines();
    const unsigned NSEQUENCES = nAllowEngines()+nBeamEngines()+nExptEngines();
    const unsigned SEQADDRW   = seqAddrWidth();
    char buff[256];
    ScalVal seqRestart = IScalVal::create(_private->path->findByName("mmio/tpg/SeqRestart"));
    _private->sequences.reserve(NSEQUENCES);
    for(unsigned i=0; i<NSEQUENCES; i++) {
      sprintf(buff,"mmio/tpg/seqmem[%u]",i);
      Path memPath = _private->path->findByName(buff);
      sprintf(buff,"mmio/tpg/seqjump[%u]",i);
      Path jumpPath = _private->path->findByName(buff);
      _private->sequences[i] =
	new SequenceEngineCpsw(memPath,
                               seqRestart,
                               jumpPath,
                               i,
                               i<NBEAMSEQ ?
                               ControlRequest::Beam : ControlRequest::Expt,
                               SEQADDRW);
    }
    //  Start the asynchronous notification thread
    //  Shut it down (gracefully?) when the application exits
    //  pthread_t      thread_id;
    //  pthread_create(&thread_id, 0, poll_irq, (void*)this);
    //
    //  09-30-2017, Kukhee Kim, the polling thread will be created by epics with RT priority

    Bsa::AmcCarrier::instance(_private->path);

    initializeRam();
  }

  TPGCpsw::~TPGCpsw() 
  {
    for(unsigned i=0; i<_private->sequences.size(); i++)
      delete _private->sequences[i];
  }

  unsigned TPGCpsw::fwVersion() const
  { return 0; }

  unsigned TPGCpsw::nBeamEngines () const
  { return GET_U32(NBeamSeq); }

  unsigned TPGCpsw::nAllowEngines () const
  { return GET_U32(NAllowSeq); }

  unsigned TPGCpsw::nExptEngines () const
  { return GET_U32(NControlSeq); }

  unsigned TPGCpsw::nArraysBSA   () const
  { return GET_U32(NArraysBsa); }

  unsigned TPGCpsw::seqAddrWidth () const
  { return GET_U32(SeqAddrLen); }

  unsigned TPGCpsw::fifoAddrWidth() const
  { return 0; }

  unsigned TPGCpsw::nRateCounters() const
  { return NRATECOUNTERS; }

  void TPGCpsw::setClockStep(unsigned ns,
                             unsigned frac_num,
                             unsigned frac_den)
  {
    if (ns<32 && frac_den<256 && frac_num<frac_den) {
      unsigned v = (ns<<16) | (frac_num<<8) | frac_den;
      SET_U32(ClockPeriod,v);
    }
  }

  int TPGCpsw::setBaseDivisor(unsigned v)
  { 
    if (v > FRAME_SIZE/sizeof(uint16_t)+4) {
        SET_U32(BaseControl,v);
        return 0;
    }

    return -1;
  }

  int TPGCpsw::setACDelay(unsigned v)
  { 
    if (v > MAX_AC_DELAY)
      return -1;
    SET_REG(ACDelay,v);
    return 0; 
  }

  int TPGCpsw::setFrameDelay(unsigned v)
  { 
    unsigned u = GET_U32(BaseControl);
    if (v < u) {
      SET_REG(FrameDelay,v);
      return 0; 
    }
    return -1;
  }

  void TPGCpsw::setPulseID(uint64_t v)
  { SET_REG(PulseId,v); }

  void TPGCpsw::setTimestamp(unsigned sec, unsigned nsec)
  { uint64_t v=sec; v<<=32; v+=nsec; SET_REG(TStamp,v); }

  uint64_t TPGCpsw::getPulseID() const
  { return GET_U64(PulseId); }

  void     TPGCpsw::getTimestamp(unsigned& sec, 
                                 unsigned& nsec) const
  { uint64_t v = GET_U64(TStamp);
    sec  = v>>32;
    nsec = v&0xffffffff; }

  void TPGCpsw::setACDivisors(const std::vector<unsigned>& d)
  {
    std::vector<uint8_t> v(d.size());
    for(unsigned i=0; i<d.size(); i++)
      v[i] = d[i]&0xff;
    IScalVal::create(_private->path->findByName("mmio/tpg/ACRateDiv"))->setVal(v.data(),v.size());
  }

  void TPGCpsw::setFixedDivisors(const std::vector<unsigned>& d)
  {
    IScalVal::create(_private->path->findByName("mmio/tpg/FixedRateDiv"))->setVal(const_cast<uint32_t*>(d.data()),d.size());
  }

  void TPGCpsw::loadDivisors()
  { SET_U32(RateReload,1); }

  void TPGCpsw::initializeRam()
  {
#if 1
    Bsa::AmcCarrier::instance()->initialize();
    Bsa::AmcCarrier::instance()->initRaw   (0, 1<<24, true);
#else
    unsigned n = 64;
    uint64_t start=0,end=0;
    unsigned zero(0),one(1);
    for(unsigned i=0; i<n; i++) {
      end += (i<60 ? 1ULL<<24 : 1ULL<<28);
      IndexRange rng(i);
      IScalVal::create(_private->path->findByName("mmio/bsa/ring/StartAddr"))->setVal(&start,1,&rng);
      IScalVal::create(_private->path->findByName("mmio/bsa/ring/EndAddr"  ))->setVal(&end  ,1,&rng);
      IScalVal::create(_private->path->findByName("mmio/bsa/ring/Enabled"  ))->setVal(&one  ,1,&rng);
      IScalVal::create(_private->path->findByName("mmio/bsa/ring/Mode"     ))->setVal(&zero ,1,&rng);
      IScalVal::create(_private->path->findByName("mmio/bsa/ring/Init"     ))->setVal(&one  ,1,&rng);
      IScalVal::create(_private->path->findByName("mmio/bsa/ring/Init"     ))->setVal(&zero ,1,&rng);
      start = end;
    }
    for(unsigned i=0; i<2; i++) {
      end += 1ULL<<23;
      IndexRange rng(i);
      printf("start,end [%lx,%lx]\n",start,end);
      IScalVal::create(_private->path->findByName("mmio/raw/StartAddr"))->setVal(&start,1,&rng);
      IScalVal::create(_private->path->findByName("mmio/raw/EndAddr"  ))->setVal(&end  ,1,&rng);
      IScalVal::create(_private->path->findByName("mmio/raw/Enabled"  ))->setVal(&one  ,1,&rng);
      IScalVal::create(_private->path->findByName("mmio/raw/Mode"     ))->setVal(&zero ,1,&rng);
      IScalVal::create(_private->path->findByName("mmio/raw/Init"     ))->setVal(&one  ,1,&rng);
      IScalVal::create(_private->path->findByName("mmio/raw/Init"     ))->setVal(&zero ,1,&rng);
      start = end;
    }
#endif
  }

  void TPGCpsw::acquireHistoryBuffers(bool v)
  { SET_U32(BeamDiagControl,1<<31); }

  void TPGCpsw::clearHistoryBuffers(unsigned v)
  { SET_U32(BeamDiagControl,1<<v); }

  std::vector<FaultStatus> TPGCpsw::getHistoryStatus()
  { std::vector<FaultStatus> vec(4);
    for(unsigned i=0; i<4; i++) {
      unsigned v = GET_U32I(BeamDiagStatus,i);
      vec[i].manualLatch = v&(1<<31);
      vec[i].bcsLatch    = v&(1<<30);
      vec[i].mpsLatch    = v&(1<<29);
      vec[i].tag         = (v>>16)&0xfff;
      vec[i].mpsTag      = v&0xffff;
    }
    return vec; }

  void TPGCpsw::setEnergy(const std::vector<unsigned>& energy) {
    IScalVal::create(_private->path->findByName("mmio/tpg/BeamEnergy"))->setVal(const_cast<unsigned*>(energy.data()),4);
  }
  void TPGCpsw::setWavelength(const std::vector<unsigned>& wavelen) {
    IScalVal::create(_private->path->findByName("mmio/tpg/PhotonWavelen"))->setVal(const_cast<unsigned*>(wavelen.data()),2);
  }

  void TPGCpsw::dump() const {
#define printr(reg) printf("%15.15s: %08x\n",#reg,GET_U32(reg))
#define printrn(reg,n)                                           \
    printf("%15.15s:",#reg);                                     \
      for(unsigned i=0; i<n; i++) {                              \
        IndexRange rng(i);                                       \
        printf(" %08x",GET_U32I(reg,i));                         \
        if ((i%10)==9) printf("\n                ");             \
      }                                                          \
      printf("\n")
#define printr64(reg) printf("%15.15s: %016lx\n",#reg,GET_U64(reg))
    char buff[256];
    //    printr("FwVersion       : %08x\n",_private->device->fwVersion);
    printr  (NBeamSeq);
    printr  (NControlSeq);
    printr  (NArraysBsa);
    printr  (SeqAddrLen);
    printr  (BaseControl);
    printr64(PulseId);
    printr64(TStamp);
    printrn (SeqReqd,16);
    printrn (SeqDestn,16);
    printrn (ACRateDiv,6);
    printrn (FixedRateDiv,10);
    printr  (DiagSeq);
    printr  (IrqControl);
    printr  (IrqStatus);
    printrn (Energy,4);
    { printf("%15.15s:","MpsState(Latch)");
      for(unsigned i=0; i<16; i++) {
        unsigned s,v;
        const_cast<TPGen::TPGCpsw*>(this)->getMpsState(i,s,v);
        printf(" %02x(%02x)",s,v);
        if ((i%10)==9) printf("\n                ");
      }
      printf("\n"); }
    printrn (BeamDiagStatus,4);
    printr  (SeqRestart);
#if 0
    printf("L1Trig          :");
    for(unsigned i=0; i<7; i++)
      printf(" %08x:%08x",
	     unsigned(_private->device->intTrig[i].evcode),
	     unsigned(_private->device->intTrig[i].delay));
    printf("\n");
#endif
    printr64(BsaComplete);
    printrn (BsaEventSel,1);
    printrn (BsaStatSel ,1);
    printr  (PllCnt);
    printr  (ClkCnt);
    printr  (SyncErrCnt);
    printr  (CountInterval);
    printr  (BaseRateCount);
    printrn (CountTrig,12);
    printrn (CountSeq,28);
    printf("%15.15s:","SeqState");
    for(unsigned i=0; i<28; i++) {
      sprintf(buff,"mmio/tpg/seqstate[%u]/Addr",i);
      printf(" %08x",_GET_U32(buff,_private->path));
      unsigned cc[4];
      sprintf(buff,"mmio/tpg/seqstate[%u]/CondC",i);
      IScalVal_RO::create(_private->path->findByName(buff))->getVal(cc,4);
      printf(" :%u:%u:%u:%u",cc[0],cc[1],cc[2],cc[3]);
      if ((i%5)==4)
	printf("\n                ");
    }
    printf("\n");
    for(unsigned i=0; i<16; i++) {
      printf("%11.11s[%02d]:","SeqJump",i);
      unsigned a[16];
      sprintf(buff,"mmio/tpg/seqjump[%u]/StartAddr",i);
      IScalVal_RO::create(_private->path->findByName(buff))->getVal(a,16);
      for(unsigned j=0; j<16; j++)
	printf(" %04x", a[j]);
      printf("\n");
    }

#if 1
    Bsa::AmcCarrier::instance()->dump();
#else
    //  dump the raw diagnostic summary
    //  unsigned NArrays = nArrays();
    unsigned NArrays = 2;
    //
    //  Print BSA buffer status summary
    //
    uint64_t done, full, empty;
    done  = GET_U1(_private->path->findByName("mmio/raw/Done"));
    full  = GET_U1(_private->path->findByName("mmio/raw/Full"));
    empty = GET_U1(_private->path->findByName("mmio/raw/Empty"));
    printf("BufferDone [%016llx]\t  Full[%016llx]\t  Empty[%016llx]\n",
           (unsigned long long)done,(unsigned long long)full,(unsigned long long)empty);
    printf("%4.4s ","Buff");
    printf("%9.9s ","Start");
    printf("%9.9s ","End");
    printf("%9.9s ","Write");
    printf("%9.9s ","Trigger");
    printf("%4.4s ","Done");
    printf("%4.4s ","Full");
    printf("%4.4s ","Empt");
    printf("\n");

#define PR36(name) {                                                    \
      ScalVal_RO s = IScalVal_RO::create( _private->path->findByName("mmio/raw/"#name) ); \
      unsigned v;                                                       \
      s->getVal(&v,1,&rng);                                             \
      printf("%09llx ",(unsigned long long)(v));                        \
    }

    for(unsigned i=0; i<NArrays; i++) {
      IndexRange rng(i);
      printf("%4.4x ",i);
      PR36(StartAddr);
      PR36(EndAddr);
      PR36(WrAddr);
      PR36(TriggerAddr);
      printf("%4.4s ", (done &(1ULL<<i)) ? "X": "-");
      printf("%4.4s ", (full &(1ULL<<i)) ? "X": "-");
      printf("%4.4s ", (empty&(1ULL<<i)) ? "X": "-");
      printf("\n");
    }
#endif

#define PRINT_U1(name) {                                                    \
      ScalVal_RO s = IScalVal_RO::create( _private->path->findByName("mmio/rssi/"#name) ); \
      unsigned v;                                                       \
      s->getVal(&v,1);                                                  \
      printf("%16.16s : %d\n",#name, v ? 1:0);                            \
    }
      
#define PRINT_UN(name) {                                                    \
      ScalVal_RO s = IScalVal_RO::create( _private->path->findByName("mmio/rssi/"#name) ); \
      unsigned v;                                                       \
      s->getVal(&v,1);                                                  \
      printf("%16.16s : 0x%x\n",#name, v);                                \
    }
      

    PRINT_U1(OpenConn        );
    PRINT_U1(CloseConn       );
    PRINT_U1(Mode            );
    PRINT_U1(HeaderChkSumEn  );
    PRINT_U1(InjectFault     );

    PRINT_UN(InitSeqN        );
    PRINT_UN(Version         );
    PRINT_UN(MaxOutsSet      );
    PRINT_UN(MaxSegSize      );
    PRINT_UN(RetransTimeout  );
    PRINT_UN(CumAckTimeout   );
    PRINT_UN(NullSegTimeout  );
    PRINT_UN(MaxNumRetrans   );
    PRINT_UN(MaxCumAck       );
    PRINT_UN(MaxOutOfSeq     );
    PRINT_U1(ConnectionActive);
    PRINT_U1(ErrMaxRetrans   );
    PRINT_U1(ErrNullTout     );
    PRINT_U1(ErrAck          );
    PRINT_U1(ErrSsiFrameLen  );
    PRINT_U1(ErrConnTout     );
    PRINT_U1(ParamRejected   );
    PRINT_UN(ValidCnt        );
    PRINT_UN(DropCnt         );
    PRINT_UN(RetransmitCnt   );
    PRINT_UN(ReconnectCnt    );
  }

  void TPGCpsw::dump_rcvr(unsigned n) {
    /*
      Device* p = this->_private->device;
      printf("RcvrStat        : %08x\n",p->rcvrStat);
      printf("RcvrClkCnt      : %08x\n",p->rcvrClkCnt);
      printf("RcvrDecCnt      : %08x\n",p->rcvrDecCnt);
      printf("RcvrDspCnt      : %08x\n",p->rcvrDspCnt);
      printf("RcvrSofCnt      : %08x\n",p->rcvrSofCnt);
      printf("RcvrEofCnt      : %08x\n",p->rcvrEofCnt);
      bool lheader=false;
      while(n-- && (p->rcvrFifoCntl&0x8)==0) {
      if (!lheader) {
      lheader=true;
      printf("%4.4s %19.19s %19.19s %19.19s %4.4s %4.4s %9.9s %4.4s %14.14s %24.24s %19.19s\n",
      "SOF","---Vsn","---Pulse Id","---Time Stamp","Rate","TS","BeamReq","Sta","---MPS","---Diag","---BSA[0:15]");
      }
      volatile uint32_t* d = &p->rcvrFifoData;
      for(unsigned i=0; i<13; i++) {
      uint32_t w = *d;
      printf("%04x %04x ",w&0xffff,(w>>16)&0xffff);
      }
      for(unsigned i=0; i<4; i++) {
      volatile uint32_t w = *d;
      w = *d;
      w = *d;
      w = *d;
      printf("%04x ",(w>>16)&0xffff);
      }
      {
      volatile uint32_t w = *d;
      w = *d;
      w = *d;
      w = *d;
      }
      printf("\n");
      }
      p->rcvrFifoCntl=1;
    */
  }

#if 0
  void TPGCpsw::reset_fifo(unsigned b,
			      unsigned m)
  {}
#endif

  void TPGCpsw::force_sync() { SET_U32(GenStatus,1); }
 
  void TPGCpsw::reset_xbar()
  {
    unsigned v=1;  // from FPGA
    for(unsigned i=0; i<4; i++) {
      IndexRange rng(i);
      IScalVal::create(_private->path->findByName("mmio/xbar/Out"))->setVal(&v,1,&rng);
    }
  }

  void TPGCpsw::setSequenceRequired(unsigned iseq, unsigned requiredMask) 
  {
    //  Firmware wants index to start at beginning of beam sequence engines
    iseq -= nAllowEngines();
    IndexRange rng(iseq);
    IScalVal::create(_private->path->findByName("mmio/tpg/SeqReqd"))->setVal(&requiredMask,1,&rng);
  }

  void TPGCpsw::setSequenceDestination(unsigned iseq, TPGDestination destn) 
  {
    //  Firmware wants index to start at beginning of beam sequence engines
    iseq -= nAllowEngines();
    IndexRange rng(iseq);
    IScalVal::create(_private->path->findByName("mmio/tpg/SeqDestn"))->setVal(&destn,1,&rng);
  }

#if 0
  void TPGCpsw::setDestinationPriority(const std::list<TPGDestination>& l)
  {
    uint32_t v=0;
    for(std::list<TPGDestination>::const_iterator it=l.begin();
	it!=l.end(); it++)
      v = (v<<4) | ((*it)&0xf);
    _private->device->destnPriority = v;
  }
#endif
  SequenceEngine& TPGCpsw::engine(unsigned i)
  { return *_private->sequences[i]; }

  void TPGCpsw::resetSequences(const std::list<unsigned>& l)
  {
    uint64_t v=0;
    for(std::list<unsigned>::const_iterator it=l.begin();
	it!=l.end(); it++)
      v |= 1ULL<<(*it);
    SET_REG(SeqRestart,v);
  }

  unsigned TPGCpsw::getDiagnosticSequence() const  { return GET_U32(DiagSeq); }

  void     TPGCpsw::setDiagnosticSequence(unsigned v) { SET_U32(DiagSeq,v); }

  int  TPGCpsw::startBSA            (unsigned array,
                                     unsigned nToAverage,
                                     unsigned avgToAcquire,
                                     EventSelection* selection)
  { return startBSA(array,nToAverage,avgToAcquire,selection,0); }

  int  TPGCpsw::startBSA            (unsigned array,
                                     unsigned nToAverage,
                                     unsigned avgToAcquire,
                                     EventSelection* selection,
                                     unsigned maxSevr)
  {
    int result=0;

    if (array < nArraysBSA()) {
      if (nToAverage <= MAXAVGBSA) {
	if (avgToAcquire <= MAXACQBSA) {
          IndexRange rng(array);
          unsigned v = selection->word();
          IScalVal::create(_private->path->findByName("mmio/tpg/BsaEventSel"))->setVal(&v,1,&rng);
          v = (nToAverage&0x1fff) | 
            ((maxSevr&3)<<14) |
	    (avgToAcquire<<16);
          IScalVal::create(_private->path->findByName("mmio/tpg/BsaStatSel"))->setVal(&v,1,&rng);
	}
	else result = -3;
      }
      else result = -2;
    }
    else
      result = -1;

    delete selection;
    return result;
  }

  void TPGCpsw::stopBSA             (unsigned array)
  {
    IndexRange rng(array);
    unsigned v=0;
    IScalVal::create(_private->path->findByName("mmio/tpg/BsaEventSel"))->setVal(&v,1,&rng);
  }

  void TPGCpsw::queryBSA            (unsigned  array,
                                     unsigned& nToAverage,
                                     unsigned& avgToAcquire)
  {
    IndexRange rng(array);
    unsigned v;
    IScalVal::create(_private->path->findByName("mmio/tpg/BsaStat"))->getVal(&v,1,&rng);
    nToAverage   = v & 0xffff;
    avgToAcquire = v >> 16; 
  }

  uint64_t TPGCpsw::bsaComplete()
  {
    uint64_t v;
    _private->bsaComplete->getVal(&v,1);
    return v;
  }

  void TPGCpsw::bsaComplete(uint64_t ack)
  {
    SET_REG(BsaComplete, ack);
  }

  void TPGCpsw::setCountInterval(unsigned v) { SET_U32(CountInterval,v); }
  
  unsigned TPGCpsw::getPLLchanges   () const { return GET_U32(PllCnt); }
  unsigned TPGCpsw::get186Mticks    () const { return GET_U32(ClkCnt); }
  unsigned TPGCpsw::getSyncErrors   () const { return GET_U32(SyncErrCnt); }
  unsigned TPGCpsw::getCountInterval() const { return GET_U32(CountInterval); }
  unsigned TPGCpsw::getBaseRateTrigs() const { return GET_U32(BaseRateCount); }
  unsigned TPGCpsw::getInputTrigs(unsigned ch) const { return GET_U32I(CountTrig,ch); }
  unsigned TPGCpsw::getSeqRequests  (unsigned seq) const { return GET_U32I(CountSeq,seq); }
  unsigned TPGCpsw::getSeqRequests  (unsigned* array, unsigned array_size) const
  { unsigned n = array_size;
    const unsigned NSEQUENCES = nAllowEngines()+nBeamEngines()+nExptEngines();
    if (n > NSEQUENCES)
      n = NSEQUENCES;
    IScalVal_RO::create(_private->path->findByName("TPGStatus/CountSeq"))->getVal(array,n);
    return n;
  }

  void     TPGCpsw::lockCounters    (bool q) { SET_U32(CtrLock,(q?1:0)); }
  void     TPGCpsw::setCounter      (unsigned i, EventSelection* s)
  {
    IndexRange rng(i);
    unsigned v = s->word();
    IScalVal::create(_private->path->findByName("mmio/tpg/CtrDef"))->setVal(&v,1,&rng);
  }
  unsigned TPGCpsw::getCounter      (unsigned i) { return GET_U32I(CtrDef,i); }

  void     TPGCpsw::getMpsState     (unsigned  destination, 
                                     unsigned& latch, 
                                     unsigned& state) 
  {
    unsigned word = GET_U8I(MpsState, destination);
    latch = word&0xf;
    state = (word>>4)&0xf;
  }

  void     TPGCpsw::getMpsCommDiag   (unsigned& rxRdy,
                                       unsigned& txRdy,
                                       unsigned& locLnkRdy,
                                       unsigned& remLnkRdy,
                                       unsigned& rxClkFreq,
                                       unsigned& txClkFreq,
                                       unsigned& rxFrameErrorCount,
                                       unsigned& rxFrameCount)
  {
      Path _path = _private->path->findByName("mmio/AmcCarrierTimingGenerator/ApplicationCore/TPGMps/Pgp2bAxi/");
      IScalVal_RO::create(_path->findByName("PhyReadyRx"))->getVal(&rxRdy,1);
      IScalVal_RO::create(_path->findByName("PhyReadyTx"))->getVal(&txRdy,1);
      IScalVal_RO::create(_path->findByName("LocalLinkReady"))->getVal(&locLnkRdy,1);
      IScalVal_RO::create(_path->findByName("RemoteLinkReady"))->getVal(&remLnkRdy,1);
      IScalVal_RO::create(_path->findByName("RxClockFreq"))->getVal(&rxClkFreq,1);
      IScalVal_RO::create(_path->findByName("TxClockFreq"))->getVal(&txClkFreq,1);
      IScalVal_RO::create(_path->findByName("RxFrameErrorCount"))->getVal(&rxFrameErrorCount,1);
      IScalVal_RO::create(_path->findByName("RxFrameCount"))->getVal(&rxFrameCount,1);
  }


  void    TPGCpsw::getTimingFrameRxDiag(unsigned& txClkCount)
  {
      Path _path = _private->path->findByName("mmio/AmcCarrierTimingGenerator/AmcCarrierCore/AmcCarrierTiming/TimingFrameRx/");
      IScalVal_RO::create(_path->findByName("TxClkCount"))->getVal(&txClkCount, 1);
  }


  Callback* TPGCpsw::subscribeBSA     (unsigned bsaArray,
					  Callback* cb)
  { Callback* v = _private->bsaCallback[bsaArray];
    if (cb)
      _private->bsaCallback[bsaArray] = cb; 
    else
      _private->bsaCallback.erase(bsaArray);
    return v; }

  Callback* TPGCpsw::subscribeInterval(Callback* cb)
  { Callback* v = _private->intervalCallback;
    _private->intervalCallback = cb; 
    return v; }

  Callback* TPGCpsw::subscribeFault   (Callback* cb)
  { Callback* v = _private->faultCallback;
    _private->faultCallback = cb;
    return v; }

  void TPGCpsw::cancel           (Callback* cb)
  { // remove(_private->seqCallback,cb);
    remove(_private->bsaCallback,cb);
    if (_private->intervalCallback==cb) _private->intervalCallback=0;
    if (_private->faultCallback   ==cb) _private->faultCallback=0; }

#if 0
  void TPGCpsw::dumpFIFO(unsigned nfifo) const
  {
    FILE* fo = fopen("tpg.dump","w");

    bool lheader=false;
    Device* p = this->_private->device;
    
    unsigned i = nfifo;

    printf("---\n");
    while(i--)
      printf("f %08x\n",p->fifoData);
    printf("---\n");
    return;

    do {
      FrameCpsw f(&p->fifoData,
		     &p->fifoCntl);

      if ((p->fifoCntl&0x8)!=0)
	break;

      if (nfifo) {
	if (!lheader) {
	  lheader=true;
	  f.printHeader();
	}
	f.print();
      }
      if (fo) f.dump(fo);
    } while (--i);

    if (fo) fclose(fo);
  }
#endif
  void TPGCpsw::enableIrq    (unsigned bit, bool q)
  { unsigned v;
    v = GET_U32(IrqControl);
    if (q)
      v |= 1<<bit;
    else
      v &= ~(1<<bit);
    SET_U32(IrqControl,v);
  }

  void TPGCpsw::enableSequenceIrq (bool q) 
  { enableIrq(IRQ_CHECKPOINT,q); }

  void TPGCpsw::enableIntervalIrq (bool q) 
  { enableIrq(IRQ_INTERVAL,q); }

  void TPGCpsw::enableBsaIrq (bool q) 
  { enableIrq(IRQ_BSA,q); }

  void TPGCpsw::enableFaultIrq (bool q) 
  { enableIrq(IRQ_FAULT,q); }


  //  Read TPGCpsw to find outstanding async notifications and loop through them
  //  If this was the real implementation, we should worry about the
  //    race conditions when adding/removing callback functions.
  void TPGCpsw::handleIrq() {

    printf("handleIrq: entered\n");

    timespec tvo; clock_gettime(CLOCK_REALTIME,&tvo);
#if 0
    //  Async messages interfere with SRP!
    Stream           strm = IStream::create( _private->path->findByName("irq") );
    CTimeout         tmo(1000000);
    CAxisFrameHeader hdr;

    uint8_t buf[256];
    int v;
    while ((v=strm->read( buf, sizeof(buf), tmo, 0 ))>=0) {
      
      if (v>0) {
        printf("handleIrq: Received %d bytes\n",v);

        if(!hdr.parse(buf, sizeof(buf)))
          continue;

        unsigned irqStatus = *reinterpret_cast<uint32_t*>(&buf[hdr.getSize()]);
#else
    // Disable async messaging
    unsigned irqControl=0; SET_REG(IrqControl, irqControl);
    while(1) {
      unsigned irqStatus = GET_U32(IrqStatus);
      irqStatus &= ~(1<<IRQ_BSA); // 09-29-2017, Kukhee Kim, quick bandage to ignore BSA ireq
      if (irqStatus==0) {
        usleep(10000);
      }
      else {
#endif
        if (0) {
          timespec tv; clock_gettime(CLOCK_REALTIME,&tv);
          double dt = double(tv.tv_sec-tvo.tv_sec)+1.e-9*(double(tv.tv_nsec)-double(tvo.tv_nsec));
          tvo = tv; 
          printf("received irqStatus %x  %f\n",irqStatus,dt); 
        }

        if ((irqStatus&(1<<IRQ_INTERVAL)) && _private->intervalCallback)
          _private->intervalCallback->routine();
        
        if ((irqStatus&(1<<IRQ_BSA))) {
          uint64_t cmpl = GET_U64(BsaComplete);
          SET_REG(BsaComplete, cmpl); // clear handled bits
          if (_private->bsaCallback.size()) {
            //uint64_t cmpl = *reinterpret_cast<uint64_t*>(&buf[hdr.getSize()+4]);
            uint64_t q = cmpl;
            for(unsigned i=0; q!=0; i++)
              if (q & (1ULL<<i)) {
                std::map<unsigned,Callback*>::iterator it=_private->bsaCallback.find(i);
                if (it!=_private->bsaCallback.end())
                  it->second->routine();
                q &= ~(1ULL<<i);
              }
          }
        }

        if ((irqStatus&(1<<IRQ_FAULT)) && _private->faultCallback)
          _private->faultCallback->routine();
        
        while((irqStatus&(1<<IRQ_CHECKPOINT))) {
          unsigned w = GET_U32(SeqFifo);
          unsigned a = w&0xffff;
          _private->sequences[a>>11]->handle(a&0x7ff);
          irqStatus = GET_U32(IrqStatus);
        }
      }

      unsigned irqControl=(1<<IRQ_CHECKPOINT);
      if (_private->intervalCallback)
        irqControl |= (1<<IRQ_INTERVAL);
      if (_private->bsaCallback.size())
        irqControl |= (1<<IRQ_BSA);
      if (_private->faultCallback)
        irqControl |= (1<<IRQ_FAULT);

#if 0
      printf("Write IrqControl %x\n",irqControl);
      SET_REG(IrqControl, irqControl);
#else
#endif
      //  Acknowledge
      if ((irqStatus&((1<<IRQ_INTERVAL)|(1<<IRQ_FAULT))))
        SET_REG(IrqStatus, irqStatus);
    }
    perror("Exited TPGCpsw::handleIrq()\n");
  }

  void TPGCpsw::setL1Trig(unsigned index,
			     unsigned evcode,
			     unsigned delay)
#if 0
  {
    if (index<Device::NUML1TRIG) {
      _private->device->intTrig[index].evcode = evcode;
      _private->device->intTrig[index].delay  = delay;
    }
  }
#else
  {}
#endif


  void TPGCpsw::initADC()
  {
  }

};



