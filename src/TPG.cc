#include <TPG.hh>
#include <SeqState.hh>
#include <SeqJump.hh>
#include <SeqMem.hh>

#define ADD_U1(name,addr,lsbit) {                                       \
    f = IIntField::create(#name, 1, false, lsbit);                      \
    v->CMMIODevImpl::addAtAddress(f, addr);                             \
  }
#define ADD_U3(name,addr,lsbit) {                                       \
    f = IIntField::create(#name, 3, false, lsbit);                      \
    v->CMMIODevImpl::addAtAddress(f, addr);                             \
  }
#define ADD_RO(name,addr,N,lsbit) {                                     \
    f = IIntField::create(#name, N, false, lsbit, IIntField::RO);       \
    v->CMMIODevImpl::addAtAddress(f, addr);                             \
  }
#define ADD_U16(name,addr) {                                            \
    f = IIntField::create(#name, 16, false, 0);                         \
    v->CMMIODevImpl::addAtAddress(f, addr);                             \
  }
#define ADD_U32(name,addr) {                                            \
    f = IIntField::create(#name, 32, false, 0);                         \
    v->CMMIODevImpl::addAtAddress(f, addr);                             \
  }
#define ADD_U64(name,addr) {                                            \
    f = IIntField::create(#name, 64, false, 0);                         \
    v->CMMIODevImpl::addAtAddress(f, addr);                             \
  }
#define ADD_U32R(name,addr) ADD_RO(name,addr,32,0)

#define ADD_64U(name,addr,nbits,lsbit) {                                \
    f = IIntField::create(#name, nbits, false, lsbit);                  \
    v->CMMIODevImpl::addAtAddress(f, addr, 64, 8);                     \
  }

TPG ITPG::create(const char* name) 
{
  TPGImpl v = CShObj::create<TPGImpl>(name);
  Field f;

  ADD_RO (SeqAddrLen ,0x0000,4,0);
  ADD_RO (NBeamSeq   ,0x0000,6,4);
  ADD_RO (NAllowSeq  ,0x0001,6,2);
  ADD_RO (NControlSeq,0x0002,8,0);
  ADD_RO (NArraysBsa ,0x0003,8,0);

  ADD_U32(ClockPeriod,0x0004);
  ADD_U32(BaseControl,0x0008);
  ADD_U16(ACDelay    ,0x000C);
  ADD_U16(FrameDelay ,0x000E);
  ADD_U64(PulseId    ,0x0010);
  ADD_U64(TStamp     ,0x0018);

  f = IIntField::create("ACRateDiv", 8, false, 0);
  v->CMMIODevImpl::addAtAddress(f, 0x0020, 6, 1);

  f = IIntField::create("FixedRateDiv", 32, false, 0);
  v->CMMIODevImpl::addAtAddress(f, 0x0040, 10, 4);

  ADD_U1 (RateReload  ,0x0068,0);
  ADD_U32(DiagSeq     ,0x006c);
  ADD_U1 (GenStatus   ,0x0070,0);
  ADD_U32(IrqControl  ,0x0074);
  ADD_U32(IrqStatus   ,0x0078);
  ADD_U32(SeqFifo     ,0x007c);

  f = IIntField::create("SeqReqd", 16, false, 0);
  v->CMMIODevImpl::addAtAddress(f, 0x0080, 16, 4);

  f = IIntField::create("SeqDestn", 4, false, 0);
  v->CMMIODevImpl::addAtAddress(f, 0x0082, 16, 4);

  ADD_U64(SeqRestart ,0x100);

  f = IIntField::create("Energy", 32, false, 0);
  v->CMMIODevImpl::addAtAddress(f, 0x0120, 4, 4);

  ADD_U1(CtrLock,0x17c,0);
  f = IIntField::create("CtrDef", 32, false, 0);
  v->CMMIODevImpl::addAtAddress(f, 0x180, 24, 8);

  ADD_U64(BsaComplete,0x01f8);

  ADD_U32(BeamDiagControl,0x01e4);

  f = IIntField::create("BeamDiagStatus", 32, false, 0);
  v->CMMIODevImpl::addAtAddress(f, 0x01e8, 4, 4);

  ADD_64U(BsaEventSel   ,0x0200,32,0);
  ADD_64U(BsaStatSel    ,0x0204,32,0);

  f = IIntField::create("BsaStat", 32, false, 0);
  v->CMMIODevImpl::addAtAddress(f, 0x400, 32, 4);
    //  ADD_64U(BsaStat       ,0x0400,32,0);

  ADD_U32R(PllCnt       ,0x0500);
  ADD_U32R(ClkCnt       ,0x0504);
  ADD_U32R(SyncErrCnt   ,0x0508);
  ADD_U32 (CountInterval,0x050c);
  ADD_U32R(BaseRateCount,0x0510);

  f = IIntField::create("CountTrig", 32, false, 0, IIntField::RO);
  v->CMMIODevImpl::addAtAddress(f, 0x0514, 12);

  f = IIntField::create("CountSeq", 32, false, 0, IIntField::RO);
  v->CMMIODevImpl::addAtAddress(f, 0x0544, 64);

  SeqState state = ISeqState::create("seqstate");
  v->CMMIODevImpl::addAtAddress(state, 0x800, 64); 
  
  SeqJump jump = ISeqJump::create("seqjump");
  v->CMMIODevImpl::addAtAddress(jump, 0x1000, 64); 

  SeqMem mem = ISeqMem::create("seqmem");
  v->CMMIODevImpl::addAtAddress(mem, 0x2000, 64); 

  return v;
}

CTPGImpl::CTPGImpl(Key& key, const char* name) : CMMIODevImpl(key, name, 0x10000000, LE)
{
}
