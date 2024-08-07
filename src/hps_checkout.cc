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
  printf("         -2 With -r or -R, select LCLS-II timing (default)\n");
  printf("         -i With -r or -R, invert polarity (default is not)\n");
  printf("         -r Reset to external (RTM0) timing\n");
  printf("         -R Reset to internal (FPGA) timing\n");
  printf("         -t Align target\n");
  printf("         -x Value to override XBar output mapping with\n");
  printf("         -m Measure and dump clocks\n");
  printf("         -c Clear counters\n");
  printf("         -y <yaml file>[,<path to timing>]\n");
  printf("         -s Dump stats\n");
}

int main(int argc, char* argv[])
{
  extern char* optarg;
  char c;

  const char* ip = "192.168.2.10";
  unsigned short port = 8192;
  bool dumpStats = false;
  bool lcls2 = true;
  bool lReset = false;
  bool lInternal = false;
  bool invertPolarity = false;
  bool measureClks = false;
  bool lClear = false;
  int alignTarget = -1;
  bool lXbar = false;
  XBar::Map vXbar = XBar::RTM0;
  bool lDump0=false, lDump1=false;
  const char* yaml_file = 0;
  const char* yaml_path = "mmio/AmcCarrierCheckout";

  while ( (c=getopt( argc, argv, "a:cdDhimrRst:x:y:2")) != EOF ) {
    switch(c) {
    case 'a':
      ip = optarg;
      break;
    case 'c':
      lClear = true;
      break;
    case 'd':
      lDump0 = true;
      break;
    case 'D':
      lDump1 = true;
      break;
    case 'i':
      invertPolarity = true;
      break;
    case 'm':
      measureClks = true;
      break;
    case 'r':
      lReset = true;
      lInternal = false;
      break;
    case 'R':
      lReset = true;
      lInternal = true;
      break;
    case 's':
      dumpStats = true;
      break;
    case 't':
      alignTarget = strtoul(optarg,NULL,0);
      break;
    case 'x':
      lXbar = true;
      vXbar = XBar::Map(strtoul(optarg,NULL,0));
      break;
    case 'y':
      if (strchr(optarg,',')) {
        yaml_file = strtok(optarg,",");
        yaml_path = strtok(NULL,",");
      }
      else
        yaml_file = optarg;
      break;
    case '2':
      lcls2 = true;
      break;
    default:
      usage(argv[0]);
      return 0;
    }
  }

  IYamlFixup* fixup = new IpAddrFixup(ip);
  Path path = IPath::loadYamlFile(yaml_file,"NetIODev",0,fixup);
  delete fixup;

  static const double ClkMin[] = { 118, 185 };
  static const double ClkMax[] = { 120, 186 };
  static const double FrameMin[] = { 356, 928000 };
  static const double FrameMax[] = { 362, 929000 };
  //  unsigned ilcls = lcls2 ? 1:0;
  unsigned ilcls = 1;

  Cphw::AmcTiming* t = new Cphw::AmcTiming(path->findByName(yaml_path));
  printf("buildStamp %s\n",t->version().buildStamp().c_str());

  //
  //  Configure the XBAR (RTM for slot 2 and BP for all others)
  //
  if (lXbar) {
    t->xbar().setOut( XBar::FPGA, vXbar );
  }
  else {
    if (strcmp(strrchr(ip,'.')+1,"102")==0)
      t->xbar().setOut( XBar::FPGA, XBar::RTM0 );
    else
      t->xbar().setOut( XBar::FPGA, XBar::BP );
  }
  t->xbar().dump();

  //
  //  Reset link with LCLS-II Timing
  //
  t->setLCLSII();
  t->setPolarity(false);
  usleep(1000);

  //
  //  Check Link Up, Frame counters (TPR link_test)
  //
  t->resetStats();
  Cphw::TimingStats s0, s1;
  struct timespec tv_begin, tv_end;
  clock_gettime(CLOCK_REALTIME, &tv_begin);
  s0 = t->getStats();
  usleep(1000000);

  clock_gettime(CLOCK_REALTIME, &tv_end);
  s1 = t->getStats();
  double dt = double(tv_end.tv_sec - tv_begin.tv_sec) + 1.e-9*(double(tv_end.tv_nsec)-double(tv_begin.tv_nsec));
  double rxClkFreq = double(s1.rxclks-s0.rxclks)*16.e-6 / dt;
  printf("RxRecClkFreq: %7.2f  %s\n", 
         rxClkFreq,
         (rxClkFreq > ClkMin[ilcls] &&
          rxClkFreq < ClkMax[ilcls]) ? "PASS":"FAIL");
  double txClkFreq = double(s1.txclks-s0.txclks)*16.e-6 / dt;
  printf("TxRefClkFreq: %7.2f  %s\n", 
         txClkFreq,
         (txClkFreq > ClkMin[ilcls] &&
          txClkFreq < ClkMax[ilcls]) ? "PASS":"FAIL");
  printf("Link        : %s     %s\n",
         s1.linkup ? "Up  " : "Down", s1.linkup ? "PASS":"FAIL");
  double sofCnts = double(s1.sof-s0.sof) / dt;
  printf("SOFcounts   : %7.0f  %s\n",
         sofCnts,
         (sofCnts > FrameMin[ilcls] &&
          sofCnts < FrameMax[ilcls]) ? "PASS":"FAIL");
  double crcErrs = double(s1.crc-s0.crc) / dt;
  printf("CRCerrors   : %7.0f  %s\n",
         crcErrs,
         crcErrs == 0 ? "PASS":"FAIL");
  double decErrs = double(s1.dec-s0.dec) / dt;
  printf("DECerrors   : %7.0f  %s\n",
         decErrs,
         decErrs == 0 ? "PASS":"FAIL");
  double dspErrs = double(s1.dsp-s0.dsp) / dt;
  printf("DSPerrors   : %7.0f  %s\n",
         dspErrs,
         dspErrs == 0 ? "PASS":"FAIL");

  return 0;
}
