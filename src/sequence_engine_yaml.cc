//////////////////////////////////////////////////////////////////////////////
// This file is part of 'tpg'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'tpg', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#include "sequence_engine_yaml.hh"
#include "user_sequence.hh"
#include "tpg.hh"
 
#include <climits>
#include <map>
#include <stdexcept>
#include <stdio.h>

static char _buff[256];
static const char* StartAddrName(unsigned engine) {
  sprintf(_buff, "StartAddr[%u]/MemoryArray", engine);
  return _buff;
}

static unsigned _verbose=0;

namespace TPGen {

  class SeqCache {
  public:
    int      index;
    unsigned size;
    std::vector<Instruction*> instr;
  };
  
  class SeqJump {
  public:
    SeqJump(Path p, unsigned iengine) :
      _startAddr(IScalVal::create(p->findByName(StartAddrName(iengine)))),
      _engine   (iengine)
    {}
  public:
    void    setManStart(unsigned   addr, unsigned pclass, unsigned sync) { 
      IndexRange rng(15);   
      unsigned v = (addr&0xfff) | ((pclass&0xf)<<12) | (sync<<16);
      _startAddr->setVal(&v,1,&rng); 
      _startAddrCache[15] = v;
    }
    void    setBcsStart(unsigned   addr,
                        unsigned   pclass) { 
      IndexRange rng(14);
      unsigned v = (addr&0xfff) | ((pclass&0xf)<<12);
      _startAddr->setVal(&v,1,&rng); 
      _startAddrCache[14] = v;
    }
    void    setMpsStart(unsigned   chan,
                        unsigned   addr,
                        unsigned   pclass) { 
      IndexRange rng(chan); 
      unsigned v = (addr&0xfff) | ((pclass&0xf)<<12);
      _startAddr->setVal(&v,1,&rng); 
      _startAddrCache[chan] = v;
    }
    void    setMpsState(unsigned   val ,
                        unsigned   sync) 
    { unsigned a,p;
      a = _startAddrCache[val];
      p = (a>>12)&0xf;
      a &= 0xfff;
      setManStart(a,p,sync); }
      
 private:
    ScalVal _startAddr;
    unsigned _engine;
    unsigned _startAddrCache[16];
  };

  class SeqWord {
  public:
    SeqWord(ScalVal s, unsigned index) :
      _s(s), _rng(index) {}
  public:
    operator unsigned() const {
      unsigned v;
      _s->getVal(&v,1,&_rng);
      return v;
    }
    SeqWord& operator=(unsigned v) {
      _s->setVal(&v,1,&_rng);
      return *this;
    }
  private:
    ScalVal  _s;
    mutable IndexRange _rng;
  };

  class SeqRam {
  public:
    SeqRam(Path p, unsigned iengine) :
      _scal(IScalVal::create(p->findByName("MemoryArray"))),
      _word(_scal,0)
    {}
  public:
    SeqWord& operator[](unsigned i) { 
      _word = SeqWord(_scal,i);
      return _word;
    }
  private:
    ScalVal  _scal;
    SeqWord  _word; // won't allow copying from one location to another
  };

  class SequenceEngineYaml::PrivateData {
  public:
    PrivateData(Path     ramPath,
                ScalVal  reset,
                ScalVal  start,
                Path     jumpPath,
                unsigned id,
                ControlRequest::Type req) :
      _ram    (ramPath,id),
      _reset  (reset),
      _start  (start),
      _jump   (new SeqJump(jumpPath,id)),
      _id     (id),
      _request_type(req),
      _indices(0) {}
  public:
    SeqRam                       _ram;
    ScalVal                      _reset;
    ScalVal                      _start;
    SeqJump*                     _jump;
    uint32_t                     _id;
    ControlRequest::Type         _request_type;
    uint64_t                     _indices;  // bit mask of reserved indices
    std::map<unsigned,SeqCache>  _caches;   // map start address to sequence
    std::map<unsigned,Callback*> _callback;
  };
};

using namespace TPGen;

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
   --       (23:12)=test_value
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
  return (i.test&0xff)==0 ? (a) :
    ((unsigned(i.counter)&0x3)<<27) | (1<<24) | ((i.test&0xfff)<<12) | (a);
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

