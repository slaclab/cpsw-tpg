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
#include "app.hh"
#include "tpg_cpsw.hh"
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

static void usage(const char* p)
{
  printf("Usage: %s [options]\n"
         "  -a <ip>\n",p);
}

typedef struct {
  unsigned ns;
  unsigned num;
  unsigned den;
} clk_rate_t;

static clk_rate_t clk_rate[] = { {5,5,13},
                                 {5,2,5},
                                 {5,5,12},
                                 {5,3,5},
                                 {5,6,23} };

int main(int argc, char* argv[])
{
  const char* ip = "192.168.2.10";

  opterr = 0;

  int c;
  while( (c=getopt(argc,argv,"a:h"))!=-1 ) {
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

  TPGen::TPG* p = new TPGen::TPGCpsw(ip);

  unsigned irate=0;
  while(1) {
    p->setClockStep(clk_rate[irate].ns,
                    clk_rate[irate].num,
                    clk_rate[irate].den);
    
    timespec tso; clock_gettime(CLOCK_REALTIME,&tso);

    uint64_t pido = p->getPulseID();
    uint32_t tsosec,tsonsec;  p->getTimestamp(tsosec,tsonsec);

    sleep(2);

    timespec ts; clock_gettime(CLOCK_REALTIME,&ts);

    uint64_t pid = p->getPulseID();
    uint32_t tssec,tsnsec;  p->getTimestamp(tssec,tsnsec);

    double dtc = double(clk_rate[irate].ns)+double(clk_rate[irate].num)/double(clk_rate[irate].den);
    double dtm = double(ts.tv_sec-tso.tv_sec)+1.e-9*(double(ts.tv_nsec)-double(tso.tv_nsec));
    double dts = double(tssec-tsosec)+1.e-9*(double(tsnsec)-double(tsonsec));
    double dpid = pid-pido;

    printf("[%d,%d,%d] %f : %f : %f\n",
           clk_rate[irate].ns,
           clk_rate[irate].num,
           clk_rate[irate].den,
           dtc*dpid*200.e-9, dtm, dts);

    irate = irate+1;
    if (irate == sizeof(clk_rate)/sizeof(clk_rate_t))
      irate=0;
  }
  return 1;
}
