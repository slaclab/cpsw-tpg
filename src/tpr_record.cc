//
//  Command-line driver of TPR configuration
//
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>
#include <stdio.h>

#include "tpr.hh"
#include "tprsh.hh"
#include "tprdata.hh"

#include <string>
#include <vector>
#include <sstream>

using namespace Tpr;

static const double CLK_FREQ = 1300e6/7.;

static unsigned linktest_period = 1;

extern int optind;

static void frame_capture      (TprReg&, char, bool lcls2);
static void dump_frame         (const uint32_t*);
static bool parse_frame        (const uint32_t*, uint64_t&, uint64_t&);

static void usage(const char* p) {
  printf("Usage: %s [options]\n",p);
  printf("Options: -d <device>   : open device (/dev/tpra)\n");
  printf("         -f <filename> : record frame capture to file (tpr.dat)\n");
}

int main(int argc, char** argv) {

  extern char* optarg;
  int c;
  const char* fname = "tpr.dat";
  const char* dname = "/dev/tpra";

  while ( (c=getopt( argc, argv, "d:f:h?")) != EOF ) {
    switch(c) {
    case 'd': dname = optarg; break;
    case 'f': fname = optarg; break;
    case 'h':
    case '?':
    default:
      usage(argv[0]);
      exit(0);
    }
  }

  printf("Using tpr %s\n",dname);

  int fd = open(dname, O_RDWR);
  if (fd<0) {
    perror("Could not open");
    return -1;
  }

  void* ptr = mmap(0, sizeof(TprReg), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED) {
    perror("Failed to map");
    return -2;
  }

  TprReg& reg = *reinterpret_cast<TprReg*>(ptr);
  printf("FpgaVersion: %08X\n", reg.version.FpgaVersion);
  printf("BuildStamp: %s\n", reg.version.buildStamp().c_str());

  reg.xbar.setEvr( XBar::StraightIn );
  reg.xbar.setEvr( XBar::StraightOut);
  reg.xbar.setTpr( XBar::StraightIn );
  reg.xbar.setTpr( XBar::StraightOut);

  reg.tpr.clkSel(true);  // LCLS2 clock
  reg.tpr.rxPolarity(false);

  {
    int idx=0;
    char dev[16];
    sprintf(dev,"%s%u",dname,idx);

    int fd = open(dev, O_RDONLY);
    if (fd<0) {
      printf("Open failure for dev %s [FAIL]\n",dev);
      perror("Could not open");
      exit(1);
    }

    void* ptr = mmap(0, sizeof(TprQueues), PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
      perror("Failed to map - FAIL");
      exit(1);
    }

    unsigned _channel = 0;
    reg.base.setupTrigger(_channel,
                          _channel,
                          0, 0, 1, 0);
    unsigned ucontrol = reg.base.channel[_channel].control;
    reg.base.channel[_channel].control = 0;

    unsigned urate   = 0; // max rate
    unsigned destsel = 1<<17; // BEAM - DONT CARE
    reg.base.channel[_channel].evtSel = (destsel<<13) | (urate<<0);
    reg.base.channel[_channel].bsaDelay = 0;
    reg.base.channel[_channel].bsaWidth = 1;
    reg.base.channel[_channel].control = ucontrol | 1;

    FILE* f = fopen(fname,"w");
    if (!f) {
      printf("Open failure for output file %s [FAIL]\n",fname);
      perror("Open failed");
      exit(1);
    }

    //  read the captured frames
    TprQueues& q = *(TprQueues*)ptr;

    char* buff = new char[32];

    int64_t allrp = q.allwp[idx];

    read(fd, buff, 32);

    //  capture all frames between 1Hz markers


    uint64_t pulseIdP=0;
    uint64_t pulseId, timeStamp;
    unsigned nframes=0;
    
    bool done  = false;
    bool first = false;

    do {
      while(allrp < q.allwp[idx] && !done) {
        void* p = reinterpret_cast<void*>
          (&q.allq[q.allrp[idx].idx[allrp &(MAX_TPR_ALLQ-1)] &(MAX_TPR_ALLQ-1) ].word[0]);
        Tpr::SegmentHeader* hdr = new(p) Tpr::SegmentHeader;
        while(hdr->type() != Tpr::_Event )
          hdr = hdr->next();
        Tpr::Event* event = new(hdr) Tpr::Event;

        if (!first) {
          if (event->fixedRates & (1<<6)) {
            first = true;
            printf("Record started\n");
            fwrite(hdr+1,sizeof(Event)-sizeof(*hdr),1,f); 
            nframes++;
          }
        }
        else if (event->fixedRates & (1<<6)) {
            done = true;
            printf("Record finished\n");
        }
        else {
          fwrite(hdr+1,sizeof(Event)-sizeof(*hdr),1,f); 
          nframes++;
        }

        allrp++;
      }
      read(fd, buff, 32);
    } while(!done);
    
    //  disable channel 0
    reg.base.channel[_channel].control = 0;

    printf("Write %d frames\n", nframes);
    fclose(f);

    munmap(ptr, sizeof(TprQueues));
    close(fd);
  }
}

