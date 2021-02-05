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

#include <cpsw_api_builder.h>
#include <cpsw_mmio_dev.h>
#include <cpsw_proto_mod_depack.h>

#include <string>

//#define USE_STREAM

extern int optind;

void usage(const char* p) {
  printf("Usage: %s [-a <IP addr (dotted notation)>] [-p <port>] [-w]\n",p);
}

static bool lWrite = false;
static unsigned reg=0, word=0;

static void* write_thread(void* arg)
{
  Stream* pstrm = (Stream*)(arg);
  Stream strm = *pstrm;

  uint8_t buf [1500];
  unsigned sz;
  {
    CAxisFrameHeader hdr;
    hdr.insert(buf, sizeof(buf));
    hdr.iniTail(buf + hdr.getSize()+0x8);
    sz = hdr.getSize()+hdr.getTailSize()+0x8;
    uint32_t* bb = reinterpret_cast<uint32_t*>(&buf[hdr.getSize()]);
    bb[0] = (reg<<1) | (lWrite ? 0:1);
    bb[1] = word;
    strm->write( (uint8_t*)buf, sz);
  }

  while(1) {
    usleep(100000);
  
    strm->write( (uint8_t*)buf, sz);
  }  
}

int main(int argc, char** argv) {

  extern char* optarg;

  int c;
  bool lUsage = false;

  const char* ip = "10.0.2.103";
  unsigned short port = 8194;
  unsigned tdest=0;

  while ( (c=getopt( argc, argv, "a:d:p:r:w:h")) != EOF ) {
    switch(c) {
    case 'a':
      ip = optarg; break;
    case 'd':
      tdest = strtoul(optarg,NULL,0); break;
    case 'p':
      port = strtoul(optarg,NULL,0); break;
    case 'r':
      reg  = strtoul(optarg,NULL,0); break;
    case 'w':
      lWrite = true; word = strtoul(optarg,NULL,0); break;
    case '?':
    default:
      lUsage = true; break;
    }
  }

  if (lUsage) {
    usage(argv[0]);
    exit(1);
  }

  NetIODev  root = INetIODev::create("fpga", ip);

  {  // Streaming access
    ProtoStackBuilder bldr = IProtoStackBuilder::create();
    bldr->setSRPVersion              ( IProtoStackBuilder::SRP_UDP_NONE );
    bldr->setUdpPort                 (                  port );
    bldr->setSRPTimeoutUS            (                 90000 );
    bldr->setSRPRetryCount           (                     5 );
    //    bldr->setSRPMuxVirtualChannel    (                     0 );
    bldr->useDepack                  (                  true );
    bldr->useRssi                    (                 false );
    bldr->setTDestMuxTDEST           (                 tdest );

    Field    irq = IField::create("irq");
    root->addAtAddress( irq, bldr );
  }

  Path path = IPath::create(root);

  Stream strm = IStream::create( path->findByName("irq") );
  CTimeout         tmo(100000);

  //  pthread_attr_t attr;
  //  pthread_attr_init(&attr);
  //  pthread_t rthread;
  //  pthread_create(&rthread, &attr, &write_thread, (void*)&strm);

  uint8_t ibuf[256];
  int v;

  while ((v=strm->read( ibuf, sizeof(ibuf), tmo, 0 ))>=0) {

    if (v) {

      CAxisFrameHeader hdr;
      if (!hdr.parse(ibuf, sizeof(ibuf))) {
        printf("bad header\n");
        continue;
      }

      const uint32_t* bb = reinterpret_cast<const uint32_t*>(&ibuf[hdr.getSize()]);
      printf("Frame %u  %04x:%04x\n",
             hdr.getFrameNo(),
             bb[0], bb[1]);
    }
  }

  return 0;
}
