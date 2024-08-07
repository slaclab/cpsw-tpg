//////////////////////////////////////////////////////////////////////////////
// This file is part of 'tpg'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'tpg', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>
#include <arpa/inet.h>
#include <signal.h>
#include <new>

#include "Reg.hh"

#include <string>

#if 1
class PhaseMsmt {
public:
  void     rescan  () { _rescan = 1; }
  void     set(unsigned n) { _ramAddr = n&0x1ff; }
  unsigned scan0   () const { return unsigned(_ramData0)&0x1ffff; }
  unsigned scan1   () const { return unsigned(_ramData1)&0x1ffff; }
private:
  uint32_t       _rsvd[0x28>>2];
  Pds::Cphw::Reg _rescan;
  Pds::Cphw::Reg _delaySet;
  Pds::Cphw::Reg _ramAddr;
  Pds::Cphw::Reg _ramData0;
  Pds::Cphw::Reg _ramData1;
};
#else
class PhaseMsmt {
public:
  void     set(unsigned n) { _cntrl = n&0x1ff; }
  unsigned readback() const { return unsigned(_readb)&0x1ff; }
  unsigned phase   () const { return unsigned(_phase)&0x7ffffff; }
private:
  Pds::Cphw::Reg _cntrl;
  Pds::Cphw::Reg _readb;
  Pds::Cphw::Reg _locked;
  Pds::Cphw::Reg _valid;
  Pds::Cphw::Reg _phase;
};
#endif

void usage(const char* p) {
  printf("Usage: %s [options]\n",p);
  printf("Options: -a <IP addr (dotted notation)> : Use network <IP>\n");
  printf("         -f <filename>                  : Set output file\n");
}

int main(int argc, char** argv) {

  const char* ip = "192.168.2.10";
  const char* fname = "/tmp/phase.dat";
  bool rescan = false;

  opterr = 0;

  char opts[32];
  sprintf(opts,"a:f:rh");

  int c;
  while( (c=getopt(argc,argv,opts))!=-1 ) {
    switch(c) {
    case 'a':
      ip = optarg;
      break;
    case 'f':
      fname = optarg;
      break;
    case 'r':
      rescan = true;
      break;
    default:
      usage(argv[0]); return 1;
    }
  }

  Pds::Cphw::Reg::set(ip, 8192, 0);

  FILE* f = fopen(fname,"w");

  PhaseMsmt* p = new ((void*)0x82000000) PhaseMsmt;

  if (rescan) {
    p->rescan();
    usleep(1000000);
  }

#if 1
  for(unsigned i=0; i<512; i++) {
    p->set(i);
    unsigned v0 = p->scan0();
    unsigned v1 = p->scan1();
    fprintf(f,"{ 0x%x, 0x%x, 0x%x},\n", i, v0, v1);
    printf("{ 0x%x, 0x%x, 0x%x},\n", i, v0, v1);
  }
#else
  for(unsigned i=0; i<512; i++) {
    p->set(i);
    do { usleep(1000); } while (p->readback()!=i);
    unsigned v = p->phase();
    fprintf(f,"{ 0x%x, 0x%x },\n", i,v);
    printf("{ 0x%x, 0x%x },\n", i,v);
  }
#endif

  fclose(f);
  
  return 0;
}
