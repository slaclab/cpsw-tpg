#ifndef TPG_SequenceEngine_hh
#define TPG_SequenceEngine_hh

//
//  Sequence engine to drive timing patterns for beam requests
//  and experiment control.
//

#include "user_sequence.hh"

#include <vector>

namespace TPGen {
  class Instruction;

  class SequenceEngine {
  public:
    virtual ~SequenceEngine() {}
    //
    //  Insert into the sequence RAM a vector of instructions.
    //  The instructions will be encoded into the hardware representation.
    //  Return value is a (sub)sequence number used to refer to
    //  instruction sequence for the calls that follow.  Many instruction
    //  sequences may be stored in sequence RAM and activated by the setAddress
    //  call below.
    //  Returns negative result on error.
    //
    virtual int  insertSequence(std::vector<Instruction*>& seq)= 0;
    //
    //  Remove a sequence of instructions from sequence RAM clearing
    //  space for more instruction sets.  Should not be performed on a 
    //  sequence currently being executed.
    //  Returns negative result on error.
    //
    virtual int  removeSequence(int seq)= 0;
    //
    //  Set the starting/jump address for the next reset of the engine.
    //  The address is specified by (sub)sequence index and relative
    //  start offset (instruction index from input vector).
    //
    virtual void setAddress    (int seq, unsigned start=0, unsigned sync=1)= 0;
    virtual void reset         ()= 0;
    //
    //  Set the starting/jump address for mps faults.
    //  The address is specified by (sub)sequence index and relative
    //  start offset (instruction index from input vector).
    //
    virtual void setMPSJump    (int mps, int seq, unsigned pclass, unsigned start=0)= 0;
    //
    //  Set the starting/jump address for bcs faults.
    //  The address is specified by (sub)sequence index and relative
    //  start offset (instruction index from input vector).
    //
    virtual void setBCSJump    (int seq, unsigned start=0)= 0;

    virtual void dumpSequence  (int seq) const= 0;
    virtual void dump          ()        const= 0;
  };
};

#endif
