#include "sequence_engine_legacy.hh"
#include "user_sequence.hh"
#include "tpg.hh"
#include "device_legacy.hh"
 
#include <climits>
#include <map>

namespace TPGen {

  class SeqCache {
  public:
    int      index;
    unsigned size;
    std::vector<Instruction*> instr;
  };

  class SequenceEngineCarrier::PrivateData {
  public:
    LegacyRegister*              _ram;
    LegacyRegister*              _reset;
    JumpTable*                   _jump;
    uint32_t                     _id;
    ControlRequest::Type         _request_type;
    uint64_t                     _indices;  // bit mask of reserved indices
    std::map<unsigned,SeqCache>  _caches;   // map start address to sequence
    std::map<unsigned,Callback*> _callback;
  };
};

using namespace TPGen;

static unsigned _verbose=0;

/**
   --
   --  Sequencer instructions:
   --    (31:29)="010"  Fixed Rate Sync -- shifted down 1
   --       (19:16)=marker_id
   --       (11:0)=occurrence
   --    (31:29)="011"  AC Rate Sync -- shifted down 1
   --       (28:23)=timeslot_mask  -- shifted down 1
   --       (19:16)=marker_id
   --       (11:0)=occurrence
   --    (31:29)="001"  Checkpoint/Notify -- shifted down 1
   --    (31:29)="000"  Branch -- shifted down 1
   --       (28:27)=counter
   --       (24)=conditional
   --       (23:16)=test_value
   --       (10:0)=address
   --    (31:29)="100" Request
   --       (15:0)  Value
**/

static unsigned _nwords(const Instruction& i) { return 1; }

static inline uint32_t _word(const FixedRateSync& i)
{
  return (2<<29) | ((i.marker_id&0xf)<<16) | (i.occurrence&0xfff);
}

static inline uint32_t _word(const ACRateSync& i)
{
  return (3<<29) | ((i.timeslot_mask&0x3f)<<23) | ((i.marker_id&0xf)<<16) | (i.occurrence&0xfff);
}

static inline uint32_t _word(const Branch& i, unsigned a)
{
  return (i.test&0xff)==0 ? (a&0x3ff) :
    ((unsigned(i.counter)&0x3)<<27) | (1<<24) | ((i.test&0xff)<<16) | (a&0x3ff);
}

static inline uint32_t _word(const Checkpoint& i)
{
  return 1<<29;
}

static inline uint32_t _word(const ControlRequest& i)
{
  return (4<<29) | i.value();
}

static uint32_t _request(const ControlRequest* req, unsigned id)
{
  uint32_t v = 0;
  if (req) {
    switch(req->request()) {
    case ControlRequest::Beam:
      v = static_cast<const BeamRequest*>(req)->charge&0xffff;
      break;
    case ControlRequest::Expt:
      v = static_cast<const ExptRequest*>(req)->word;
      break;
    default:
      break;
    }
  }
  return v; 
}

static int _lookup_address(const std::map<unsigned,SeqCache>& caches,
			   int seq, 
			   unsigned start)
{
  for(std::map<unsigned,SeqCache>::const_iterator it=caches.begin();
      it!=caches.end(); it++)
    if (it->second.index == seq) {
      unsigned a = it->first;
      for(unsigned i=0; i<start; i++)
	a += _nwords(*it->second.instr[i]);
      return a;
    }
  return -1;
}

SequenceEngineCarrier::SequenceEngineCarrier(LegacyRegister* addr,
					     LegacyRegister* reset,
					     JumpTable* jump,
					     uint32_t  id,
					     ControlRequest::Type req) :
  _private(new SequenceEngineCarrier::PrivateData)
{
  _private->_ram    = addr;
  _private->_reset  = reset;
  _private->_jump   = jump;
  _private->_id     = id;
  _private->_request_type = req;
  
  _private->_indices = 3;

  //  Assign a single instruction sequence at first and last address to trap
  std::vector<Instruction*> v(1);
  v[0] = new Branch(0);
  unsigned a=0;
  _private->_caches[a].index = 0;
  _private->_caches[a].size  = 1;
  _private->_caches[a].instr = v;
  _private->_ram   [a] = _word(*static_cast<const Branch*>(v[0]),a);

  v[0] = new Branch(0);
  a=0x800-1;
  _private->_caches[a].index = 1;
  _private->_caches[a].size  = 1;
  _private->_caches[a].instr = v;
  _private->_ram   [a] = _word(*static_cast<const Branch*>(v[0]),a);
}

SequenceEngineCarrier::~SequenceEngineCarrier()
{
  for(std::map<unsigned,SeqCache>::iterator it=_private->_caches.begin();
      it!=_private->_caches.end(); it++)
    removeSequence(it->second.index);
}

