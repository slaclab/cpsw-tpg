#include <Sy56040.hh>

using namespace TPGen;

Sy56040 ISy56040::create(const char* name)
{
  Sy56040Impl v = CShObj::create<Sy56040Impl>(name);
  Field f;
  f = IIntField::create("Out", 32, false, 0);
  v->CMMIODevImpl::addAtAddress(f, 0, 4, 4);
  return v;
}

CSy56040Impl::CSy56040Impl(Key& key, const char* name) : CMMIODevImpl(key, name, 0x2000, LE)
{
}
