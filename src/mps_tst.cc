//
//  List of features to test:
//
//    Sequence engine - sync (fixed, AC+timeslot mask), branch, jump
//                    - reset, multi-engine reset
//                    - destination priority
//                    - triggered jump (triggered sync?)
//                    - bcs/mps fault jump
//                    - notify callback
//            
//    Counters        - interval updates
//                    - interval callback
//
//    Frame data      - content
//                    - FIFO dump
//
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>

#include <cpsw_api_builder.h>
#include <cpsw_api_user.h>

static void usage(const char* p)
{
  printf("Usage: %s [options]\n"
         "  -a <ip>\n",p);
}

int main(int argc, char* argv[])
{
  const char* ip = "192.168.2.10";
  const char* yaml = 0;
  char* endptr = 0;

  unsigned latch_tag=0;

  unsigned pc=0;
  int      dst=-1;

  opterr = 0;

  char opts[32];
  sprintf(opts,"a:y:C:L:h");

  int c;
  while( (c=getopt(argc,argv,opts))!=-1 ) {
    switch(c) {
    case 'a':
      ip = optarg;
      break;
    case 'y':
      yaml = optarg;
      break;
    case 'C':
      dst = strtoul(optarg,&endptr,0);
      pc  = strtoul(endptr+1,&endptr,0);
      break;
    case 'L':
      latch_tag = strtoul(optarg,NULL,0);
      break;
    case 'h':
      usage(argv[0]); return 1;
    default:
      break;
    }
  }

  Path path;

  if (!yaml) {
    NetIODev  root = INetIODev::create("fpga", ip);
    ProtoStackBuilder bldr = IProtoStackBuilder::create();
    bldr->setSRPVersion              ( IProtoStackBuilder::SRP_UDP_NONE );
    bldr->setUdpPort                 (                  8192 );
    bldr->useDepack                  (                  false );
    bldr->useRssi                    (                  false );

    MMIODev   mmio = IMMIODev::create ("mmio", (1ULL<<32));
    Field f;

    //  name, bits, false, lsbit
    f = IIntField::create("LatchDiag", 1, false, 0);
    //  field, address, Nelm, stride
    mmio->addAtAddress(f, 0x82000000);

    f = IIntField::create("Strobe", 1, false, 7);
    mmio->addAtAddress(f, 0x82000003);

    f = IIntField::create("Tag", 16, false, 0);
    mmio->addAtAddress(f, 0x82000004);

    f = IIntField::create("TimeStamp", 16, false, 0);
    mmio->addAtAddress(f, 0x82000006);

    f = IIntField::create("Class", 4, false, 0);
    mmio->addAtAddress(f, 0x82000010, 16, 1);

    root->addAtAddress(mmio, bldr);

    Path p = IPath::create(root);
    path = p->findByName("mmio");
  }
  else {
    Path p = IPath::loadYamlFile(yaml,"NetIODev");
    path = p->findByName("mmio/AmcCarrierTimingGenerator/ApplicationCore/TPGMps/MpsSim");
  }

  unsigned one(1),zero(0);

  if (dst >= 0) {
    IndexRange rng(dst);
    IScalVal::create(path->findByName("Class"))->setVal(&pc,1,&rng);
  }

  IScalVal::create(path->findByName("Tag"))->setVal(&latch_tag);
  IScalVal::create(path->findByName("LatchDiag"))->setVal(latch_tag ? &one : &zero);

  if (latch_tag || (dst>=0)) {
    IScalVal::create(path->findByName("Strobe"))->setVal(&one);
  }

  unsigned latch, tag, timestamp;
  unsigned pclass[16];

  IScalVal_RO::create(path->findByName("LatchDiag"))->getVal(&latch);
  IScalVal_RO::create(path->findByName("Tag"      ))->getVal(&tag);
  IScalVal_RO::create(path->findByName("TimeStamp"))->getVal(&timestamp);
  for(unsigned i=0; i<16; i++) {
    IndexRange rng(i);
    IScalVal_RO::create(path->findByName("Class"))->getVal(&pclass[i],1,&rng);
  }

  printf("Latch [%u]  Tag [%x]  Timestamp[%x]\n",
         latch, tag, timestamp );
  for(unsigned i=0; i<16; i++)
    printf("  c%u[%x]", i, pclass[i]);
  printf("\n");
  
  return 0;
}
