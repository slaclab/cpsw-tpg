#include "tpg_yaml.hh"
#include "sequence_engine_yaml.hh"
#include "event_selection.hh"

#include <TPG.hh>
#include <AmcCarrier.hh>

#include <cpsw_api_user.h>

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

static uint8_t _GET_U8(const char* name, Path path, unsigned index) 
{
  IndexRange rng(index);
  uint8_t v;
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

#define GET_U32(name)   _GET_U32("TPGControl/"#name,_private->tpg)
#define GET_U8I(name,i) _GET_U8("TPGControl/"#name,_private->tpg,i)
#define GET_U32I(name,i) _GET_U32("TPGControl/"#name,_private->tpg,i)
#define GET_U64(name)   _GET_U64("TPGControl/"#name,_private->tpg)
#define SET_U32(name,v) _SET_U32("TPGControl/"#name,_private->tpg,v)
#define SET_REG(name,v) IScalVal::create(_private->tpg->findByName("TPGControl/"#name))->setVal(&v,1)
#define SET_U32S(name,v) _SET_U32("TPGStatus/"#name,_private->tpg,v)
#define GET_U32S(name)   _GET_U32("TPGStatus/"#name,_private->tpg)
#define GET_U32SI(name,i) _GET_U32("TPGStatus/"#name,_private->tpg,i)



namespace TPGen {

  static void* poll_irq(void* arg)
  {
    TPGYaml* p = reinterpret_cast<TPGYaml*>(arg);
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

  class TPGYaml::PrivateData {
  public:
    PrivateData() : intervalCallback(0), faultCallback(0) {}
    ~PrivateData() {}
  public:
    Path                             root;
    Path                             tpg;
    ScalVal_RO                       bsaComplete;
    std::vector<SequenceEngineYaml*> sequences;
    std::map<unsigned,Callback*>     bsaCallback;
    Callback*                        intervalCallback;
    Callback*                        faultCallback;
  };

  TPGYaml::TPGYaml(Path root) :
    _private(new PrivateData)
  {
    _private->root        = root;
    _private->tpg         = root->findByName("mmio/AmcCarrierTimingGenerator/ApplicationCore/TPG");
    _private->bsaComplete = IScalVal_RO::create(_private->tpg->findByName("TPGControl/BsaComplete"));

    const unsigned NALLOWSEQ  = nAllowEngines();
    const unsigned NBEAMSEQ   = nAllowEngines()+nBeamEngines();
    const unsigned NSEQUENCES = nAllowEngines()+nBeamEngines()+nExptEngines();
    const unsigned SEQADDRW   = seqAddrWidth();
    char buff[256];
    ScalVal seqRestart = IScalVal::create(_private->tpg->findByName("TPGControl/SeqRestart"));
    _private->sequences.reserve(NSEQUENCES);
    for(unsigned i=0; i<NSEQUENCES; i++) {
      sprintf(buff,"TPGSeqMem/ICache[%u]",i);
      Path memPath = _private->tpg->findByName(buff);
      sprintf(buff,"TPGSeqJump");
      Path jumpPath = _private->tpg->findByName(buff);
      _private->sequences[i] =
	new SequenceEngineYaml(memPath,
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

    initializeRam();
  }

  TPGYaml::~TPGYaml() 
  {
    for(unsigned i=0; i<_private->sequences.size(); i++)
      delete _private->sequences[i];
  }

  unsigned TPGYaml::fwVersion() const
  { return 0; }

  unsigned TPGYaml::nBeamEngines () const
  { return GET_U32(NBeamSeq); }

  unsigned TPGYaml::nAllowEngines () const
  { return GET_U32(NAllowSeq); }

  unsigned TPGYaml::nExptEngines () const
  { return GET_U32(NControlSeq); }

  unsigned TPGYaml::nArraysBSA   () const
  { return GET_U32(NArraysBsa); }

  unsigned TPGYaml::seqAddrWidth () const
  { return GET_U32(SeqAddrLen); }

  unsigned TPGYaml::fifoAddrWidth() const
  { return 0; }

  unsigned TPGYaml::nRateCounters() const
  { return NRATECOUNTERS; }

  void TPGYaml::setClockStep(unsigned ns,
                             unsigned frac_num,
                             unsigned frac_den)
  {
    if (ns<32 && frac_den<256 && frac_num<frac_den) {
      SET_U32(ClockPeriodDiv, frac_den);
      SET_U32(ClockPeriodRem, frac_num);
      SET_U32(ClockPeriodInt, ns);
    }
  }

  int TPGYaml::setBaseDivisor(unsigned v)
  { 
    if (v > FRAME_SIZE/sizeof(uint16_t)+4)
      return -1;
    SET_U32(BaseControl,v);
    return 0;
  }

  int TPGYaml::setACDelay(unsigned v)
  { 
    if (v > MAX_AC_DELAY)
      return -1;
    SET_REG(ACDelay,v);
    return 0; 
  }

  int TPGYaml::setFrameDelay(unsigned v)
  { 
    unsigned u = GET_U32(BaseControl);
    if (v < u) {
      SET_REG(FrameDelay,v);
      return 0; 
    }
    return -1;
  }

  void TPGYaml::setPulseID(uint64_t v)
  { SET_REG(PulseId,v); }

  void TPGYaml::setTimestamp(unsigned sec, unsigned nsec)
  { uint64_t v=sec; v<<=32; v+=nsec; SET_REG(TStamp,v); }

  uint64_t TPGYaml::getPulseID() const
  { return GET_U64(PulseId); }

  void     TPGYaml::getTimestamp(unsigned& sec, 
                                 unsigned& nsec) const
  { uint64_t v = GET_U64(TStamp);
    sec  = v>>32;
    nsec = v&0xffffffff; }

  void TPGYaml::setACDivisors(const std::vector<unsigned>& d)
  {
    std::vector<uint8_t> v(d.size());
    for(unsigned i=0; i<d.size(); i++)
      v[i] = d[i]&0xff;
    IScalVal::create(_private->tpg->findByName("TPGControl/ACRateDiv"))->setVal(v.data(),v.size());
  }

  void TPGYaml::setFixedDivisors(const std::vector<unsigned>& d)
  {
    IScalVal::create(_private->tpg->findByName("TPGControl/FixedRateDiv"))->setVal(const_cast<uint32_t*>(d.data()),d.size());
  }

  void TPGYaml::loadDivisors()
  { SET_U32(RateReload,1); }

  void TPGYaml::initializeRam()
  {
    const uint64_t BlockMask = (0x1ULL<<12)-1;  // Buffers must be in blocks of 4kB
    const uint64_t bufferSize = (0x1ULL<<24);
    const bool     doneWhenFull = true;
    const int      index = 0;

    char buff[256];
    sprintf(buff,"mmio/AmcCarrierTimingGenerator/AmcCarrierCore/AmcCarrierBsa/BsaWaveformEngine[%d]/WaveformEngineBuffers", index/4);
    Path path = _private->root->findByName(buff);
    
    //  Setup the waveform memory
    //  Assume BSA is within first 4GB
    uint64_t p = (1ULL<<32);
    uint64_t pn = p+((bufferSize+BlockMask)&~BlockMask);
    uint32_t one(1), zero(0), mode(doneWhenFull ? 1:0);
    IndexRange rng(index%4);
    printf("Setup waveform memory %i %llx:%llx\n", index, p,pn);
    IScalVal::create( path->findByName("StartAddr"))->setVal(&p   ,1,&rng);
    IScalVal::create( path->findByName("EndAddr"  ))->setVal(&pn  ,1,&rng);
    IScalVal::create( path->findByName("Enabled"  ))->setVal(&one ,1,&rng);
    IScalVal::create( path->findByName("Mode"     ))->setVal(&mode,1,&rng);
    IScalVal::create( path->findByName("Init"     ))->setVal(&one ,1,&rng);
    IScalVal::create( path->findByName("Init"     ))->setVal(&zero,1,&rng);
  }

  void TPGYaml::acquireHistoryBuffers(bool v)
  { SET_U32(BeamDiagControl,1<<31); }

  void TPGYaml::clearHistoryBuffers(unsigned v)
  { SET_U32(BeamDiagControl,1<<v); }

  std::vector<FaultStatus> TPGYaml::getHistoryStatus()
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

  void TPGYaml::setEnergy(const std::vector<unsigned>& energy) {
    IScalVal::create(_private->tpg->findByName("TPGControl/BeamEnergy"))->setVal(const_cast<unsigned*>(energy.data()),4);
  }

  void TPGYaml::setWavelength(const std::vector<unsigned>& wavelen) {
    IScalVal::create(_private->tpg->findByName("TPGControl/PhotonWavelen"))->setVal(const_cast<unsigned*>(wavelen.data()),2);
  }

  void TPGYaml::dump() const {
#define printr(reg) printf("%15.15s: %08x\n",#reg,GET_U32(reg))
#define printrs(reg) printf("%15.15s: %08x\n",#reg,GET_U32S(reg))
#define printrn(reg,n)                                           \
    printf("%15.15s:",#reg);                                     \
      for(unsigned i=0; i<n; i++) {                              \
        IndexRange rng(i);                                       \
        printf(" %08x",GET_U32I(reg,i));                         \
        if ((i%10)==9) printf("\n                ");             \
      }                                                          \
      printf("\n")
#define printrsn(reg,n)                                          \
    printf("%15.15s:",#reg);                                     \
      for(unsigned i=0; i<n; i++) {                              \
        IndexRange rng(i);                                       \
        printf(" %08x",GET_U32SI(reg,i));                        \
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
    printrn (BeamSeqAllowMask,16);
    printrn (BeamSeqDestination,16);
    printrn (ACRateDiv,6);
    printrn (FixedRateDiv,10);
    printr  (DiagSeq);
    printr  (IrqControl);
    printr  (IrqStatus);
    printrn (BeamEnergy,4);
    { printf("%15.15s:","MpsLink");
      Path pgp_path = _private->root->findByName("mmio/AmcCarrierTimingGenerator/ApplicationCore/TPGMps/Pgp2bAxi/");
      printf(" RxRdy[%c]", _GET_U32("PhyReadyRx",pgp_path) ? 'T':'F');
      printf(" TxRdy[%c]", _GET_U32("PhyReadyTx",pgp_path) ? 'T':'F');
      printf(" LocRdy[%c]", _GET_U32("LocalLinkReady",pgp_path) ? 'T':'F');
      printf(" RemRdy[%c]", _GET_U32("RemoteLinkReady",pgp_path) ? 'T':'F');
      printf(" RxClkF[%u]", _GET_U32("RxClockFreq",pgp_path));
      printf(" TxClkF[%u]", _GET_U32("TxClockFreq",pgp_path));
      printf("\n"); }
    { printf("%15.15s:","MpsState(Latch)");
      IndexRange rng(0);
      for(unsigned i=0; i<16; i++,++rng) {
        unsigned s = GET_U8I(MpsState,i);
        unsigned v = s&0xf;
        s = (s>>4)&0xf;
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
    printrs (CountPLL);
    printrs (Count186M);
    printrs (CountSyncE);
    printrs (CountIntv);
    printrs (CountBRT);
    printrsn(CountTrig,12);
    printf("%15.15s:","ProgCount: ");
    for(unsigned i=0; i<8; i++)
      printf(" %09u", const_cast<TPGYaml*>(this)->getCounter(i));
    printf("\n");

    printf("-- Sequence Diagnostics:  CountSeq (# requests)  SeqState (instr# ctr0:ctr1:ctr2:ctr3)\n");
    printf("%15.15s\n","--AllowEngines");
    _dumpSeqState(0,nAllowEngines());
    printf("%15.15s\n","--BeamEngines");
    _dumpSeqState(nAllowEngines(),nBeamEngines());
    printf("%15.15s\n","--ExptEngines");
    _dumpSeqState(nAllowEngines()+nBeamEngines(),nExptEngines());
    
    printf("\n");
    { ScalVal_RO r = IScalVal::create(_private->tpg->findByName("TPGSeqJump/StartAddr"));
      IndexRange rng(0);
      for(unsigned i=0; i<16; i++) {
        printf("%11.11s[%02d]:","SeqJump",i);
        for(unsigned j=0; j<16; j++, ++rng) {
          unsigned a;
          r->getVal(&a,1,&rng);
          printf(" %04x", a);
        }
        printf("\n");
      }
    }
  }

  void TPGYaml::dump_rcvr(unsigned n) {
  }

  void TPGYaml::force_sync() { SET_U32(GenStatus,1); }
 
  void TPGYaml::reset_xbar()
  {
    unsigned v=1;  // from FPGA
    for(unsigned i=0; i<4; i++) {
      IndexRange rng(i);
      IScalVal::create(_private->root->findByName("mmio/AmcCarrierTimingGenerator/AmcCarrierCore/AxiSy56040/OutputConfig"))->setVal(&v,1,&rng);
    }
  }

  void TPGYaml::setSequenceRequired(unsigned iseq, unsigned requiredMask) 
  {
    //  Firmware wants index to start at beginning of beam sequence engines
    if (iseq < nAllowEngines())
      return;
    iseq -= nAllowEngines();
    if (iseq < nBeamEngines()) {
      IndexRange rng(iseq);
      IScalVal::create(_private->tpg->findByName("TPGControl/BeamSeqAllowMask"))->setVal(&requiredMask,1,&rng);
    }
  }

  void TPGYaml::setSequenceDestination(unsigned iseq, TPGDestination destn) 
  {
    //  Firmware wants index to start at beginning of beam sequence engines
    if (iseq < nAllowEngines())
      return;
    iseq -= nAllowEngines();
    if (iseq < nBeamEngines()) {
      IndexRange rng(iseq);
      IScalVal::create(_private->tpg->findByName("TPGControl/BeamSeqDestination"))->setVal(&destn,1,&rng);
    }
  }

#if 0
  void TPGYaml::setDestinationPriority(const std::list<TPGDestination>& l)
  {
    uint32_t v=0;
    for(std::list<TPGDestination>::const_iterator it=l.begin();
	it!=l.end(); it++)
      v = (v<<4) | ((*it)&0xf);
    _private->device->destnPriority = v;
  }
#endif
  SequenceEngine& TPGYaml::engine(unsigned i)
  { return *_private->sequences[i]; }

  void TPGYaml::resetSequences(const std::list<unsigned>& l)
  {
    uint64_t v=0;
    for(std::list<unsigned>::const_iterator it=l.begin();
	it!=l.end(); it++)
      v |= 1ULL<<(*it);
    SET_REG(SeqRestart,v);
  }

  unsigned TPGYaml::getDiagnosticSequence() const  { return GET_U32(DiagSeq); }

  void     TPGYaml::setDiagnosticSequence(unsigned v) { SET_U32(DiagSeq,v); }

  int  TPGYaml::startBSA            (unsigned array,
                                     unsigned nToAverage,
                                     unsigned avgToAcquire,
                                     EventSelection* selection)
  { return startBSA(array,nToAverage,avgToAcquire,selection,0); }

  int  TPGYaml::startBSA            (unsigned array,
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
          IScalVal::create(_private->tpg->findByName("TPGControl/BsaEventSel"))->setVal(&v,1,&rng);
          v = (nToAverage&0x1fff) | 
            ((maxSevr&3)<<14) |
	    (avgToAcquire<<16);
          IScalVal::create(_private->tpg->findByName("TPGControl/BsaStatSel"))->setVal(&v,1,&rng);
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

  void TPGYaml::stopBSA             (unsigned array)
  {
    IndexRange rng(array);
    unsigned v=0;
    IScalVal::create(_private->tpg->findByName("TPGControl/BsaEventSel"))->setVal(&v,1,&rng);
  }

  void TPGYaml::queryBSA            (unsigned  array,
                                     unsigned& nToAverage,
                                     unsigned& avgToAcquire)
  {
    IndexRange rng(array);
    unsigned v;
    IScalVal_RO::create(_private->tpg->findByName("TPGStatus/BsaStatus"))->getVal(&v,1,&rng);
    nToAverage   = v & 0xffff;
    avgToAcquire = v >> 16; 
  }

  uint64_t TPGYaml::bsaComplete()
  {
    uint64_t v;
    _private->bsaComplete->getVal(&v,1);
    return v;
  }

  void TPGYaml::bsaComplete(uint64_t ack)
  {
    SET_REG(BsaComplete, ack);
  }

  void TPGYaml::setCountInterval(unsigned v) { SET_U32S(CountIntv,v); }
  
  unsigned TPGYaml::getPLLchanges   () const { return GET_U32S(CountPLL); }
  unsigned TPGYaml::get186Mticks    () const { return GET_U32S(Count186M); }
  unsigned TPGYaml::getSyncErrors   () const { return GET_U32S(CountSyncE); }
  unsigned TPGYaml::getCountInterval() const { return GET_U32S(CountIntv); }
  unsigned TPGYaml::getBaseRateTrigs() const { return GET_U32S(CountBRT); }
  unsigned TPGYaml::getInputTrigs(unsigned ch) const { return GET_U32SI(CountTrig,ch); }
  unsigned TPGYaml::getSeqRequests  (unsigned seq) const { return GET_U32SI(CountSeq,seq); }
  unsigned TPGYaml::getSeqRequests  (unsigned* array, unsigned array_size) const
  { unsigned n = array_size;
    //    const unsigned NSEQUENCES = nAllowEngines()+nBeamEngines()+nExptEngines();
    //    if (n > NSEQUENCES)
    //      n = NSEQUENCES;
    IScalVal_RO::create(_private->tpg->findByName("TPGStatus/CountSeq"))->getVal(array,n);
    return n;
  }

  void     TPGYaml::lockCounters    (bool q) { SET_U32(CounterLock,(q?1:0)); }
  void     TPGYaml::setCounter      (unsigned i, EventSelection* s)
  {
    IndexRange rng(i);
    unsigned v = s->word();
    IScalVal::create(_private->tpg->findByName("TPGControl/CounterDef"))->setVal(&v,1,&rng);
  }
  unsigned TPGYaml::getCounter      (unsigned i) { return GET_U32I(CounterDef,i); }

  void     TPGYaml::getMpsState     (unsigned  destination, 
                                     unsigned& latch, 
                                     unsigned& state) 
  {
    unsigned a = GET_U8I(MpsState,destination);
    latch = a&0xf;
    state = (a>>4)&0xf;
  }

  Callback* TPGYaml::subscribeBSA     (unsigned bsaArray,
					  Callback* cb)
  { Callback* v = _private->bsaCallback[bsaArray];
    if (cb)
      _private->bsaCallback[bsaArray] = cb; 
    else
      _private->bsaCallback.erase(bsaArray);
    return v; }

  Callback* TPGYaml::subscribeInterval(Callback* cb)
  { Callback* v = _private->intervalCallback;
    _private->intervalCallback = cb; 
    return v; }

  Callback* TPGYaml::subscribeFault   (Callback* cb)
  { Callback* v = _private->faultCallback;
    _private->faultCallback = cb;
    return v; }

  void TPGYaml::cancel           (Callback* cb)
  { // remove(_private->seqCallback,cb);
    remove(_private->bsaCallback,cb);
    if (_private->intervalCallback==cb) _private->intervalCallback=0;
    if (_private->faultCallback   ==cb) _private->faultCallback=0; }

  void TPGYaml::enableIrq    (unsigned bit, bool q)
  { unsigned v;
    v = GET_U32(IrqControl);
    if (q)
      v |= 1<<bit;
    else
      v &= ~(1<<bit);
    SET_U32(IrqControl,v);
  }

  void TPGYaml::enableSequenceIrq (bool q) 
  { enableIrq(IRQ_CHECKPOINT,q); }

  void TPGYaml::enableIntervalIrq (bool q) 
  { enableIrq(IRQ_INTERVAL,q); }

  void TPGYaml::enableBsaIrq (bool q) 
  { enableIrq(IRQ_BSA,q); }

  void TPGYaml::enableFaultIrq (bool q) 
  { enableIrq(IRQ_FAULT,q); }


  //  Read TPGYaml to find outstanding async notifications and loop through them
  //  If this was the real implementation, we should worry about the
  //    race conditions when adding/removing callback functions.
  void TPGYaml::handleIrq() {

    printf("handleIrq: entered\n");

    timespec tvo; clock_gettime(CLOCK_REALTIME,&tvo);

    // Disable async messaging
    unsigned irqControl=0; SET_REG(IrqControl, irqControl);
    while(1) {
      unsigned irqStatus = GET_U32(IrqStatus);
      irqStatus &= ~(1<<IRQ_BSA); // 09-29-2017, Kukhee Kim, quick bandage to ignore BSA ireq
      if (irqStatus==0) {
        usleep(10000);
      }
      else {
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
          unsigned w = GET_U32(SeqFifoData);
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

      //  Acknowledge
      if ((irqStatus&((1<<IRQ_INTERVAL)|(1<<IRQ_FAULT))))
        SET_REG(IrqStatus, irqStatus);
    }
    perror("Exited TPGYaml::handleIrq()\n");
  }

  void TPGYaml::setL1Trig(unsigned index,
			     unsigned evcode,
			     unsigned delay)
  {}


  void TPGYaml::initADC()
  {
  }

  void TPGYaml::_dumpSeqState(unsigned seq0, unsigned nseq) const
  {
    char buff[256];

    { printf("%15.15s:","CountSeq");
      unsigned* seqcount = new unsigned[64];
      getSeqRequests(seqcount,64);
      unsigned i=seq0;
      for(unsigned j=0; j<nseq; j++,i++) {
        printf(" %8u",seqcount[i]);
        if ((j%10)==9) printf("\n                ");
      }
      delete seqcount;
      printf("\n"); }

    { printf("%15.15s:","SeqState");
      unsigned i=seq0;
      for(unsigned j=0; j<nseq; i++,j++) {
        sprintf(buff,"TPGSeqState/SeqIndex[%u]",i);
        printf(" %8u",_GET_U32(buff,_private->tpg));
        IndexRange rng(i);
        unsigned cc[4];
        IScalVal_RO::create(_private->tpg->findByName("TPGSeqState/SeqCondACount"))->getVal(&cc[0],1,&rng);
        IScalVal_RO::create(_private->tpg->findByName("TPGSeqState/SeqCondBCount"))->getVal(&cc[1],1,&rng);
        IScalVal_RO::create(_private->tpg->findByName("TPGSeqState/SeqCondCCount"))->getVal(&cc[2],1,&rng);
        IScalVal_RO::create(_private->tpg->findByName("TPGSeqState/SeqCondDCount"))->getVal(&cc[3],1,&rng);
        printf(" :%u:%u:%u:%u",cc[0],cc[1],cc[2],cc[3]);
        if ((j%5)==4)
          printf("\n                ");
      }
      printf("\n");
    }
  }


};
