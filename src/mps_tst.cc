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

#include <cpsw_api_builder.h>
#include <cpsw_api_user.h>

#include "tpg_yaml.hh"
#include "sequence_engine.hh"
#include "user_sequence.hh"
#include "event_selection.hh"

static unsigned _charge = 0xabcd;

using namespace TPGen;

static void remSequences(SequenceEngine& e)
{
  e.setAddress(0);  // Jump to null sequence
  e.reset();

  for(int i=2; true; i++) {
    int result = e.removeSequence(i);
    printf("Removed sequence %d (%d)\n",i,result);
    if (result < 0) break;
  }
}

static int addSequence(SequenceEngine& e, unsigned sync)
{
  std::vector<Instruction*> seq;
  seq.push_back(new FixedRateSync(sync,1));
  seq.push_back(new BeamRequest  (_charge));
  seq.push_back(new Branch       (0));

  int nseq = e.insertSequence(seq);
  printf("Inserted sequence %d\n",nseq);

  for(unsigned i=0; i<seq.size(); i++)
    delete seq[i];

  return nseq;
}
 
static void* recover_thread(void* arg)
{
  TPGen::TPG* p = reinterpret_cast<TPGen::TPG*>(arg);

  while(1) {
    unsigned mps;
    while( scanf("%u",&mps)!=1 ) ;
    p->engine(0).setMPSState(mps);
    p->engine(0).reset();
  }
}

static void usage(const char* p)
{
  printf("Usage: %s [options]\n"
         "  -a <ip>\n",p);
}

int main(int argc, char* argv[])
{
  const char* ip = "192.168.2.10";
  const char* yaml = 0;
  char* endptr = 0;

  unsigned latch_tag=0;

  unsigned pc=0;
  int      dst=-1;

  opterr = 0;

  char opts[32];
  sprintf(opts,"a:y:C:L:h");

  int c;
  while( (c=getopt(argc,argv,opts))!=-1 ) {
    switch(c) {
    case 'a':
      ip = optarg;
      break;
    case 'y':
      yaml = optarg;
      break;
    case 'C':
      dst = strtoul(optarg,&endptr,0);
      pc  = strtoul(endptr+1,&endptr,0);
      break;
    case 'L':
      latch_tag = strtoul(optarg,NULL,0);
      break;
    case 'h':
      usage(argv[0]); return 1;
    default:
      break;
    }
  }


  TPGen::TPG* p;
  if (yaml) {
    Path path = IPath::loadYamlFile(yaml,"NetIODev");
    p = new TPGen::TPGYaml(path);
  }
  else {
    printf("No yaml specified\n");
    return -1;
  }

  //  For sequence engine 0, setup allow tables for trivial rates

  { 
    SequenceEngine& e = p->engine(0);
    remSequences(e);
    for(unsigned i=0; i<7; i++) {
      int iseq = addSequence( e, 6-i);
      if (iseq < 0)
        return -1;
      else
        e.setMPSJump(i, iseq, i);
    }
    e.setMPSState(6); 
  }

  //  For sequence engine 16, setup full rate + engine 0 dependence
  {
    SequenceEngine& e = p->engine(16);
    remSequences(e);

    p->setSequenceRequired   (16,0x1);
    p->setSequenceDestination(16,0);

    int iseq = addSequence(e, 0); 
    e.setAddress(iseq);
    e.reset();
  }

  p->dump();

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_t rthread;
  pthread_create(&rthread, &attr, &recover_thread, p);

  { FixedRateSelect evt(0,0x1);  // all beam requests for destn 0
    p->setCounter(0, &evt); }
  p->lockCounters(true);

  while(1) {
    sleep(1);
    p->lockCounters(true);
    unsigned latch, state;
    p->getMpsState(0,latch,state);
    printf("MPS latch [%u]  state [%u]  Beam Rate [%u]\n",
           latch, state, p->getCounter(0));
  }

  return 0;
}
