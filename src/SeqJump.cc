#include <SeqJump.hh>

SeqJump ISeqJump::create(const char* name) 
{
  SeqJumpImpl v = CShObj::create<SeqJumpImpl>(name);
  Field f;

  f = IIntField::create("StartAddr", 12, false, 0);
  v->CMMIODevImpl::addAtAddress(f, 0x0000, 16, 4);

  f = IIntField::create("Class", 4, false, 4);
  v->CMMIODevImpl::addAtAddress(f, 0x0001, 16, 4);

  f = IIntField::create("StartSync", 16, false, 0);
  v->CMMIODevImpl::addAtAddress(f, 0x0002, 16, 4);

  return v;
}

CSeqJumpImpl::CSeqJumpImpl(Key& key, const char* name) : CMMIODevImpl(key, name, 0x40, LE)
{
}