int  SequenceEngineCarrier::insertSequence(std::vector<Instruction*>& seq)
{
  int rval=0, aindex=-3;

  do {
    //  Validate sequence
    for(unsigned i=0; i<seq.size(); i++) {
      if (seq[i]->instr()==Instruction::Request) {
        ControlRequest* request = static_cast<const ControlRequest*>(seq[i]);
        if (request->request()!=_private->_request_type)
          rval=-1;
      }
    }

    if (rval) break;

    //  Calculate memory needed
    unsigned nwords=0;
    for(unsigned i=0; i<seq.size(); i++)
      nwords += _nwords(*seq[i]);

    if (_verbose>1)
      printf("insertSequence %zu instructions, %u words\n",seq.size(),nwords);

    //  Find memory range (just) large enough
    unsigned best_ram=0;
    {
      unsigned addr=0;
      unsigned best_size=INT_MAX;
      for(std::map<unsigned,SeqCache>::iterator it=_private->_caches.begin();
	  it!=_private->_caches.end(); it++) {
	unsigned isize = it->first-addr;
	if (_verbose>1)
	  printf("Found memblock %x:%x [%x]\n",addr,it->first,isize);
	if (isize == nwords) {
	  best_size = isize;
	  best_ram = addr;
	  break;
	}
	else if (isize>nwords && isize<best_size) {
	  best_size = isize;
	  best_ram = addr;
	}
	addr = it->first+it->second.size;
      }
      if (best_size==0) {
	rval=-1;  // no space available in BRAM
	break;
      }
      if (_verbose>1)
	printf("Using memblock %x:%x [%x]\n",best_ram,best_ram+nwords,nwords);
    }

    if (rval) break;

    //  Cache instruction vector, start address (reserve memory)
    if (_private->_indices == -1ULL) {
      rval=-2;
      break;
    }

    for(unsigned i=0; i<64; i++)
      if ((_private->_indices & (1<<i))==0) {
	_private->_indices |= (1<<i);
	aindex=i;
	break;
      }
  
    _private->_caches[best_ram].index = aindex;
    _private->_caches[best_ram].size  = nwords;
    _private->_caches[best_ram].instr = seq;
  
    //  Translate addresses
    unsigned addr = best_ram;
    for(unsigned i=0; i<seq.size(); i++) {
      switch(seq[i]->instr()) {
      case Instruction::Branch:
	{ const Branch& instr = *static_cast<const Branch*>(seq[i]);
	  int jumpto = instr.address;
	  if (jumpto > int(seq.size())) rval=-3;
	  else if (jumpto >= 0) {
	    unsigned jaddr = 0;
	    for(int j=0; j<jumpto; j++)
	      jaddr += _nwords(*seq[j]);
	    _private->_ram[addr++] = _word(instr,jaddr+best_ram);
	  }
	} break;
      case Instruction::Fixed:
	_private->_ram[addr++] =
	  _word(*static_cast<const FixedRateSync*>(seq[i])); 
	break;
      case Instruction::AC:
	_private->_ram[addr++] = _word(*static_cast<const ACRateSync*>(seq[i]));
	break;
      case Instruction::Check:
	{ const Checkpoint& instr = 
	    *static_cast<const Checkpoint*>(seq[i]);
	  _private->_callback[addr] = instr.callback();
	  _private->_ram[addr++] = _word(instr); }
	break;
      case Instruction::Request:
	_private->_ram[addr++] =
	  _word(*static_cast<const ControlRequest*>(seq[i]));
        break;
      default:
	break;
      }
    }
    if (rval)
      removeSequence(aindex);
  } while(0);

  return rval ? rval : aindex;
}

int  SequenceEngineCarrier::removeSequence(int index)
{
  if ((_private->_indices&(1ULL<<index))==0) return -1;
  _private->_indices &= ~(1ULL<<index);

  //  Lookup sequence
  for(std::map<unsigned,SeqCache>::iterator it=_private->_caches.begin();
      it!=_private->_caches.end(); it++)
    if (it->second.index == index) {
      //  Remove callbacks
      for(unsigned i=0; i<it->second.size; i++) {
	std::map<unsigned,Callback*>::iterator it=_private->_callback.find(it->first+i);
	if (it!=_private->_callback.end())
	  _private->_callback.erase(it);
      }

      //  Free instruction vector
      for(unsigned i=0; i<it->second.instr.size(); i++)
	delete it->second.instr[i];

      //  Trap entry and free memory
      _private->_ram[it->first] = 0;
      _private->_caches.erase(it);

      return 0;
    }
  return -2;
}

void SequenceEngineCarrier::setAddress  (int seq, unsigned start, unsigned sync)
{
  int a = _lookup_address(_private->_caches,seq,start);
  if (a>=0)
    _private->_jump->man = (sync<<16) | (a&0xffff);
}

void SequenceEngineCarrier::reset() 
{
  uint64_t v = (1<<_private->_id);
  _private->_reset[0] = unsigned(v&0xffffffff);
  _private->_reset[1] = unsigned(v>>32);
}

void SequenceEngineCarrier::setMPSJump    (int mps, int seq, unsigned start)
{
  if (seq>=0) {
    int a = _lookup_address(_private->_caches,seq,start);
    if (a>=0)
      _private->_jump->mps[mps] = (1<<15) | (a&0x7fff);
  }
  else
    _private->_jump->mps[mps] = 0;    // disable
}

void SequenceEngineCarrier::setBCSJump    (int seq, unsigned start)
{
  if (seq>=0) {
    int a = _lookup_address(_private->_caches,seq,start);
    if (a>=0)
      _private->_jump->bcs = (1<<15) | (a&0x7fff);
  }
  else
    _private->_jump->bcs = 0;    // disable
}

void SequenceEngineCarrier::dumpSequence(int index) const
{
  if ((_private->_indices&(1ULL<<index))==0) return;

  //  Lookup sequence
  for(std::map<unsigned,SeqCache>::const_iterator it=_private->_caches.begin();
      it!=_private->_caches.end(); it++)
    if (it->second.index == index)
      for(unsigned i=0; i<it->second.size; i++)
	printf("[%p] %08x\n",_private->_ram+it->first+i,unsigned(_private->_ram[it->first+i]));
}

void SequenceEngineCarrier::handle(unsigned addr)
{
  std::map<unsigned,Callback*>::iterator it=_private->_callback.find(addr);
  if (it!=_private->_callback.end()) 
    it->second->routine();
}

void SequenceEngineCarrier::dump() const
{
  for(unsigned i=0; i<64; i++)
    if ((_private->_indices&(1ULL<<i))!=0) {
      printf("Sequence %d\n",i);
      dumpSequence(i);
    }
}

void SequenceEngineCarrier::verbosity(unsigned v)
{ _verbose=v; }

