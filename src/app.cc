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
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>
#include <new>
#include "tpg.hh"
#include "tpg_lcls.hh"
#include "user_sequence.hh"
#include "sequence_engine.hh"
#include "event_selection.hh"
#include "app.hh"

using namespace TPGen;

#if 1
enum { InjectorEngine = 18,
       DiagLineEngine = 17,
       D10Engine = 16 };
#else
enum { InjectorEngine = 0,
       DiagLineEngine = 1,
       D10Engine      = 2 };
#endif

class BeamCheckpoint : public TPGen::Callback {
public:
  BeamCheckpoint(unsigned engine) : _engine(engine), _counts(0) {}
  ~BeamCheckpoint() { printf("BeamCheckpoint[%u] %u\n",_engine,_counts); } 
public:
  void routine() { _counts++; }
private:
  unsigned _engine;
  unsigned _counts;
};

class IntervalCallback : public TPGen::Callback {
public:
  IntervalCallback() : _counts(0) {}
  ~IntervalCallback() { printf("IntervalCallback[%d]\n",_counts); }
public:
  void routine() { _counts++; }
private:
  unsigned _counts;
};

class BsaCallback : public TPGen::Callback {
public:
  BsaCallback(TPG& tpg, sem_t& sem) : _tpg(tpg), _sem(sem) 
  {
    timespec ts; clock_gettime(CLOCK_REALTIME,&ts);
    printf("BSA started at %u.%09u\n",
	   unsigned(ts.tv_sec),unsigned(ts.tv_nsec));
  }
  ~BsaCallback() {}
public:
  void routine() { 
    timespec ts; clock_gettime(CLOCK_REALTIME,&ts);
    printf("BSA done    at %u.%09u\n",
	   unsigned(ts.tv_sec),unsigned(ts.tv_nsec));
    //    uint64_t bsaDone = _tpg.bsaComplete();
    uint64_t bsaDone = 1ULL << 8;
    for(unsigned i=0; i<64; i++)
      if ((1ULL<<i) & bsaDone) {
        unsigned ntoAvg, avgToAcq;
        _tpg.queryBSA(i, ntoAvg, avgToAcq);
        printf("  array[%u] %u:%u\n",i,ntoAvg,avgToAcq);
      }
    sem_post(&_sem);
  }
private:
  TPG&   _tpg;
  sem_t& _sem;
};

class FaultCallback : public TPGen::Callback {
public:
  FaultCallback()
  {
  }
  ~FaultCallback() {}
public:
  void routine() { 
    printf("Fault!\n");
  }
private:
};

//
//  Generator programming registers
//

static unsigned next_ul(unsigned def, char*& endPtr)
{
  char* p = endPtr;
  unsigned v = strtoul(endPtr+1,&endPtr,0);
  return (endPtr==p+1) ? def : v;
}

static const char* _opts = "nN:r:RSI:X:b:B:s:E:Mm:D:C:e:";

static const char* _usage = "  -b <bsaRate,nToAvg,nToAcq,array>\n  -n (notify)\n  -r sequence rate marker(0-9)\n  -s <seconds> (sleep before dumping fifo)\n  -B <TPFifo bit,select>\n  -I count_interval\n  -N seqfifo reads\n  -R (reset rate)\n  -S (force Sync)\n  -X rcvr reads\n  -E <energy0,energy1,energy2,energy3>\n  -M (manual fault)\n  -m <buffer> (clear fault)\n  -D <diagnostic sequence> -C <ns,num,den>\n";

static void show_usage(const char* p)
{
  printf("Usage: %s %s\n",p,_usage);
}

const char* TPGen::usage() { return _usage; }

const char* TPGen::opts() { return _opts; }

