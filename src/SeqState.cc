#include <SeqState.hh>

SeqState ISeqState::create(const char* name) 
{
  SeqStateImpl v = CShObj::create<SeqStateImpl>(name);
  Field f;

  f = IIntField::create("Addr", 12, false, 0);
  v->CMMIODevImpl::addAtAddress(f, 0x0000);

  f = IIntField::create("CondC", 8, false, 0);
  v->CMMIODevImpl::addAtAddress(f, 0x0004, 4, 1);

  return v;
}

CSeqStateImpl::CSeqStateImpl(Key& key, const char* name) : CMMIODevImpl(key, name, 0x8, LE)
{
}
