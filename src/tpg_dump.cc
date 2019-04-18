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
#include "tpg_yaml.hh"
#include "sequence_engine_yaml.hh"

static void cache_dump(unsigned engine,
                       const std::vector<TPGen::InstructionCache>& cache)
{
  printf("Engine %02u\n",engine);
  for(unsigned i=0; i<cache.size(); i++)
    printf("  Sequence %02d  address 0x%04x  ramsize %4u  ninstr %u\n",
           cache[i].index, cache[i].ram_address, cache[i].ram_size, cache[i].instructions.size());
}

void usage(const char* p) {
  printf("Usage: %s [options]\n",p);
  printf("Options: -a <ip address, dotted notation>\n");
  printf("         -y <yaml file>\n");
  printf("         -e <engine>  Dump cache for <engine>\n");
  printf("         -E           Dump cache for all engines\n");
}

int main(int argc, char* argv[])
{
  extern char* optarg;
  char c;

  const char* ip = "192.168.2.10";
  unsigned short port = 8192;
  const char* yaml_file = 0;
  const char* yaml_path = "NetIODev";
  int engine = -1;

  while ( (c=getopt( argc, argv, "a:e:Ey:p:h")) != EOF ) {
    switch(c) {
    case 'a':
      ip = optarg;
      break;
    case 'e':
      engine = strtoul(optarg,NULL,0);
      break;
    case 'E':
      engine = -2;
      break;
    case 'y':
      yaml_file = optarg;
      break;
    case 'p':
      yaml_path = optarg;
      break;
    default:
      usage(argv[0]);
      return 0;
    }
  }

  IYamlFixup* fixup = new Cphw::IpAddrFixup(ip);
  Path path = IPath::loadYamlFile(yaml_file,yaml_path,0,fixup);
  delete fixup;

  TPGen::TPGYaml* tpg = new TPGen::TPGYaml(path,false);

  tpg->dump();

  if (engine==-2) {
    for(unsigned i=0; i<tpg->nAllowEngines(); i++)
      cache_dump( i, tpg->engine(i).cache() );
    for(unsigned i=0; i<tpg->nBeamEngines(); i++)
      cache_dump( i+16, tpg->engine(i+16).cache() );
    for(unsigned i=0; i<tpg->nExptEngines(); i++)
      cache_dump( i+32, tpg->engine(i+32).cache() );
  }

  if (engine>=0)
    cache_dump( engine, tpg->engine(engine).cache() );

  delete tpg;
  return 0;
}
