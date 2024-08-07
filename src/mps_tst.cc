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

static int addSequence(SequenceEngine& e, unsigned sync, unsigned n)
{
  std::vector<Instruction*> seq;
  seq.push_back(new FixedRateSync(sync,1));
  while(n--) {
    seq.push_back(new BeamRequest  (_charge));
    if (n)
      seq.push_back(new FixedRateSync(0,1));
  }
  seq.push_back(new Branch       (0));

  int nseq = e.insertSequence(seq);
  printf("Inserted sequence %d\n",nseq);

  for(unsigned i=0; i<seq.size(); i++)
    delete seq[i];

  return nseq;
}
 
struct ThreadArgs { 
  TPGen::TPG* tpg;
  unsigned    ieng;
};

static void* recover_thread(void* arg)
{
  ThreadArgs* a = reinterpret_cast<ThreadArgs*>(arg);
  TPGen::TPG* p = a->tpg;

  while(1) {
    unsigned mps;
    while( scanf("%u",&mps)!=1 ) ;
    p->engine(a->ieng).setMPSState(mps);
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
  unsigned ieng=0;
  unsigned istate=6;

  unsigned pc=0;
  int      dst=-1;

  opterr = 0;

  char opts[32];
  sprintf(opts,"a:y:C:L:e:s:h");

  int c;
  while( (c=getopt(argc,argv,opts))!=-1 ) {
    switch(c) {
    case 'a':
      ip = optarg;
      break;
    case 'e':
      ieng = strtoul(optarg,&endptr,0);
      break;
    case 's':
      istate = strtoul(optarg,&endptr,0);
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

    { char buff[256];
      IndexRange rng(0,256);
      IScalVal_RO::create(path->findByName("mmio/AmcCarrierTimingGenerator/AmcCarrierCore/AxiVersion/BuildStamp"))
        ->getVal(reinterpret_cast<uint8_t*>(buff),256);
      buff[255]=0;
      printf("BuildString: %s\n",buff); }
  }
  else {
    printf("No yaml specified\n");
    return -1;
  }

  //  For sequence engine ieng, setup allow tables for trivial rates

  { 
    SequenceEngine& e = p->engine(ieng);
    remSequences(e);
    for(unsigned i=0; i<14; i++) {
      int iseq = addSequence( e, 6-i/2, 1+(i&1));
      if (iseq < 0)
        return -1;
      else {
        e.setMPSJump(i, iseq, i);
        if (i==0) e.setBCSJump(iseq, i);
      }
    }
    
    e.setMPSState(istate); 
  }

  //  For sequence engine 16, setup full rate + engine 0 dependence
  {
    SequenceEngine& e = p->engine(16+ieng);
    remSequences(e);

    p->setSequenceRequired   (16+ieng,(1<<ieng));
    p->setSequenceDestination(16+ieng,ieng);

    int iseq = addSequence(e, 0, 1); 
    e.setAddress(iseq);
    e.reset();
  }

  //  Special request for 10Hz to destination 0
  {
    SequenceEngine& e = p->engine(31);
    remSequences(e);

    p->setSequenceRequired   (31,(1<<0));
    p->setSequenceDestination(31,0);

    int iseq = addSequence(e, 5, 1); 
    e.setAddress(iseq);
    e.reset();
  }

  p->dump();

  p->engine(ieng).dump();
  p->engine(16).dump();

  ThreadArgs* args = new ThreadArgs;
  args->tpg  = p;
  args->ieng = ieng;

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_t rthread;
  pthread_create(&rthread, &attr, &recover_thread, args);

  //  { FixedRateSelect evt(0,0x1);  // all beam requests for destn 0
  for(unsigned i=0; i<16; i++) {
    FixedRateSelect evt(0,(1<<i));
    p->setCounter(i, &evt);
  }

  while(1) {
    sleep(1);
    printf("%17.17s","MPS latch[state]:");
    for(unsigned i=0; i<16; i++) {
      unsigned latch, state;
      p->getMpsState(i,latch,state);
      printf("   %02u [%02u]",latch,state);
    }
    printf("\n%17.17s","Beam Rates:");
    for(unsigned i=0; i<16; i++)
      printf(" %9.9u", p->getCounter(i));
    printf("\n");
  }

  return 0;
}
