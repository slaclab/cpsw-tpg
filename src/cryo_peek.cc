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
}

int main(int argc, char* argv[])
{
  extern char* optarg;
  char c;

  const char* ip = "192.168.2.10";
  unsigned short port = 8192;
  const char* yaml_file = 0;
  const char* yaml_path = "mmio/AmcCarrierCheckout";

  while ( (c=getopt( argc, argv, "a:y:")) != EOF ) {
    switch(c) {
    case 'a':
      ip = optarg;
      break;
    case 'y':
      if (strchr(optarg,',')) {
        yaml_file = strtok(optarg,",");
        yaml_path = strtok(NULL,",");
      }
      else
        yaml_file = optarg;
      break;
    default:
      usage(argv[0]);
      return 0;
    }
  }

  IYamlFixup* fixup = new IpAddrFixup(ip);
  Path path = IPath::loadYamlFile(yaml_file,"NetIODev",0,fixup);
  delete fixup;

  unsigned v;
  IScalVal_RO::create(path->findByName("mmio/AmcCarrierCore/AxiVersion/FpgaVersion"))->getVal(&v,1);
  printf("%12.12s: %08x\n","FpgaVersion",v);

  IScalVal_RO::create(path->findByName("mmio/AmcCarrierCore/AmcCarrierTiming/TimingFrameRx/sofCount"))->getVal(&v,1);
  printf("%12.12s: %08x\n","sofCount",v);

#define DUMP(s) { \
  IScalVal_RO::create(path->findByName("mmio/TimingExtn/"#s))->getVal(&v); \
  printf("%12.12s: %08x\n",#s,v); }

  DUMP(PulseId);
  DUMP(TimeStamp);
  DUMP(ExtnValid);
  DUMP(TimeCodeHeader);
  DUMP(TimeCode);
  DUMP(BaseRateSince1Hz);
  DUMP(BaseRateSinceTM);

#undef DUMP

  return 0;
}