SequenceEngineYaml::SequenceEngineYaml(Path    ramPath,
                                       ScalVal reset,
                                       ScalVal start,
                                       Path    jumpPath,
                                       uint32_t  id,
                                       ControlRequest::Type req,
                                       unsigned  addrWidth) :
  _private(new SequenceEngineYaml::PrivateData(ramPath,
                                               reset,
                                               start,
                                               jumpPath,
                                               id,
                                               req))
{
  _private->_indices = 3;

  //  Assign a single instruction sequence at first and last address to trap
  std::vector<Instruction*> v(1);
  v[0] = new Branch(0);
  unsigned a=0;
  _private->_caches[a].index = 0;
  _private->_caches[a].size  = 1;
  _private->_caches[a].instr = v;
  _private->_ram   [a] = _word(*static_cast<const Branch*>(v[0]),a);

  a=(1<<addrWidth)-1;
  v[0] = new Branch(a);
  _private->_caches[a].index = 1;
  _private->_caches[a].size  = 1;
  _private->_caches[a].instr = v;
  _private->_ram   [a] = _word(*static_cast<const Branch*>(v[0]),a);
}

SequenceEngineYaml::~SequenceEngineYaml()
{
  for(std::map<unsigned,SeqCache>::iterator it=_private->_caches.begin();
      it!=_private->_caches.end(); it++)
    removeSequence(it->second.index);
  delete _private;
}