int TPGen::execute(int argc, char* argv[], TPGen::TPG* p, bool lAsync)
{
  int rate  = -1;
  int nseqfifo = 100;
  unsigned countInterval = 0;
  unsigned rcvrReads = 0;
  bool reset_rate=false;
  bool notify=false;
  bool resync=false;
  bool lbsa=false;
  int      bsa_rate=0;
  unsigned bsa_ntoavg=2;
  unsigned bsa_ntoacq=4;
  unsigned bsa_buffer=0;
  unsigned tpfifoBit=0xd0;
  unsigned tpfifoSelect=0x11e;
  unsigned sleeps = 1;
  std::vector<unsigned> energy;
  bool lFault=false;
  int clearFault=-1;
  int diagSeq=-1;
  int ns=-1, num=-1, den=-1;
  int expSeq=-1;
  
  char* endPtr;
  int c;
  while( (c=getopt(argc,argv,_opts))!=-1 ) {
    switch(c) {
    case 'B':
      tpfifoBit    = strtoul(optarg,&endPtr,0);
      tpfifoSelect = next_ul(tpfifoSelect,endPtr);
      break;
    case 'b':
      lbsa=true;
      bsa_rate   = strtol (optarg,&endPtr,0);
      bsa_ntoavg = next_ul(bsa_ntoavg,endPtr);
      bsa_ntoacq = next_ul(bsa_ntoacq,endPtr);
      bsa_buffer = next_ul(bsa_buffer,endPtr);
      break;
    case 'C':
      ns  = strtoul(optarg,&endPtr,0);
      num = next_ul(num,endPtr);
      den = next_ul(den,endPtr);
      break;
    case 'D':
      diagSeq = strtoul(optarg,NULL,0);
      break;
    case 'e':
      expSeq = strtoul(optarg,NULL,0);
      break;
    case 'E':
      energy.push_back(strtoul(optarg  ,&endPtr,0));
      energy.push_back(strtoul(endPtr+1,&endPtr,0));
      energy.push_back(strtoul(endPtr+1,&endPtr,0));
      energy.push_back(strtoul(endPtr+1,&endPtr,0));
      break;
    case 'm':
      clearFault=strtoul(optarg,NULL,0);
      break;
    case 'M':
      lFault=true;
      break;
    case 'n':
      notify=true;
      break;
    case 'N':
      nseqfifo = strtoul(optarg,NULL,0);
      break;
    case 'r':
      rate = strtoul(optarg,NULL,0);
      break;
    case 's':
      sleeps = strtoul(optarg,NULL,0);
      break;
    case 'R':
      reset_rate = true;
      break;
    case 'S':
      resync = true;
      break;
    case 'I':
      countInterval = strtoul(optarg,NULL,0);
      break;
    case 'X':
      rcvrReads = strtoul(optarg,NULL,0);
      break;
    case 'h':
      show_usage(argv[0]);
      return 1;
    default:
      break;
    }
  }

  if (resync) {
    struct tm tm_s;
    tm_s.tm_year = 1995;
    time_t t0 = mktime(&tm_s);
    timespec ts;
    clock_gettime(CLOCK_REALTIME,&ts);

    printf("now [%u] 1995 [%u]\n",unsigned(ts.tv_sec),unsigned(t0));

    p->setPulseID(0);
    p->setTimestamp(ts.tv_sec-t0,ts.tv_nsec);
    p->force_sync();
    p->initializeRam();
    
    //  Limit count update "interrupts" to 1Hz
    //  We can latch counters at smaller periods; we just can't interrupt/poll at a high rate -
    //    it interferes with other register read/writes.
    p->setCountInterval(185714286);
    p->enableIntervalIrq(true);

#if 0
    std::vector<unsigned> f(10);
    f[0] = 1;
    f[1] = 2;
    f[2] = 10;
    f[3] = 100;
    f[4] = 1000;
    f[5] = 10000;
    f[6] = 100000;
    f[7] = 1000000;
    f[8] = 0;
    f[9] = 0;
    p->setFixedDivisors(f);
    p->loadDivisors();
#endif
  }
  if (ns>0) {
    p->setClockStep(ns,num,den);
  }

  p->dump();

  if (energy.size())
    p->setEnergy(energy);

  if (clearFault>=0)
    p->clearHistoryBuffers(clearFault);

  if (expSeq>=0) {
    unsigned istart=0;
    unsigned i;
    std::vector<Instruction*> hxu_seq;
    hxu_seq.push_back(new FixedRateSync(0,1));
    hxu_seq.push_back(new ExptRequest(0xffff));
    hxu_seq.push_back(new Branch(istart));

    printf("insertSeq\n");
    int iseq0 = p->engine(expSeq).insertSequence(hxu_seq);
    if (iseq0<0)
      printf("Expseq [%d] insertSequence failed: %d\n",expSeq,iseq0);
    
    printf("--ExpSeq sequence [%u] %d --\n", expSeq, iseq0);
    p->engine(expSeq).dump();

    p->engine(expSeq).setAddress(iseq0,istart);

    std::list<unsigned> resetEngines;
    resetEngines.push_back(expSeq);
    p->resetSequences(resetEngines);
  }

  if (rate>=0) {
#if 0
    //
    //  Set kicker controls
    //
    p->setDestinationControl(Injector,0x100);
    p->setDestinationControl(DiagLine,0x040);
    p->setDestinationControl(D10     ,0x008);
#endif
    //  
    //  Generate two competing sequences
    //
    unsigned istart=0;
    unsigned i;
    std::vector<Instruction*> hxu_seq;
#if 0
    hxu_seq.push_back(new FixedRateSync(4,1));
    if (notify)  
      hxu_seq.push_back(new Checkpoint(new BeamCheckpoint(Injector)));
    i = hxu_seq.size();
    hxu_seq.push_back(new FixedRateSync(2,2));
    hxu_seq.push_back(new BeamRequest(1));
    hxu_seq.push_back(new FixedRateSync(2,2));
    hxu_seq.push_back(new BeamRequest(1));
    hxu_seq.push_back(new FixedRateSync(0,1));
    hxu_seq.push_back(new BeamRequest(1));
    hxu_seq.push_back(new Branch(i+4,ctrA,6));
    hxu_seq.push_back(new Branch(i+2,ctrB,3));
    hxu_seq.push_back(new FixedRateSync(3,2));
    hxu_seq.push_back(new Branch(i  ,ctrC,2));
    hxu_seq.push_back(new Branch(istart));
#else
    hxu_seq.push_back(new FixedRateSync(rate,4));
    hxu_seq.push_back(new BeamRequest(1));
    hxu_seq.push_back(new Branch(istart,ctrA,250));
    hxu_seq.push_back(new Branch(istart,ctrB,100));
    hxu_seq.push_back(new Checkpoint(new BeamCheckpoint(Injector)));
    hxu_seq.push_back(new Branch(istart));
#endif

    std::vector<Instruction*> sxu_seq;
    /*
    sxu_seq.push_back(new FixedRateSync(3,2));
    if (notify)  
      sxu_seq.push_back(new Checkpoint(new BeamCheckpoint(DiagLine)));
    sxu_seq.push_back(new FixedRateSync(2,5));
    i = sxu_seq.size();
    sxu_seq.push_back(new FixedRateSync(0,1));
    sxu_seq.push_back(new BeamRequest(6));
    sxu_seq.push_back(new Branch(i,ctrA,5));
    sxu_seq.push_back(new Branch(istart));
    */
    sxu_seq.push_back(new FixedRateSync(rate,1));
    sxu_seq.push_back(new BeamRequest(6));
    sxu_seq.push_back(new FixedRateSync(rate,2));
    sxu_seq.push_back(new BeamRequest(6));
    sxu_seq.push_back(new Branch(istart));

    if (InjectorEngine>15) {
      p->setSequenceRequired   (InjectorEngine,0);
      p->setSequenceDestination(InjectorEngine,Injector);
    }

    printf("insertSeq\n");
    int iseq0 = p->engine(InjectorEngine).insertSequence(hxu_seq);
    if (iseq0<0)
      printf("Injector insertSequence failed: %d\n",iseq0);
    
    printf("--Injector sequence [%u]--\n", InjectorEngine);
    p->engine(InjectorEngine).dump();

    if (DiagLineEngine>15) {
      p->setSequenceRequired   (DiagLineEngine,0);
      p->setSequenceDestination(DiagLineEngine,DiagLine);
    }

    int iseq1 = p->engine(DiagLineEngine).insertSequence(sxu_seq);
    if (iseq1<0)
      printf("DiagLine insertSequence failed: %d\n",iseq0);

    printf("--DiagLine sequence [%u]--\n", DiagLineEngine);
    p->engine(DiagLineEngine).dump();

    if (D10Engine>15) {
      p->setSequenceRequired   (D10Engine,0);
      p->setSequenceDestination(D10Engine,D10);
    }

    std::vector<Instruction*> d10_seq;
    d10_seq.push_back(new FixedRateSync(rate,1));
    d10_seq.push_back(new BeamRequest(9));
    d10_seq.push_back(new Branch(istart));

    int iseq2 = p->engine(D10Engine).insertSequence(d10_seq);
    if (iseq2<0)
      printf("D10 insertSequence failed: %d\n",iseq2);

    printf("--D10 sequence [%u]--\n", D10Engine);
    p->engine(D10Engine).dump();

    //
    //
    //  Launch sequences: (1) set reset addresses, (2) reset
    //
    p->engine(InjectorEngine).setAddress(iseq0,istart);
    p->engine(DiagLineEngine).setAddress(iseq1,istart);
    p->engine(D10Engine).setAddress(iseq2,istart);

    std::list<unsigned> resetEngines;
    resetEngines.push_back(InjectorEngine);
    resetEngines.push_back(DiagLineEngine);
    resetEngines.push_back(D10Engine);
    p->resetSequences(resetEngines);
  }
  else if (reset_rate) {
    //  Stop the sequences
    p->engine(InjectorEngine).setAddress(0,0);
    p->engine(DiagLineEngine).setAddress(0,0);
    p->engine(D10Engine).setAddress(0,0);

    std::list<unsigned> resetEngines;
    resetEngines.push_back(InjectorEngine);
    resetEngines.push_back(DiagLineEngine);
    resetEngines.push_back(D10Engine);
    p->resetSequences(resetEngines);
  }

  //  if (rcvrReads)
  //    p->dump_rcvr(rcvrReads);

  if (diagSeq>=0)
    p->setDiagnosticSequence(diagSeq);

  if (countInterval)
    p->setCountInterval(countInterval);

  p->subscribeInterval(new IntervalCallback);
  p->subscribeFault   (new FaultCallback);

  p->enableSequenceIrq(true);
  p->enableFaultIrq   (true);

  if (lbsa) {
    sem_t sem;
    sem_init(&sem,0,0);
    p->subscribeBSA     (bsa_buffer, new BsaCallback(*p,sem));
    if (bsa_rate>=0)
      p->startBSA         (bsa_buffer,bsa_ntoavg,bsa_ntoacq,new FixedRateSelect(bsa_rate));
    else
      p->stopBSA          (bsa_buffer);
    if (lAsync) {
      p->enableBsaIrq     (true);
      sem_wait(&sem);
    }
    else {
      sleep(1);
    }
  }

  if (lFault) {
    sleep(1);
    p->acquireHistoryBuffers(true);
  }

  while(sleeps--) 
    sleep(1);

  p->enableSequenceIrq(false);
  p->enableBsaIrq     (false);
  p->enableFaultIrq   (false);

  delete p->subscribeInterval(0);
  delete p->subscribeFault   (0);
  
  delete p;

  return 1;
}
