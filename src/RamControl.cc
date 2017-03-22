#include <RamControl.hh>
#include <AxiStreamDmaRingWrite.hh>

using namespace TPGen;

RamControl IRamControl::create(const char* name,
                               unsigned    nbuf) 
{
  RamControlImpl v = CShObj::create<RamControlImpl>(name);
  Field f;
  f = IIntField::create("TimeStamp", 64, false, 0, IIntField::RO);
  v->CMMIODevImpl::addAtAddress(f, 0x1000, nbuf, 8);

  AxiStreamDmaRingWrite r = IAxiStreamDmaRingWrite::create("ring",nbuf);
  v->CMMIODevImpl::addAtAddress(r, 0x1000);

  return v;
}

CRamControlImpl::CRamControlImpl(Key& key, const char* name) : CMMIODevImpl(key, name, 0x2000, LE)
{
}
