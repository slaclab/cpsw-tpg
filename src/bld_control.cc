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
#include <arpa/inet.h>
#include <signal.h>

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
  printf("         -m <channel mask>\n");
}

static int      count = 0;
static int64_t  bytes = 0;
static unsigned lanes = 0;
static Path     core;

static void sigHandler( int signal ) 
{
  psignal(signal, "bld_control received signal");

  unsigned zero(0);
  IScalVal::create(core->findByName("AmcCarrierCore/AmcCarrierBsa/BldAxiStream/Enable"))->setVal((uint32_t*)&zero);
  
  printf("BLD disabled\n");
  ::exit(signal);
}

void* countThread(void* args)
{
  timespec tv;
  clock_gettime(CLOCK_REALTIME,&tv);
  unsigned ocount = count;
  int64_t  obytes = bytes;
  while(1) {
    usleep(1000000);
    timespec otv = tv;
    clock_gettime(CLOCK_REALTIME,&tv);
    unsigned ncount = count;
    int64_t  nbytes = bytes;

    double dt     = double( tv.tv_sec - otv.tv_sec) + 1.e-9*(double(tv.tv_nsec)-double(otv.tv_nsec));
    double rate   = double(ncount-ocount)/dt;
    double dbytes = double(nbytes-obytes)/dt;
    double tbytes = dbytes/rate;
    unsigned dbsc = 0, rsc=0, tbsc=0;
    
    if (count < 0) break;

    static const char scchar[] = { ' ', 'k', 'M' };
    if (rate > 1.e6) {
      rsc     = 2;
      rate   *= 1.e-6;
    }
    else if (rate > 1.e3) {
      rsc     = 1;
      rate   *= 1.e-3;
    }

    if (dbytes > 1.e6) {
      dbsc    = 2;
      dbytes *= 1.e-6;
    }
    else if (dbytes > 1.e3) {
      dbsc    = 1;
      dbytes *= 1.e-3;
    }
    
    if (tbytes > 1.e6) {
      tbsc    = 2;
      tbytes *= 1.e-6;
    }
    else if (tbytes > 1.e3) {
      tbsc    = 1;
      tbytes *= 1.e-3;
    }
    
    printf("Rate %7.2f %cHz [%u]:  Size %7.2f %cBps [%lld B] (%7.2f %cB/evt) lanes %02x\n", 
           rate  , scchar[rsc ], ncount, 
           dbytes, scchar[dbsc], (long long)nbytes, 
           tbytes, scchar[tbsc], 
           lanes);
    lanes = 0;

    ocount = ncount;
    obytes = nbytes;
  }
  return 0;
}
  


int main(int argc, char* argv[])
{
  extern char* optarg;
  char c;

  const char* ip   = "192.168.2.10";
  unsigned short port = 8198;
  const char* yaml_file = 0;
  const char* yaml_path = "mmio/AmcCarrierEmpty";
  unsigned mask = 1;
  const char* endptr;

  while ( (c=getopt( argc, argv, "a:y:m:")) != EOF ) {
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
    case 'm':
      mask = strtoul(optarg,NULL,0);
      break;
    default:
      usage(argv[0]);
      return 0;
    }
  }

  if (!yaml_file) {
    usage(argv[0]);
    return 0;
  }

  IYamlFixup* fixup = new IpAddrFixup(ip);
  Path path = IPath::loadYamlFile(yaml_file,"NetIODev",0,fixup);
  delete fixup;

  core = path->findByName(yaml_path);

  ::signal( SIGINT, sigHandler );

  int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    perror("Open socket");
    return -1;
  }

  sockaddr_in saddr;
  saddr.sin_family = PF_INET;

  if (inet_aton(ip,&saddr.sin_addr) < 0) {
    perror("Converting IP");
    return -1;
  }
    
  saddr.sin_port = htons(port);

  if (connect(fd, (sockaddr*)&saddr, sizeof(saddr)) < 0) {
    perror("Error connecting UDP socket");
    return -1;
  }

  sockaddr_in haddr;
  socklen_t   haddr_len = sizeof(haddr);
  if (getsockname(fd, (sockaddr*)&haddr, &haddr_len) < 0) {
    perror("Error retrieving local address");
    return -1;
  }

  pthread_attr_t tattr;
  pthread_attr_init(&tattr);
  pthread_t thr;
  if (pthread_create(&thr, &tattr, &countThread, &fd)) {
    perror("Error creating read thread");
    return -1;
  }

  //  Set the target address
#if 0
  unsigned lip   = ntohl(haddr.sin_addr.s_addr);
  unsigned lport = ntohs(haddr.sin_port);

  IScalVal::create(core->findByName("AmcCarrierCore/TimingUdpServer[0]/ServerRemoteIp"))->setVal((uint32_t*)&lip);
  IScalVal::create(core->findByName("AmcCarrierCore/TimingUdpServer[0]/ServerRemotePort"))->setVal((uint32_t*)&lport);
#else
  // sockaddr_in saddr;
  // saddr.sin_family      = PF_INET;
  // saddr.sin_addr.s_addr = lip;
  // saddr.sin_port        = lport;

  // if (connect(fd, (sockaddr*)&saddr, sizeof(saddr)) < 0) {
  //   perror("Error connecting UDP socket");
  //   return -1;
  // }

  // "connect" to the sending socket
  char buf[4];
  send(fd,buf,sizeof(buf),0);
#endif    

  unsigned one(-1);
  uint64_t onell(-1ULL);
  unsigned psize(0x3c0);

  IScalVal::create(core->findByName("AmcCarrierCore/AmcCarrierBsa/Bld/channelMask"))->setVal((uint32_t*)&mask);
  //  IScalVal::create(core->findByName("AmcCarrierCore/AmcCarrierBsa/Bld/channelSevr"))->setVal((uint64_t*)&onell);
  IScalVal::create(core->findByName("AmcCarrierCore/AmcCarrierBsa/Bld/packetSize"))->setVal((uint32_t*)&psize);

  {
    uint32_t v;
    IScalVal::create(core->findByName("AmcCarrierCore/AmcCarrierBsa/Bld/packetSize"))->getVal(&v);
    printf("PacketSize: %u words\n");
    IScalVal_RO::create(core->findByName("AmcCarrierCore/AmcCarrierBsa/Bld/currPacketSize"))->getVal(&v);
    printf("WordCount : %u words\n");
  }
    
  IScalVal::create(core->findByName("AmcCarrierCore/AmcCarrierBsa/Bld/enable"))->setVal((uint32_t*)&one);
  
  const unsigned buffsize=8*1024;
  char* buff = new char[buffsize];
  do {
    ssize_t ret = read(fd,buff,buffsize);
    if (ret < 0) break;
    count++;
    bytes += ret;
  } while(1);

  pthread_join(thr,NULL);
  free(buff);

  return 0;
}

