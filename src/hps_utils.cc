
#include <unistd.h>

#include <new>

#include <cpsw_api_user.h>
#include <cpsw_yaml_keydefs.h>
#include <cpsw_yaml.h>

#include "hps_utils.hh"

using namespace Cphw;

AxiVersion::AxiVersion(Path root) : _root(root) {}
std::string AxiVersion::buildStamp() const
{
  char buff[256];
  IScalVal_RO::create(_root->findByName("BuildStamp"))
    ->getVal(reinterpret_cast<uint8_t*>(buff),256);
  buff[255]=0;
  return std::string(buff);
}

XBar::XBar(Path root) : _root(root) {}
void XBar::setOut( Map out, Map in )
{
  unsigned uin(in);
  unsigned uout(out);
  IndexRange rng(uout);
  IScalVal::create(_root->findByName("OutputConfig"))->setVal(&uin,1,&rng);
}
void XBar::dump  () const
{
  static const char* src[] = {"EVR0", "FPGA", "BP", "EVR1"};
  unsigned outMap[4];
  IScalVal_RO::create(_root->findByName("OutputConfig"))->getVal(outMap,4);
  for(unsigned i=0; i<4; i++)
    printf("OUT[%u (%4s)] = %u (%4s)\n", i, src[i], outMap[i], src[outMap[i]]);
}

RingBuffer::RingBuffer(Path root) :
  _csr (IScalVal::create(root->findByName("csr"))),
  _dump(IScalVal::create(root->findByName("csr")))
{
}

void RingBuffer::enable(bool v)
{
  unsigned r;
  _csr->getVal(&r,1);
  if (v)
    r |= (1<<31);
  else
    r &= ~(1<<31);
  _csr->setVal(&r,1);
}

void RingBuffer::clear()
{
  unsigned r;
  _csr->getVal(&r,1);
  r |= (1<<30);
  _csr->setVal(&r,1);
  r &= ~(1<<30);
  _csr->setVal(&r,1);
}

void RingBuffer::dump()
{
  unsigned len;
  _csr->getVal(&len,1);
  len &= 0xfffff;
  uint32_t* buff = new uint32_t[len];
  _dump->getVal(buff,len);
  for(unsigned i=0; i<len; i++)
    printf("%08x%c", buff[i], (i&0x7)==0x7 ? '\n':' ');
}

void RingBuffer::clear_and_dump()
{
  enable(false);
  clear();
  enable(true);
  usleep(10);
  enable(false);
  dump();
}

AmcTiming::AmcTiming(Path root) : 
  _root(root), 
  _timingRx(root->findByName("TimingFrameRx")),
  _alignRx (root->findByName("GthRxAlignCheck"))
{
}

void AmcTiming::setPolarity(bool inverted)
{
  unsigned u = inverted ? 1:0;
  IScalVal::create(_timingRx->findByName("RxPolarity"))->setVal(&u);
}

void AmcTiming::setLCLS()
{
  unsigned u = 0;
  IScalVal::create(_timingRx->findByName("ClkSel"))->setVal(&u);
}

void AmcTiming::setLCLSII()
{
  unsigned u = 1;
  IScalVal::create(_timingRx->findByName("ClkSel"))->setVal(&u);
}

void AmcTiming::bbReset()
{
  unsigned u=1;
  IScalVal::create(_timingRx->findByName("RxReset"))->setVal(&u);
  usleep(10);
  u=0;
  IScalVal::create(_timingRx->findByName("RxReset"))->setVal(&u);
}

void AmcTiming::resetStats()
{
  unsigned u=1;
  IScalVal::create(_timingRx->findByName("RxCountReset"))->setVal(&u);
  usleep(10);
  u=0;
  IScalVal::create(_timingRx->findByName("RxCountReset"))->setVal(&u);
}

