#include "user_sequence.hh"
#include "tpg.hh"
#include <stdio.h>

using namespace TPGen;

static char buff[256];

Instruction::Type ControlRequest::instr() const
{ return Instruction::Request; }

BeamRequest::BeamRequest(unsigned q) : charge(q) {}
BeamRequest::~BeamRequest() {}
ControlRequest::Type BeamRequest::request() const 
{ return ControlRequest::Beam; }
std::string BeamRequest::descr() const
{ sprintf(buff,"BeamRequest(charge=%u)",charge); return std::string(buff); }
unsigned BeamRequest::value() const
{ return charge; }

ExptRequest::ExptRequest(unsigned w) : word(w) {}
ExptRequest::~ExptRequest() {}
ControlRequest::Type ExptRequest::request() const 
{ return ControlRequest::Expt; }
std::string ExptRequest::descr() const
{ sprintf(buff,"ExptRequest(request=%u)",word); return std::string(buff); }
unsigned ExptRequest::value() const
{ return word; }

Checkpoint::Checkpoint(Callback* cb) : _callback(cb) {}
Checkpoint::~Checkpoint() { delete _callback; }
Callback* Checkpoint::callback() const { return _callback; }
Instruction::Type Checkpoint::instr() const
{ return Instruction::Check; }
std::string Checkpoint::descr() const
{ sprintf(buff,"Checkpoint()"); return std::string(buff); }

FixedRateSync::FixedRateSync(unsigned        m,
			     unsigned        o) :
  marker_id  (m),
  occurrence(o)
{}

FixedRateSync::~FixedRateSync()
{
}

Instruction::Type FixedRateSync::instr() const
{ return Instruction::Fixed; }

std::string FixedRateSync::descr() const
{ sprintf(buff,"FixedRateSync(marker=%u,occ=%u)",marker_id,occurrence); return std::string(buff); }

ACRateSync::ACRateSync(unsigned        t,
		       unsigned        m,
		       unsigned        o) :
  timeslot_mask(t),
  marker_id    (m),
  occurrence   (o)
{
}

ACRateSync::~ACRateSync() 
{
}

Instruction::Type ACRateSync::instr() const
{ return Instruction::AC; }

std::string ACRateSync::descr() const
{ sprintf(buff,"ACRateSync(tsm=0x%x,marker=%u,occ=%u)",timeslot_mask,marker_id,occurrence); return std::string(buff); }


Branch::Branch( unsigned  a ) :
  address(a),
  counter(ctrA),
  test   (0)
{
}

Branch::Branch( unsigned  a,     // address to jump to if test fails
		CCnt      c,     // index of counter to test and increment
		unsigned  t ) :  // value to test against
  address(a),
  counter(c),
  test   (t)
{
}

Branch::~Branch() {}

Instruction::Type Branch::instr() const
{ return Instruction::Branch; }

std::string Branch::descr() const
{ if (test) sprintf(buff,"Branch(addr=%u,cc=%u,test=%u)",address,counter,test);
  else sprintf(buff,"Branch(addr=%u)",address);
  return std::string(buff); }

