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
  printf("         -f <filename>                  : Data output file\n");
  printf("         -s                             : Sparse scan\n");
  printf("         -p <prescale>                  : Sample count prescale (2**(17+<prescale>))\n");
}

static void* progress(void*)
{
  unsigned row, col;
  while(1) {
    sleep(60);
    Cphw::GthEyeScan::progress(row,col);
    printf("progress: %d,%d\n", row, col);
  }
  return 0;
}

int main(int argc, char* argv[])
{
  extern char* optarg;
  char c;

  const char* ip = "192.168.2.10";
  unsigned short port = 8192;
  unsigned prescale = 0;
  const char* outfile = "eyescan.dat";
  bool lsparse = false;
  const char* yaml_file = 0;
  const char* yaml_path = "mmio/AmcCarrierCheckout";

  while ( (c=getopt( argc, argv, "a:f:p:sy:h")) != EOF ) {
    switch(c) {
    case 'a': ip = optarg; break;
    case 'f': outfile = optarg; break;
    case 'p': prescale = strtoul(optarg,NULL,0); break;
    case 's': lsparse = true; break;
    case 'y':
      if (strchr(optarg,',')) {
        yaml_file = strtok(optarg,",");
        yaml_path = strtok(NULL,",");
      }
      else
        yaml_file = optarg;
      break;
    case 'h': 
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

  pthread_attr_t tattr;
  pthread_attr_init(&tattr);
  pthread_t tid;
  if (pthread_create(&tid, &tattr, &progress, 0))
    perror("Error creating progress thread");

  //  Setup AMC
  Cphw::GthEyeScan* gth = new Cphw::GthEyeScan(path->findByName(yaml_path));
  if (!gth->enabled()) {
    gth->enable(true);
    // reset rx
    t->bbReset();
    sleep(1);
  }
  gth->scan(outfile,prescale,1,lsparse);

  return 0;

}
  
