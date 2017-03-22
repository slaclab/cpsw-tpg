#include <SeqMem.hh>

SeqMem ISeqMem::create(const char* name) 
{
  SeqMemImpl v = CShObj::create<SeqMemImpl>(name);
  Field f;

  f = IIntField::create("ram", 32, false, 0);
  v->CMMIODevImpl::addAtAddress(f, 0x0000, 2048);

  return v;
}

CSeqMemImpl::CSeqMemImpl(Key& key, const char* name) : CMMIODevImpl(key, name, 0x2000, LE)
{
}