int  SequenceEngineYaml::insertSequence(std::vector<Instruction*>& seq)
{
  int rval=0, aindex=-3;

  do {
    //  Validate sequence
    for(unsigned i=0; i<seq.size(); i++) {
      if (seq[i]->instr()==Instruction::Request) {
        const ControlRequest* request = static_cast<const ControlRequest*>(seq[i]);
        if (request->request()!=_private->_request_type) {
          printf("Incorrect request type in sequence for this engine\n");
          rval=-1;
        }
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
      if (best_size==INT_MAX) {
	printf("No space available in BRAM\n");
	rval=-1;  // no space available in BRAM
	break;
      }
      if (_verbose>1)
	printf("Using memblock %x:%x [%x]\n",best_ram,best_ram+nwords,nwords);
    }

    if (rval) break;

    //  Cache instruction vector, start address (reserve memory)
    if (_private->_indices == -1ULL) {
      printf("All subsequence indices exhausted\n");
      rval=-2;
      break;
    }

    for(unsigned i=0; i<64; i++)
      if ((_private->_indices & (1ULL<<i))==0) {
	_private->_indices |= (1ULL<<i);
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
	  if (jumpto > int(seq.size())) {
              printf("Branch jumps outside of sequence\n");
              rval=-3;
          }
	  else if (jumpto >= 0) {
	    unsigned jaddr = 0;
	    for(int j=0; j<jumpto; j++)
	      jaddr += _nwords(*seq[j]);
            if (_verbose>2)
              printf(" ram[%u] = 0x%08x\n",
                     addr,_word(instr,jaddr+best_ram));
	    _private->_ram[addr++] = _word(instr,jaddr+best_ram);
	  }
	} break;
      case Instruction::Fixed:
        if (_verbose>2)
          printf(" ram[%u] = 0x%08x\n",
                 addr,_word(*static_cast<const FixedRateSync*>(seq[i])));
        _private->_ram[addr++] = 
          _word(*static_cast<const FixedRateSync*>(seq[i])); 
	break;
      case Instruction::AC:
        if (_verbose>2)
          printf(" ram[%u] = 0x%08x\n",
                 addr,_word(*static_cast<const ACRateSync*>(seq[i])));
        _private->_ram[addr++] = 
          _word(*static_cast<const ACRateSync*>(seq[i]));
	break;
      case Instruction::Check:
	{ const Checkpoint& instr = 
	    *static_cast<const Checkpoint*>(seq[i]);
	  _private->_callback[addr] = instr.callback();
          if (_verbose>2)
            printf(" ram[%u] = 0x%08x\n",
                   addr,_word(instr));
          _private->_ram[addr++] = _word(instr); }
	break;
      case Instruction::Request:
        if (_verbose>2)
          printf(" ram[%u] = 0x%08x\n",
                 addr,_word(*static_cast<const ControlRequest*>(seq[i])));
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

int  SequenceEngineYaml::removeSequence(int index)
{
  if ((_private->_indices&(1ULL<<index))==0) return -1;
  _private->_indices &= ~(1ULL<<index);

  //  Lookup sequence
  for(std::map<unsigned,SeqCache>::iterator it=_private->_caches.begin();
      it!=_private->_caches.end(); it++)
    if (it->second.index == index) {
      //  Remove callbacks
      for(unsigned i=0; i<it->second.size; i++) {
	std::map<unsigned,Callback*>::iterator cbit=_private->_callback.find(it->first+i);
	if (cbit!=_private->_callback.end())
	  _private->_callback.erase(cbit);
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

void SequenceEngineYaml::setAddress  (int seq, unsigned start, unsigned sync)
{
  int a = _lookup_address(_private->_caches,seq,start);
  if (a>=0) {
    _private->_jump->setManStart(a,0,sync);
  }
}

void SequenceEngineYaml::reset() 
{
  uint32_t v[4];
  memset(v,0,sizeof(v));
  v[_private->_id >> 5] = 1U<<(_private->_id & 0x1f);
  { IndexRange rng(0,3);
    _private->_reset->setVal(v,4,&rng); }
  { IndexRange rng(0);
    _private->_start->setVal(v,1,&rng); }  // the value doesn't matter
}

void SequenceEngineYaml::setMPSJump    (int mps, int seq, unsigned pclass, unsigned start)
{
  if (seq>=0) {
    int a = _lookup_address(_private->_caches,seq,start);
    if (a>=0) {
      _private->_jump->setMpsStart(mps,a,pclass);
    }
  }
  else {
    //    _private->_jump->mps[mps] = 0;    // disable
  }
}

void SequenceEngineYaml::setBCSJump    (int seq, unsigned pclass, unsigned start)
{
  if (seq>=0) {
    int a = _lookup_address(_private->_caches,seq,start);
    if (a>=0) {
      _private->_jump->setBcsStart(a,pclass);
    }
  }
  else
    ; //    _private->_jump->bcs = 0;    // disable
}

void SequenceEngineYaml::setMPSState  (int mps, unsigned sync)
{
  _private->_jump->setMpsState(mps,sync);
  reset();
}


void SequenceEngineYaml::dumpSequence(int index) const
{
  //  Lookup sequence
  for(std::map<unsigned,SeqCache>::const_iterator it=_private->_caches.begin();
      it!=_private->_caches.end(); it++) {
    if (it->second.index == index) {
      const std::vector<Instruction*>& seq = it->second.instr;
      unsigned j=0, ij=0;
      for(unsigned i=0; i<it->second.size; i++) {
        printf("[%08x] %08x",it->first+i,unsigned(_private->_ram[it->first+i]));
        if (i==ij) {
          printf(" %s\n",seq[j]->descr().c_str());
          ij += _nwords(*seq[j]);
          j++;
        }
        else
          printf("\n");
      }
      break;
    }
  }
}

void SequenceEngineYaml::handle(unsigned addr)
{
  std::map<unsigned,Callback*>::iterator it=_private->_callback.find(addr);
  if (it!=_private->_callback.end()) 
    it->second->routine();
}

void SequenceEngineYaml::dump() const
{
  for(unsigned i=0; i<64; i++)
    if ((_private->_indices&(1ULL<<i))!=0) {
      printf("Sequence %d\n",i);
      dumpSequence(i);
    }
}

void SequenceEngineYaml::verbosity(unsigned v)
{ _verbose=v; }

InstructionCache SequenceEngineYaml::cache(unsigned index) const
{
  for(std::map<unsigned,SeqCache>::iterator it=_private->_caches.begin();
      it!=_private->_caches.end(); it++) {
    if (it->second.index == index) {
      InstructionCache c;
      c.index        = it->second.index;
      c.ram_address  = it->first;
      c.ram_size     = it->second.size;
      c.instructions = it->second.instr;
      return c;
      break;
    }
  }
  //  throw std::invalid_argument("index not found");
  InstructionCache c;
  c.index        = -1;
  return c;
}

std::vector<InstructionCache> SequenceEngineYaml::cache() const
{
  std::vector<InstructionCache> rval;
  for(std::map<unsigned,SeqCache>::iterator it=_private->_caches.begin();
      it!=_private->_caches.end(); it++) {
    InstructionCache c;
    c.index        = it->second.index;
    c.ram_address  = it->first;
    c.ram_size     = it->second.size;
    c.instructions = it->second.instr;
    rval.push_back(c);
  }
  return rval;
}
