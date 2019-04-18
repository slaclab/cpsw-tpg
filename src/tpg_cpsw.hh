#ifndef BSP_TPG_CPSW_HH
#define BSP_TPG_CPSW_HH

//
//  Class for coordinating all control of the timing generator
//
#include <stdint.h>

#include <list>
#include <vector>

#include "tpg.hh"

namespace TPGen {

  class TPGCpsw : public TPG {
  public:
    TPGCpsw(const char* ip);
    ~TPGCpsw();
  public:
    //
    //  TPG interface
    //
    unsigned fwVersion    () const;
    unsigned nBeamEngines () const;
    unsigned nAllowEngines() const;
    unsigned nExptEngines () const;
    unsigned nArraysBSA   () const;
    unsigned seqAddrWidth () const;
    unsigned fifoAddrWidth() const;
    unsigned nRateCounters() const;
    void     setClockStep  (unsigned ns, unsigned frac_num, unsigned frac_den);
    int      setBaseDivisor(unsigned v);
    int      setACDelay    (unsigned v); // in base rate triggers
    int      setFrameDelay (unsigned v); // in 186MHz cycles
    void     setPulseID    (uint64_t);
    void     setTimestamp  (unsigned sec, unsigned nsec);
    uint64_t getPulseID() const;
    void     getTimestamp(unsigned& sec, unsigned& nsec) const;
    void     setACDivisors   (const std::vector<unsigned>&);
    void     setFixedDivisors(const std::vector<unsigned>&);
    void     loadDivisors    ();
    void     force_sync();
    void     reset_xbar();
    void     initializeRam();
    void     acquireHistoryBuffers (bool);
    void     clearHistoryBuffers   (unsigned);
    void     setHistoryBufferHoldoff(unsigned) {}
    std::vector<FaultStatus> getHistoryStatus();
    unsigned faultCounts() const { return 0; }
    void     setEnergy(const std::vector<unsigned>&);
    void     setWavelength(const std::vector<unsigned>&);

    void     setSequenceRequired   (unsigned iseq, unsigned requiredMask);
    void     setSequenceDestination(unsigned iseq, TPGDestination);
#if 0
    void     setDestinationControl(const std::list<TPGDestination>&);
#endif
    SequenceEngine& engine (unsigned);

    void     resetSequences    (const std::list<unsigned>&);

    unsigned getDiagnosticSequence() const;
    void     setDiagnosticSequence(unsigned);

    int      startBSA      (unsigned array,
			    unsigned nToAverage,
			    unsigned avgToAcquire,
			    EventSelection* selection);
    int      startBSA      (unsigned array,
			    unsigned nToAverage,
			    unsigned avgToAcquire,
			    EventSelection* selection,
                            unsigned maxSevr);
    void     stopBSA       (unsigned array);
    void     queryBSA      (unsigned  array,
			    unsigned& nToAverage,
			    unsigned& avgToAcquire);
    uint64_t bsaComplete   ();
    void     bsaComplete   (uint64_t ack); // clear handled bits

    unsigned getPLLchanges   () const;  // PLL lock changes
    unsigned get186Mticks    () const;  // 186M clocks
    unsigned getSyncErrors   () const;  // 71k sync errors
    unsigned getCountInterval() const;  // interval counter period [186M]
    void     setCountInterval(unsigned v);
    unsigned getBaseRateTrigs() const;  // Base rate triggers/frames
    unsigned getInputTrigs   (unsigned ch) const; // Input triggers
    unsigned getSeqRequests  (unsigned seq) const; // Sequence requests
    unsigned getSeqRequests  (unsigned* array, unsigned array_size) const; // Sequence requests

    //  Programmable rate counters (NRateCounters)
    void     lockCounters    (bool);
    void     setCounter      (unsigned, EventSelection*);
    unsigned getCounter      (unsigned);

    //  MPS
    void     getMpsState     (unsigned  destination, 
                              unsigned& latch, 
                              unsigned& state);

    void     getMpsCommDiag  (unsigned& rxRdy,
                               unsigned& txRdy,
                              unsigned& locLnkRdy,
                              unsigned& remLnkRdy,
                              unsigned& rxClkFreq,
                              unsigned& txClkFreq,
                              unsigned& rxFrameErrorCount,
                              unsigned& rxFrameCount);
    void     getTimingFrameRxDiag(unsigned& txClkCount);

    //  Power line sampling ADC
    void     initADC         ();

    //  Need async messages to implement these
    Callback* subscribeBSA      (unsigned bsaArray,
				 Callback* cb);
    Callback* subscribeInterval (Callback* cb);
    Callback* subscribeFault    (Callback* cb);
    void      cancel            (Callback* cb);
    void enableSequenceIrq(bool);
    void enableIntervalIrq(bool);
    void enableBsaIrq     (bool);
    void enableFaultIrq   (bool);

    //  Diagnostics
    void dump() const;
    void dump_rcvr(unsigned n);
#if 0  // No longer implemented; use beam diagnostic buffers
    void dumpFIFO(unsigned nfifo) const;
    void reset_fifo(unsigned b=0x100,
		    unsigned m=0xffff);
#endif
    //
    void setL1Trig(unsigned,unsigned,unsigned);
  public:
    //
    //  Internal handling
    //
    void handleIrq ();
  private:
    void enableIrq (unsigned,bool);
  protected:
    class PrivateData;
    PrivateData* _private;
  };
};

#endif
