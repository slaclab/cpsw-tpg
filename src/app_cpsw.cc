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
#include <new>
#include "bsp/tpg.hh"
#include "bsp/user_sequence.hh"
#include "bsp/sequence_engine.hh"
#include "bsp/event_selection.hh"
#include "bsp/tpg_lcls.hh"
#include "app/app.hh"

using namespace TPGen;


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
  BsaCallback(sem_t& sem) : _sem(sem) 
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
    sem_post(&_sem);
  }
private:
  sem_t& _sem;
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

static const char* _opts = "nN:r:f:RSI:X:b:B:";

static const char* _usage = "[-b <bsaRate,nToAvg,nToAcq>] [-f fifo reads] [-n (notify)] [-r sequence rate marker(0-9)] [-B <TPFifo bit,select>] [-I count_interval] [-N seqfifo reads] [-R (reset rate)] [-S (force Sync)] [-X rcvr reads]";

static void show_usage(const char* p)
{
  printf("Usage: %s %s\n",p,_usage);
}

const char* TPGen::usage() { return _usage; }

const char* TPGen::opts() { return _opts; }

int TPGen::execute(int argc, char* argv[], TPGen::TPG* p, bool lAsync)
{
  int rate  = -1;
  int nfifo = 10;
  int nseqfifo = 100;
  unsigned countInterval = 0;
  unsigned rcvrReads = 0;
  bool reset_rate=false;
  bool notify=false;
  bool resync=false;
  bool lbsa=false;
  unsigned bsa_rate=0;
  unsigned bsa_ntoavg=2;
  unsigned bsa_ntoacq=4;
  unsigned tpfifoBit=0xd0;
  unsigned tpfifoSelect=0x11e;
  
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
      bsa_rate   = strtoul(optarg,&endPtr,0);
      bsa_ntoavg = next_ul(bsa_ntoavg,endPtr);
      bsa_ntoacq = next_ul(bsa_ntoacq,endPtr);
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
    case 'R':
      reset_rate = true;
      break;
    case 'S':
      resync = true;
      break;
    case 'f':
      nfifo = strtoul(optarg,NULL,0);
      break;
    case 'I':
      countInterval = strtoul(optarg,NULL,0);
      break;
    case 'X':
      rcvrReads = strtoul(optarg,NULL,0);
      break;
    case 'h':
    case '?':
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
  }

  p->setL1Trig(0, 1,10);
  p->setL1Trig(1,40,20);
  p->setL1Trig(2,41,30);
  p->setL1Trig(3,42,40);
  p->dump();

  if (rate>=0) {
    //
    //  Set priority
    //
    std::list<TPGDestination> priority;
    priority.push_back(Injector);  // highest priority
    priority.push_back(DiagLine);  // lowest priority
    p->setDestinationPriority(priority);
    //  
    //  Generate two competing sequences
    //
    unsigned istart=0;
    unsigned i;
    std::vector<Instruction*> hxu_seq;
#if 1
    hxu_seq.push_back(new FixedRateSync(4,1));
    if (notify)  
      hxu_seq.push_back(new Checkpoint(new BeamCheckpoint(Injector)));
    i = hxu_seq.size();
    hxu_seq.push_back(new FixedRateSync(2,2,new BeamRequest(1)));
    hxu_seq.push_back(new FixedRateSync(0,1,new BeamRequest(1)));
    hxu_seq.push_back(new Branch(i+1,ctrA,6));
    hxu_seq.push_back(new Branch(i  ,ctrB,3));
    hxu_seq.push_back(new FixedRateSync(3,2));
    hxu_seq.push_back(new Branch(i  ,ctrC,2));
    hxu_seq.push_back(new Branch(istart));
#else
    hxu_seq.push_back(new FixedRateSync(rate,4,new BeamRequest(1)));
    hxu_seq.push_back(new Branch(istart));
#endif
    int iseq0 = p->engine(Injector).insertSequence(hxu_seq);
    if (iseq0<0)
      printf("Injector insertSequence failed: %d\n",iseq0);
    p->engine(Injector).dump();

    std::vector<Instruction*> sxu_seq;
    /*
    sxu_seq.push_back(new FixedRateSync(3,2));
    if (notify)  
      sxu_seq.push_back(new Checkpoint(new BeamCheckpoint(DiagLine)));
    sxu_seq.push_back(new FixedRateSync(2,5));
    i = sxu_seq.size();
    sxu_seq.push_back(new FixedRateSync(0,1,new BeamRequest(6)));
    sxu_seq.push_back(new Branch(i,ctrA,5));
    sxu_seq.push_back(new Branch(istart));
    */
    sxu_seq.push_back(new FixedRateSync(rate,1, new BeamRequest(6)));
    sxu_seq.push_back(new Branch(istart));

    int iseq1 = p->engine(DiagLine).insertSequence(sxu_seq);
    if (iseq1<0)
      printf("DiagLine insertSequence failed: %d\n",iseq0);
    p->engine(DiagLine).dump();

    //
    //
    //  Launch sequences: (1) set reset addresses, (2) reset
    //
    p->engine(Injector).setAddress(iseq0,istart);
    p->engine(DiagLine).setAddress(iseq1,istart);

    std::list<unsigned> resetEngines;
    resetEngines.push_back(Injector);
    resetEngines.push_back(DiagLine);
    p->resetSequences(resetEngines);

    std::vector<Instruction*> trg_seq;
    trg_seq.push_back(new FixedRateSync(rate,1,new BeamRequest(2)));
    trg_seq.push_back(new Branch(0));
    int itrg = p->engine(DiagLine).insertSequence(trg_seq);
    p->engine(DiagLine).setTrgJump(0+5,itrg,0);
  }
  else if (reset_rate) {
    //  Stop the sequences
    p->engine(Injector).setAddress(0,0);
    p->engine(DiagLine).setAddress(0,0);

    std::list<unsigned> resetEngines;
    resetEngines.push_back(Injector);
    resetEngines.push_back(DiagLine);
    p->resetSequences(resetEngines);
  }

  //  if (rcvrReads)
  //    p->dump_rcvr(rcvrReads);

  if (countInterval)
    p->setCountInterval(countInterval);

  p->subscribeInterval(new IntervalCallback);

  p->enableSequenceIrq(true);
  p->enableIntervalIrq(true);

  if (lbsa) {
    sem_t sem;
    sem_init(&sem,0,0);
    p->subscribeBSA     (0, new BsaCallback(sem));
    p->reset_fifo       (tpfifoBit,tpfifoSelect);
    p->startBSA         (0,bsa_ntoavg,bsa_ntoacq,new FixedRateSelect(bsa_rate));
    if (lAsync) {
      p->enableBsaIrq     (true);
      sem_wait(&sem);
    }
  }
  else {
    p->reset_fifo(tpfifoBit,tpfifoSelect);
    sleep(1);
  }

  p->enableSequenceIrq(false);
  p->enableIntervalIrq(false);
  p->enableBsaIrq     (false);

  p->dumpFIFO(nfifo);

  delete p->subscribeInterval(0);
  
  //  delete p;

  return 1;
}
