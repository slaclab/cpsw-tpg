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

class MpsDbgRingBuffer : public Cphw::RingBuffer {
public:
  MpsDbgRingBuffer(Path p) : Cphw::RingBuffer(p) {}
public:
  void     dump   () {
    unsigned len;
    _csr->getVal(&len);
    unsigned wid = (len>>20)&0xff;
    len &= (1<<wid)-1;
    len = 0x3ff;
    printf("[wid=%x,len=%x]\n",wid,len);
    //  firmware only captures to a max length of 0x3ff words
    if (len>0x3ff) len=0x3ff;
    uint32_t* buff = new uint32_t[len];
    char bstr[256];

    printf("timestmp  tag latch  beamclass\n");
    
    for(unsigned i=0; i<len; i++) {
      IndexRange range(i);
      _dump->getVal(&buff[i],1,&range);
      if ((i&3)==3) {
        unsigned index = i&~3;
        sprintf(bstr,"%08x:%08x:%08x",buff[index],buff[index+1],buff[index+2]);
        _dumpline(bstr,buff,index);

        _dumpline(bstr,buff,index+1);
        _dumpline(bstr,buff,index+2);
        printf("%s\n",bstr);
      }
    }

    delete[] buff;
  }
  void _dumpline(char* s, uint32_t* buff, unsigned index) {
    unsigned latch = (buff[index]>>1)&1;
    unsigned tag   = (buff[index]>>2)&0xffff;
    unsigned tstamp = (buff[index]>>18)&0x3fff;
    tstamp |= (buff[index+1]&0x3)<<14;
    uint64_t bclass = (buff[index+3]&3); bclass <<= 32;
    bclass |= buff[index+2]; bclass <<= 30;
    bclass |= buff[index+1]>>2;
    sprintf(s+strlen(s)," | %04x %04x %c %1x:%1x:%1x:%1x:%1x:%1x:%1x:%1x",
            tstamp, tag, latch?'Y':'N', 
            (bclass>> 0)&0xf, (bclass>> 4)&0xf, 
            (bclass>> 8)&0xf, (bclass>>12)&0xf,
            (bclass>>16)&0xf, (bclass>>20)&0xf, 
            (bclass>>24)&0xf, (bclass>>28)&0xf);
  }
};


class MpsDbg {
public:
  MpsDbg(Path p) :
    _destMask(IScalVal::create(p->findByName("DestnRate/MpsDbgDestMask"))),
    _holdOff (IScalVal::create(p->findByName("DestnRate/MpsDbgHoldoff"))),
    _enable  (IScalVal::create(p->findByName("DestnRate/MpsDbgEnable"))),
    _latched (IScalVal_RO::create(p->findByName("DestnRate/MpsDbgLatched"))),
    _ring    (p->findByName("DestnRate/MpsDbgRingBuffer")) 
  {
  }
public:
  void setDest(unsigned d) {
    unsigned v = 1<<d;
    _destMask->setVal(&v);
  }
  void setHoldoff(unsigned v) {
    _holdOff->setVal(&v);
    unsigned q;
    _holdOff->getVal(&q,1);
    printf("holdoff set to %u\n",q);
  }
  void enable() {
    unsigned v =0;
    _destMask->setVal(&v);
    _ring.clear();
    _ring.enable(true);
    sleep(1);
    _enable->getVal(&v,1);
    printf("enable %u\n",v);
    v = 1;
    _enable->setVal(&v);
    _enable->getVal(&v,1);
    printf("enable %u\n",v);
  }
  void poll() {
    unsigned ntries=0;
    while(1) {
      ntries++;
      unsigned v;
      _enable->getVal(&v);
      if (!v) {
        printf("Latched after %u tries\n",ntries);
        break;
      }
      if ((ntries%100)==0)
        printf("Ntries: %u\n",ntries);
      usleep(100000);
    }
  }
  void dump() {
    _ring.dump();
  }
private:
  ScalVal    _destMask;
  ScalVal    _holdOff;
  ScalVal    _enable;
  ScalVal_RO _latched;
  MpsDbgRingBuffer  _ring;
};

void usage(const char* p) {
  printf("Usage: %s [options]\n",p);
  printf("Options: -a <ip address, dotted notation>\n");
  printf("         -y <yaml file>[,<path to timing>]\n");
  printf("         -d <destination>\n");
  printf("         -o <holdoff>\n");
}

int main(int argc, char* argv[])
{
  extern char* optarg;
  char c;

  const char* ip = "10.0.1.102";
  unsigned short port = 8192;
  const char* yaml_file = 0;
  const char* yaml_path = "mmio/AmcCarrierTimingGenerator/ApplicationCore";
  const char* endptr;
  unsigned dest;
  unsigned hoff = 256;
  bool ldump = false;

  while ( (c=getopt( argc, argv, "a:d:o:y:Dh")) != EOF ) {
    switch(c) {
    case 'a':
      ip = optarg;
      break;
    case 'o':
      hoff = strtoul(optarg,NULL,0);
      break;
    case 'y':
      if (strchr(optarg,',')) {
        yaml_file = strtok(optarg,",");
        yaml_path = strtok(NULL,",");
      }
      else
        yaml_file = optarg;
      break;
    case 'd':
      dest = strtoul(optarg,NULL,0);
      break;
    case 'D':
      ldump = true;
      break;
    default:
      usage(argv[0]);
      return 0;
    }
  }

  printf("fixup started\n");

  IYamlFixup* fixup = new IpAddrFixup(ip);
  Path path = IPath::loadYamlFile(yaml_file,"NetIODev",0,fixup);
  delete fixup;

  printf("fixup complete\n");

  MpsDbg* m = new MpsDbg(path->findByName(yaml_path));
  printf("MpsDbg instanciated\n");

  m->setHoldoff(hoff);
  printf("setHoldoff\n");
  m->enable();
  printf("enable\n");
  m->setDest(dest);
  printf("setDest\n");
  if (!ldump)
    m->poll();
  printf("poll\n");
  m->dump();
    
  return 0;
}