TimingStats AmcTiming::getStats() const
{
  TimingStats s;

#define PR(r,u) { IScalVal_RO::create(_timingRx->findByName(#r))->getVal(&u,1); }
  
  PR(RxClkCount   ,s.rxclks);
  PR(TxClkCount   ,s.txclks);
  PR(sofCount     ,s.sof);
  PR(RxLinkUp     ,s.linkup);
  PR(CrcErrCount  ,s.crc);
  PR(RxDspErrCount,s.dsp);
  PR(RxDspErrCount,s.dec);
#undef PR

  return s;
}

void AmcTiming::dumpStats() const
{
#define PR(r) { unsigned u;                                     \
    IScalVal_RO::create(_timingRx->findByName(#r))->getVal(&u,1);   \
    printf("%10.10s: 0x%x\n",#r,u); }
  
  PR(sofCount);
  PR(eofCount);
  PR(FidCount);
  PR(CrcErrCount);
  PR(RxClkCount);
  PR(RxRstCount);
  PR(RxDecErrCount);
  PR(RxDspErrCount);
  PR(RxLinkUp);
  PR(RxPolarity);
  PR(ClkSel);
  PR(VersionErr);
  PR(MsgDelay);
  PR(TxClkCount);
  PR(BypassDoneCount);
#undef PR
}

void AmcTiming::setRxAlignTarget(unsigned t)
{
  IScalVal::create(_alignRx->findByName("PhaseTarget"))->setVal(&t,1);
}

void AmcTiming::setRxResetLength(unsigned len)
{
  IScalVal::create(_alignRx->findByName("ResetLen"))->setVal(&len,1);
}

void AmcTiming::dumpRxAlign     () const
{
  unsigned tgt,last,rlen;
  IScalVal_RO::create(_alignRx->findByName("PhaseTarget"))->getVal(&tgt,1);
  IScalVal_RO::create(_alignRx->findByName("LastPhase"))  ->getVal(&last,1);
  IScalVal_RO::create(_alignRx->findByName("ResetLen"))   ->getVal(&rlen,1);
  printf("\nTarget: %u\tRstLen: %u\tLast: %u\n",
         tgt, rlen, last);

  unsigned* align = new unsigned[40];
  IScalVal_RO::create(_alignRx->findByName("PhaseCount"))->getVal(align,40);
  for(unsigned i=0; i<40; i++) {
    printf(" %04x",align[i]);
    if ((i%10)==9) printf("\n");
  }
  printf("\n");
}

void AmcTiming::measureClks() const
{
  double n=0;
  double rx1=0, rx2=0;
  double tx1=0, tx2=0;

  while(1) {
    unsigned rxT0, txT0;
    IScalVal_RO::create(_timingRx->findByName("RxClkCount"))->getVal(&rxT0,1);
    IScalVal_RO::create(_timingRx->findByName("TxClkCount"))->getVal(&txT0,1);
    usleep(1000000);

    unsigned rxT1, txT1;
    IScalVal_RO::create(_timingRx->findByName("RxClkCount"))->getVal(&rxT1,1);
    IScalVal_RO::create(_timingRx->findByName("TxClkCount"))->getVal(&txT1,1);
    double dRxT = double((rxT1 - rxT0) << 4);
    double dTxT = double((txT1 - txT0) << 4);

    n++; 
    rx1 += dRxT; rx2 += dRxT*dRxT;
    tx1 += dTxT; tx2 += dTxT*dTxT;
    printf("\n");
    double rxm = rx1/n;
    double rxs = n>1 ? sqrt((rx2-rxm*rx1)/(n-1)) : 0;
    printf("%10.10s: mean %f  rms %f\n","1S dT(rx)", rxm*1.e-6, rxs*1.e-6);
    double txm = tx1/n;
    double txs = n>1 ? sqrt((tx2-txm*tx1)/(n-1)) : 0;
    printf("%10.10s: mean %f  rms %f\n","1S dT(rx)", txm*1.e-6, txs*1.e-6);
  }
}

void IpAddrFixup::operator()(YAML::Node& node)
{
  writeNode(node, YAML_KEY_ipAddr, _ip);
}
