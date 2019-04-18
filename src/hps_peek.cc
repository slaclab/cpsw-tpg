#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include <string>
#include <new>

#include <cpsw_api_user.h>
#include <cpsw_yaml_keydefs.h>
#include <cpsw_yaml.h>

#include "hps_utils.hh"

using namespace Cphw;

void usage(const char* p) {
  printf("Usage: %s [options]\n",p);
  printf("Options: -a <ip address, dotted notation>\n");
  printf("         -y <yaml file>[,<path to timing>]\n");
  printf("         -d Dump raw timing stream\n");
  printf("         -D Dump framed timing\n");
  printf("         -s Dump stats\n");
  printf("         -S <secs>  Dump stats over seconds\n");
  printf("         -t <value> Set timingMode/clkSel\n");
  printf("         -P <value> set polarity\n");
  printf("         -X <output>,<value> set xbar output\n");
  printf("         -x ignore crossbar\n");
  printf("         -r reset FPGA\n");
  printf("         -R reload FPGA\n");
}

int main(int argc, char* argv[])
{
  extern char* optarg;
  char c;

  const char* ip = "192.168.2.10";
  unsigned short port = 8192;
  bool dumpStats = false;
  int dumpStatsSecs = 0;
  bool lDump0=false, lDump1=false;
  const char* yaml_file = 0;
  const char* yaml_path = "mmio/AmcCarrierCheckout";
  int polarity = -1;
  int xbar_o =-1, xbar_v = -1;
  int timingMode = -1;
  const char* endptr;
  bool ignoreXbar=false;
  bool lreset = false;
  bool lreload = false;

  while ( (c=getopt( argc, argv, "a:dDP:X:hsS:t:y:xrR")) != EOF ) {
    switch(c) {
    case 'a':
      ip = optarg;
      break;
    case 'd':
      lDump0 = true;
      break;
    case 'D':
      lDump1 = true;
      break;
    case 's':
      dumpStats = true;
      dumpStatsSecs = 1;
      break;
    case 'S':
      dumpStats = true;
      dumpStatsSecs = strtoul(optarg,NULL,0);
      break;
    case 't':
      timingMode = strtoul(optarg,NULL,0);
      break;
    case 'y':
      if (strchr(optarg,',')) {
        yaml_file = strtok(optarg,",");
        yaml_path = strtok(NULL,",");
      }
      else
        yaml_file = optarg;
      break;
    case 'P':
      polarity = strtoul(optarg,NULL,0);
      break;
    case 'X':
      if (strchr(optarg,',')) {
        xbar_o = strtoul(strtok(optarg,","),NULL,0);
        xbar_v = strtoul(strtok(NULL,","),NULL,0);
      }
      else {
        printf("Error parsing <out>,<value> from %s\n",optarg);
        return 0;
      }
      break;
    case 'x':
      ignoreXbar = true;
      break;
    case 'r':
      lreset = true;
      break;
    case 'R':
      lreload = true;
      break;
    default:
      usage(argv[0]);
      return 0;
    }
  }

  IYamlFixup* fixup = new IpAddrFixup(ip);
  Path path = IPath::loadYamlFile(yaml_file,"NetIODev",0,fixup);
  delete fixup;

  //  unsigned ilcls = lcls2 ? 1:0;
  unsigned ilcls = 1;

  Cphw::AmcTiming* t = new Cphw::AmcTiming(path->findByName(yaml_path));
  printf("buildStamp %s\n",t->version().buildStamp().c_str());
  printf("upTime %u seconds\n",t->version().upTime());

  if (!ignoreXbar) {

    if (xbar_o>=0) {
      //    t->xbar().setOut( XBar::FPGA, XBar::Map(xbar) );
      t->xbar().setOut( XBar::Map(xbar_o), XBar::Map(xbar_v) );
    }

    t->xbar().dump();
  }

  if (polarity>=0)
    t->setPolarity(polarity);

  if (timingMode>=0) {
    IScalVal::create(t->_timingRx->findByName("ClkSel"))->setVal((uint32_t*)&timingMode);
    uint32_t v;
    IScalVal::create(t->_timingRx->findByName("ClkSel"))->getVal(&v);
    printf("Timing mode = %u\n",v);
  }
    
    
  if (lDump0)
    t->ring0().clear_and_dump();
  if (lDump1)
    t->ring1().clear_and_dump();


  //
  //  Check Link Up, Frame counters (TPR link_test)
  //
  if (dumpStats) {
    Cphw::TimingStats s0, s1;
    struct timespec tv_begin, tv_end;
    clock_gettime(CLOCK_REALTIME, &tv_begin);
    s0 = t->getStats();

    do {
      usleep(1000000);

      clock_gettime(CLOCK_REALTIME, &tv_end);
      s1 = t->getStats();
      double dt = double(tv_end.tv_sec - tv_begin.tv_sec) + 1.e-9*(double(tv_end.tv_nsec)-double(tv_begin.tv_nsec));
      if (dumpStatsSecs==1) {
        double rxClkFreq = double(s1.rxclks-s0.rxclks)*16.e-6 / dt;
        printf("RxRecClkFreq: %7.2f\n", 
               rxClkFreq);
        double txClkFreq = double(s1.txclks-s0.txclks)*16.e-6 / dt;
        printf("TxRefClkFreq: %7.2f\n", 
               txClkFreq);
      
      }

      printf("Link        : %s\n",
             s1.linkup ? "Up  " : "Down");
      printf("RxPolarity  : %u\n", s1.polarity);
      double sofCnts = double(s1.sof-s0.sof) / dt;
      printf("SOFcounts   : %7.0f\n",
             sofCnts);
      double crcErrs = double(s1.crc-s0.crc) / dt;
      printf("CRCerrors   : %7.0f\n",
             crcErrs);
      double decErrs = double(s1.dec-s0.dec) / dt;
      printf("DECerrors   : %7.0f\n",
             decErrs);
      double dspErrs = double(s1.dsp-s0.dsp) / dt;
      printf("DSPerrors   : %7.0f\n",
             dspErrs);
    } while(--dumpStatsSecs);
  }

  if (lreset) 
    t->version().reset();

  if (lreload) 
    t->version().reload();

  return 0;
}
