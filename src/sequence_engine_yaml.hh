#ifndef TPG_SequenceEngine_yaml_hh
#define TPG_SequenceEngine_yaml_hh

//
//  Sequence engine to drive timing patterns for beam requests
//  and experiment control.
//

#include "sequence_engine.hh"
#include <cpsw_api_user.h>

namespace TPGen {
  class JumpTable;
  class LegacyRegister;

  class SequenceEngineYaml : public SequenceEngine {
  public:
    int  insertSequence(std::vector<Instruction*>& seq);
    int  removeSequence(int seq);
    void setAddress    (int seq, unsigned start=0, unsigned sync=1);
    void reset         ();
    void setMPSJump    (int mps, int seq, unsigned pclass, unsigned start=0);
    void setBCSJump    (int seq, unsigned start=0);
 public:
    void      handle            (unsigned address);
  public:
    void dumpSequence  (int seq) const;
    void dump          ()        const;
  public:
    static void verbosity(unsigned);
  protected:
    //
    //  Construct an engine with its sequence RAM and start register address
    //
    SequenceEngineYaml(Path      ramPath,
                       ScalVal   resetReg,
                       Path      jumpPath,
                       unsigned  id,
                       ControlRequest::Type req_type,
                       unsigned  addrWidth=11);
    ~SequenceEngineYaml();
  protected:
    friend class TPGYaml;
    class PrivateData;
    PrivateData* _private;
  };
};

#endif
