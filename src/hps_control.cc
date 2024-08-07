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

  Cphw::AmcTiming* t = new Cphw::AmcTiming(path->findByName(yaml_path));
  printf("buildStamp %s\n",t->version().buildStamp().c_str());

  if (lDump0)
    t->ring0().clear_and_dump();

  if (lDump1)
    t->ring1().clear_and_dump();

  if (alignTarget >= 0) {
    t->setRxAlignTarget(alignTarget);
    t->bbReset();
  }

  if (lReset) {
    t->xbar().setOut( XBar::FPGA, lInternal ? XBar::FPGA : XBar::RTM0 );
    if (!lcls2)
      t->setLCLS();
    else
      t->setLCLSII();
    t->setPolarity(invertPolarity);
    usleep(1000);
    t->resetStats();
    usleep(1000);
  }

  if (lXbar) {
    t->xbar().setOut(XBar::RTM0,vXbar);
    t->xbar().setOut(XBar::FPGA,vXbar);
    t->xbar().setOut(XBar::BP  ,vXbar);
    t->xbar().setOut(XBar::RTM1,vXbar);
  }

  t->xbar().dump();

  if (dumpStats)
    t->dumpStats();

  if (measureClks)
    t->measureClks();

  t->dumpRxAlign();

  if (lClear)
    t->resetStats();

  return 0;
}
