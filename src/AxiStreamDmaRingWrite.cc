#include <AxiStreamDmaRingWrite.hh>

#define ADD_U64(name,addr) {                                            \
    f = IIntField::create(#name, 64);                                   \
    v->CMMIODevImpl::addAtAddress(f, addr, nbuf, 8);                    \
  }
#define ADD_U64R(name,addr) {                                           \
    f = IIntField::create(#name, 64, false, 0, IIntField::RO);          \
    v->CMMIODevImpl::addAtAddress(f, addr, nbuf, 8);                    \
  }
#define ADD_U1(name,addr,lsbit) {                                       \
    f = IIntField::create(#name, 1, false, lsbit);                      \
    v->CMMIODevImpl::addAtAddress(f, addr, nbuf, 4);                    \
  }

using namespace TPGen;

AxiStreamDmaRingWrite IAxiStreamDmaRingWrite::create(const char* name,
                                                     unsigned    nbuf)
{
  AxiStreamDmaRingWriteImpl v = CShObj::create<AxiStreamDmaRingWriteImpl>(name);
  Field f;

  ADD_U64 (StartAddr  ,0x000);
  ADD_U64 (EndAddr    ,0x200);
  ADD_U64R(WrAddr     ,0x400);
  ADD_U64R(TriggerAddr,0x600);

  ADD_U1(Enabled     ,0x800,0);
  ADD_U1(Mode        ,0x800,1);
  ADD_U1(Init        ,0x800,2);
  ADD_U1(SoftTrigger ,0x800,3);

  f = IIntField::create("MsgDest", 4, false, 4);
  v->CMMIODevImpl::addAtAddress(f, 0x800, nbuf, 4);
  f = IIntField::create("FramesAfterTrigger", 16, false, 0);
  v->CMMIODevImpl::addAtAddress(f, 0x802, nbuf, 4);

  ADD_U1(Empty       ,0xa00,0);
  ADD_U1(Full        ,0xa00,1);
  ADD_U1(Done        ,0xa00,2);
  ADD_U1(Triggered   ,0xa00,3);
  ADD_U1(Error       ,0xa00,4);

  f = IIntField::create("BurstSize", 4, false, 0, IIntField::RO);
  v->CMMIODevImpl::addAtAddress(f, 0xA01);

  f = IIntField::create("FramesSinceTrigger", 16, false, 0, IIntField::RO);
  v->CMMIODevImpl::addAtAddress(f, 0xA01, nbuf, 4);
  return v;
}

CAxiStreamDmaRingWriteImpl::CAxiStreamDmaRingWriteImpl(Key& key, const char* name) : CMMIODevImpl(key, name, 0x1000, LE)
{
}
