#include <RssiCore.hh>

using namespace TPGen;

#define ADD_U1(name,addr,lsbit) {                                       \
    f = IIntField::create(#name, 1, false, lsbit);                      \
    v->CMMIODevImpl::addAtAddress(f, addr);                             \
  }

#define ADD_UN(name,addr,nbits) {                                       \
    f = IIntField::create(#name, nbits, false, 0, IIntField::RO);       \
    v->CMMIODevImpl::addAtAddress(f, addr);                             \
  }

RssiCore IRssiCore::create(const char* name)
{
  RssiCoreImpl v = CShObj::create<RssiCoreImpl>(name);
  Field f;

  ADD_U1(OpenConn        ,0x00,0);
  ADD_U1(CloseConn       ,0x00,1);
  ADD_U1(Mode            ,0x00,2);
  ADD_U1(HeaderChkSumEn  ,0x00,3);
  ADD_U1(InjectFault     ,0x00,4);

  ADD_UN(InitSeqN        ,0x04,8);
  ADD_UN(Version         ,0x08,4);
  ADD_UN(MaxOutsSet      ,0x0c,8);
  ADD_UN(MaxSegSize      ,0x10,16);
  ADD_UN(RetransTimeout  ,0x14,16);
  ADD_UN(CumAckTimeout   ,0x18,16);
  ADD_UN(NullSegTimeout  ,0x1c,16);
  ADD_UN(MaxNumRetrans   ,0x20, 8);
  ADD_UN(MaxCumAck       ,0x24, 8);
  ADD_UN(MaxOutOfSeq     ,0x28, 8);
  ADD_U1(ConnectionActive,0x40, 0);
  ADD_U1(ErrMaxRetrans   ,0x40, 1);
  ADD_U1(ErrNullTout     ,0x40, 2);
  ADD_U1(ErrAck          ,0x40, 3);
  ADD_U1(ErrSsiFrameLen  ,0x40, 4);
  ADD_U1(ErrConnTout     ,0x40, 5);
  ADD_U1(ParamRejected   ,0x40, 6);
  ADD_UN(ValidCnt        ,0x44,32);
  ADD_UN(DropCnt         ,0x48,32);
  ADD_UN(RetransmitCnt   ,0x4c,32);
  ADD_UN(ReconnectCnt    ,0x50,32);

  f = IIntField::create("FrameRate", 32, false, 0, IIntField::RO);
  v->CMMIODevImpl::addAtAddress(f, 0x54, 2, 4);

  f = IIntField::create("Bandwidth" ,64, false, 0, IIntField::RO);
  v->CMMIODevImpl::addAtAddress(f, 0x5C, 2, 8);

  return v;
}

CRssiCoreImpl::CRssiCoreImpl(Key& key, const char* name) : CMMIODevImpl(key, name, 0x2000, LE)
{
}
