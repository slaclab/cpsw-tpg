
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
unsigned AxiVersion::upTime() const
{
  unsigned v;
  IScalVal_RO::create(_root->findByName("UpTimeCnt"))->getVal(&v);
  return v;
}
void AxiVersion::reset()
{
  unsigned v = 1;
  IScalVal::create(_root->findByName("MasterReset"))->setVal(&v);
}
void AxiVersion::reload()
{
  unsigned v = 1;
  IScalVal::create(_root->findByName("FpgaReload"))->setVal(&v);
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
  _dump(IScalVal::create(root->findByName("dump")))
{
}

void RingBuffer::enable(bool v)
{
  unsigned r;
  _csr->getVal(&r);
  if (v)
    r |= (1<<31);
  else
    r &= ~(1<<31);
  _csr->setVal(&r);
}

void RingBuffer::clear()
{
  unsigned r;
  _csr->getVal(&r);
  r |= (1<<30);
  _csr->setVal(&r);
  r &= ~(1<<30);
  _csr->setVal(&r);
}

void RingBuffer::dump()
{
  unsigned len;
  _csr->getVal(&len);
  unsigned wid = (len>>20)&0xff;
  len &= (1<<wid)-1;
  printf("[wid=%x,len=%x]\n",wid,len);
  //  firmware only captures to a max length of 0x3ff words
  if (len>0x3ff) len=0x3ff;
  uint32_t* buff = new uint32_t[len];
  //  _dump->getVal(buff,len);
  //  for(unsigned i=0; i<len; i++)
  //    printf("%08x%c", buff[i], (i&0x7)==0x7 ? '\n':' ');
  //  printf("%08x\n", _dump->getVal(buff));

  for(unsigned i=0; i<len; i++) {
    IndexRange range(i);
    _dump->getVal(&buff[i],1,&range);
    printf("%08x%c", buff[i], (i&0x7)==0x7 ? '\n':' ');
  }

  delete[] buff;
}

void RingBuffer::clear_and_dump()
{
  enable(false);
  clear();
  enable(true);
  usleep(100);
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

#define PR(r,u) { IScalVal_RO::create(_timingRx->findByName(#r))->getVal(&u); }
  
  PR(RxClkCount   ,s.rxclks);
  PR(TxClkCount   ,s.txclks);
  PR(sofCount     ,s.sof);
  PR(RxLinkUp     ,s.linkup);
  PR(RxPolarity   ,s.polarity);
  PR(CrcErrCount  ,s.crc);
  PR(RxDspErrCount,s.dsp);
  PR(RxDspErrCount,s.dec);
#undef PR

  return s;
}

void AmcTiming::dumpStats() const
{
#define PR(r) { unsigned u;                                     \
    IScalVal_RO::create(_timingRx->findByName(#r))->getVal(&u);   \
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
  IScalVal::create(_alignRx->findByName("PhaseTarget"))->setVal(&t);
}

void AmcTiming::setRxResetLength(unsigned len)
{
  IScalVal::create(_alignRx->findByName("ResetLen"))->setVal(&len);
}

void AmcTiming::dumpRxAlign     () const
{
  unsigned tgt,last,rlen;
  IScalVal_RO::create(_alignRx->findByName("PhaseTarget"))->getVal(&tgt);
  IScalVal_RO::create(_alignRx->findByName("LastPhase"))  ->getVal(&last);
  IScalVal_RO::create(_alignRx->findByName("ResetLen"))   ->getVal(&rlen);
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
    IScalVal_RO::create(_timingRx->findByName("RxClkCount"))->getVal(&rxT0);
    IScalVal_RO::create(_timingRx->findByName("TxClkCount"))->getVal(&txT0);
    usleep(1000000);

    unsigned rxT1, txT1;
    IScalVal_RO::create(_timingRx->findByName("RxClkCount"))->getVal(&rxT1);
    IScalVal_RO::create(_timingRx->findByName("TxClkCount"))->getVal(&txT1);
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


static int row_, column_;

GthEyeScan::GthEyeScan(Path root) :
  _root(root->findByName("GthEyeScan"))
{
}

bool GthEyeScan::enabled() const
{
  unsigned v;
  IScalVal_RO::create(_root->findByName("Enable"))->getVal(&v);
  return v!=0;
}

void GthEyeScan::enable(bool v)
{
  unsigned u = v ? 1:0;
  IScalVal::create(_root->findByName("Enable"))->setVal(&u);
}

void GthEyeScan::scan(const char* ofile, 
                      unsigned    prescale, 
                      unsigned    xscale,
                      bool        lsparse)
{
  FILE* f = fopen(ofile,"w");

  unsigned status;
  unsigned zero(0), one(1);
  IScalVal_RO::create(_root->findByName("ScanState"))->getVal(&status);
  printf("eyescan status: %04x\n",status);
  if (status != 0) {
    printf("Forcing to WAIT state\n");
    IScalVal::create(_root->findByName("Run"))->setVal(&zero);
  }
  while(1) {
    sleep(1);
    unsigned vdone, vstate;
    IScalVal_RO::create(_root->findByName("ScanDone" ))->getVal(&vdone);
    IScalVal_RO::create(_root->findByName("ScanState"))->getVal(&vstate);
    printf("ScanState/Done = %u.%u\n",vstate,vdone);
    if (vdone==1 && vstate==0)
      break;
  }
  printf("WAIT state\n");

  IScalVal::create(_root->findByName("ErrDetEn"))->setVal(&one);
  IScalVal::create(_root->findByName("Prescale"))->setVal(&prescale);

  IScalVal_RO::create(_root->findByName("ErrDetEn"))->getVal(&status);
  printf("ErrDet %u\n",status);

  IScalVal_RO::create(_root->findByName("Prescale"))->getVal(&status);
  printf("Prescale %u\n",status);

  for(unsigned i=0; i<5; i++) {
    IndexRange range(i);
    unsigned data,qual;
    IScalVal_RO::create(_root->findByName("SDataMask"))->getVal(&data,1,&range);
    IScalVal_RO::create(_root->findByName("QualMask" ))->getVal(&qual,1,&range);
    printf("data,qual [%u] : %x %x\n", i, data, qual);
  }

  unsigned sdata_mask[] = { 0xffff, 0xffff, 0xff00, 0x000f, 0xffff };
  IScalVal::create(_root->findByName("SDataMask"))->setVal(sdata_mask,5);

  unsigned qual_mask[] = { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff };
  IScalVal::create(_root->findByName("QualMask"))->setVal(qual_mask,5);

  for(unsigned i=0; i<5; i++) {
    IndexRange range(i);
    unsigned data,qual;
    IScalVal_RO::create(_root->findByName("SDataMask"))->getVal(&data,1,&range);
    IScalVal_RO::create(_root->findByName("QualMask" ))->getVal(&qual,1,&range);
    printf("data,qual [%u] : %x %x\n", i, data, qual);
  }

  unsigned rang = 3;
  IScalVal::create(_root->findByName("EsVsRange" ))->setVal(&rang);
  IScalVal::create(_root->findByName("EsVsUtSign"))->setVal(&zero);
  IScalVal::create(_root->findByName("EsVsNegDir"))->setVal(&zero);

  ScalVal horzOffset = IScalVal::create(_root->findByName("HorzOffset"));
  horzOffset->setVal(&zero);

  char stime[200];

  for(int j=-31; j<32; j++) {
    row_ = j;

    time_t t = time(NULL);
    struct tm* tmp = localtime(&t);
    if (tmp)
      strftime(stime, sizeof(stime), "%T", tmp);

    unsigned hoff = j<<xscale;
    horzOffset->setVal(&hoff);
    horzOffset->getVal(&status);
    printf("es_horz_offset: %i [%s] (%x)\n",j, stime, status);

    ScalVal code = IScalVal::create(_root->findByName("VertOffset"));
    code->setVal(&zero); // zero vert offset

    unsigned error_count=-1, error_count_p=-1;

    for(int i=-1; i>=-127; i--) {
      column_ = i;
      code->setVal((unsigned*)&i); // vert offset
      unsigned sample_count;
      run(error_count,sample_count);
      uint64_t sample_count64 = uint64_t(sample_count) << (1 + prescale);

      fprintf(f, "%d %d %u %llu\n",
              j, i, 
              error_count,
              sample_count64);
                
      // -> wait
      IScalVal::create(_root->findByName("Run"))->setVal(&zero);

      if (error_count==0 && error_count_p==0 && !lsparse) {
        //          printf("\t%i\n",i);
        break;
      }

      error_count_p=error_count;

      if (lsparse)
        i -= 19;
    }
    code->setVal(&zero); // zero vert offset
    error_count_p = -1;
    for(int i=127; i>=0; i--) {
      column_ = i;
      code->setVal((unsigned*)&i); // vert offset
      unsigned sample_count;
      run(error_count,sample_count);
      uint64_t sample_count64 = uint64_t(sample_count) << (1 + prescale);

      fprintf(f, "%d %d %u %llu\n",
              j, i, 
              error_count,
              sample_count64);
                
      // -> wait
      IScalVal::create(_root->findByName("Run"))->setVal(&zero);

      if (error_count==0 && error_count_p==0 && !lsparse) {
        //          printf("\t%i\n",i);
        break;
      }

      error_count_p=error_count;

      if (lsparse)
        i -= 19;
    }
    if (lsparse)
      j += 3;
  }
  fclose(f);
}

void GthEyeScan::run(unsigned& error_count,
                     unsigned& sample_count)
{
  unsigned zero(0), one(1);

  // -> wait
  IScalVal::create(_root->findByName("Run"))->setVal(&one);
  ScalVal_RO done = IScalVal_RO::create(_root->findByName("ScanDone"));
  unsigned vdone;
  unsigned nwait=0;
  while(1) {
    do {
      usleep(100);
      nwait++;
      done->getVal(&vdone);
    } while(vdone==0 and nwait < 1000);
    unsigned vstate;
    IScalVal_RO::create(_root->findByName("ScanState"))->getVal(&vstate);
    if (vstate==2)
      break;
  }
  IScalVal_RO::create(_root->findByName("ErrorCount"))->getVal(&error_count);
  IScalVal_RO::create(_root->findByName("SampleCount"))->getVal(&sample_count);
}            

void GthEyeScan::progress(unsigned& row,
                          unsigned& col)
{
  row = row_;
  col = column_;
}

IpAddrFixup::~IpAddrFixup() {}

void IpAddrFixup::operator()(YAML::Node& node, YAML::Node& dummy)
{
  writeNode(node, YAML_KEY_ipAddr, _ip);
}
