#ifndef BSP_TPG_HH
#define BSP_TPG_HH

//
//  Class for coordinating all control of the timing generator
//
#include <stdint.h>

#include <list>
#include <vector>

namespace TPGen {

  typedef unsigned TPGDestination;

  class Device;
  class EventSelection;
  class SequenceEngine;

  class FaultStatus {
  public:
    bool      manualLatch;
    bool      bcsLatch;
    bool      mpsLatch;
    unsigned  tag;
    unsigned  mpsTag;
  };

  //
  //  Base class for asynchronous notifications
  //
  class Callback {
  public:
    virtual ~Callback() {}
    virtual void routine() = 0;
  };

  class TPG {
  public:
    virtual ~TPG() {}
  public:
    //
    //  Resources 
    //
    //    Number of sequence engines for beam destinations
    //
    virtual unsigned nBeamEngines() const = 0;
    //
    //    Number of sequence engines for allow tables
    //
    virtual unsigned nAllowEngines() const = 0;
    //
    //    Number of sequence engines for experiment control
    //
    virtual unsigned nExptEngines() const = 0;
    //
    //    Number of BSA arrays
    //
    virtual unsigned nArraysBSA  () const = 0;
    //
    //    Sequence address bus width (log2 of max instructions)
    //
    virtual unsigned seqAddrWidth() const = 0;
    //
    //    FIFO address bus width (log2 of max fifo words[16b])
    //
    virtual unsigned fifoAddrWidth() const = 0;
    //
    //    Number of programmable rate counters
    //
    virtual unsigned nRateCounters() const = 0;
    //
    //  Sets the timestamp advance for each 186MHz step.
    //  Expressed as integer nanoseconds + frac_num/frac_den
    //
    virtual void setClockStep(unsigned ns, unsigned frac_num, unsigned frac_den) = 0;
    //
    //  186MHz divisor for setting base rate trigger
    //  Returns a negative result in case of error (divisor too small)
    //
    virtual int  setBaseDivisor(unsigned v) = 0;
    //
    //  Sets a delay before generating the powerline-synchronized
    //  markers associated with the 360Hz zero-crossing triggers
    //  Returns a negative result in case of error (delay too large)
    virtual int  setACDelay    (unsigned v) = 0; // in base rate triggers
    //
    //  Sets a delay before transmitting the timing frame.
    //  Returns a negative result in case of error (delay too large)
    virtual int  setFrameDelay (unsigned v) = 0; // in 186MHz cycles
    //
    //  Initialize the pulseID
    //
    virtual void setPulseID    (uint64_t) = 0;
    //
    //  Initialize the timestamp
    //
    virtual void setTimestamp  (unsigned sec, unsigned nsec) = 0;
    //
    //  Current pulseID
    //
    virtual uint64_t getPulseID() const = 0;
    //
    //  Current timestamp
    //
    virtual void     getTimestamp(unsigned& sec, unsigned& nsec) const = 0;
    //
    //  60Hz divisors for setting powerline-synchronized triggers
    //
    virtual void setACDivisors   (const std::vector<unsigned>&) = 0;
    //
    //  Base rate trigger divisors for setting fixed rate triggers
    //
    virtual void setFixedDivisors(const std::vector<unsigned>&) = 0;
    //
    //  Synchronous load of previously configured divisors
    //
    virtual void loadDivisors    () = 0;
    //
    //  Force synchronization (used when no 71kHz resync trigger is available)
    //
    virtual void force_sync() = 0;
    //
    //  Enable/disable acquisition of history buffers
    //
    virtual void initializeRam         () = 0;
    virtual void acquireHistoryBuffers (bool) = 0;
    virtual void clearHistoryBuffers   (unsigned) = 0;
    virtual std::vector<FaultStatus> getHistoryStatus() = 0;
    //
    //  Program the beam energy meta data
    //
    virtual void setEnergy(const std::vector<unsigned>&) = 0;
    //
    //  Program the photon wavelength meta data
    //
    virtual void setWavelength(const std::vector<unsigned>&) = 0;

    //
    //  Set the mask of required parent sequences for the beam sequences
    //
    virtual void setSequenceRequired   (unsigned iseq, unsigned requiredMask) = 0;
    virtual void setSequenceDestination(unsigned iseq, TPGDestination) = 0;
#if 0
    //
    //  Set the control word for a beam destination
    //
    virtual void setDestinationControl(TPGDestination dst, unsigned control) = 0;
#endif
    //
    //  Sequence setup
    //
    virtual SequenceEngine& engine (unsigned) = 0;

    //
    //  Reset a single sequence engine
    //
    // void resetSequence     (unsigned engine) = 0;
    //
    //  Reset several sequence engines simultaneously
    //
    virtual void resetSequences    (const std::list<unsigned>&) = 0;

    //
    //  Choose which sequence goes into the raw diagnostics
    //
    virtual unsigned getDiagnosticSequence() const = 0;
    virtual void     setDiagnosticSequence(unsigned) = 0;
    //
    //  Enable beam synchronous acquisition for one array with
    //  nToAverage consecutive samples averaged together to make one
    //  result and avgToAcquire averaged results acquired.
    //  Returns negative result on error.
    //
    virtual int  startBSA          (unsigned array,
				    unsigned nToAverage,
				    unsigned avgToAcquire,
				    EventSelection* selection) = 0;
    virtual int  startBSA          (unsigned array,
				    unsigned nToAverage,
				    unsigned avgToAcquire,
				    EventSelection* selection,
                                    unsigned maxSevr) = 0;
    //
    //  Halt the acquisition of an array.
    //
    virtual void stopBSA           (unsigned array) = 0;
    //
    //  Query the status of an acquisition
    //
    virtual void queryBSA          (unsigned array,
                                    unsigned& nToAverage,
                                    unsigned& avgToAcquire) = 0;
    //
    // 
    //  Diagnostic counters
    //
    //  Counts since power up
    //
    virtual unsigned getPLLchanges   () const = 0;  // PLL lock changes
    virtual unsigned get186Mticks    () const = 0;  // 186M clocks
    virtual unsigned getSyncErrors   () const = 0;  // 71k sync errors
    virtual unsigned getCountInterval() const = 0;  // interval counter period [186M]
    //
    //  Counts per interval
    //
    virtual void setCountInterval    (unsigned v) = 0;
    virtual unsigned getBaseRateTrigs() const = 0;  // Base rate triggers/frames
    // Sequence requests
    virtual unsigned getSeqRequests  (unsigned seq) const = 0; 
    virtual unsigned getSeqRequests  (unsigned* array, unsigned array_size) const = 0;
    //
    //  Programmable rate counters (NRateCounters)
    //
    virtual void     lockCounters    (bool) = 0;
    virtual void     setCounter      (unsigned, EventSelection*) = 0;
    virtual unsigned getCounter      (unsigned) = 0;

    //  
    //  MPS state
    //    Retrieve the latched state (limiting power class) for a destination and
    //    the current state (allowed power class)
    //
    virtual void     getMpsState     (unsigned  destination, 
                                      unsigned& latch, 
                                      unsigned& state) = 0;

    virtual void     getMpsCommDiag (unsigned& rxRdy,
                                     unsigned& txRdy,
                                     unsigned& locLnkRdy,
                                     unsigned& remLnkRdy,
                                     unsigned& rxClkFreq,
                                     unsigned& txClkFreq) = 0;
 
    //
    //  Asynchronous notification.
    //  Returns previously registered callback.
    //
    //    When BSA is complete
    //
    virtual Callback* subscribeBSA      (unsigned bsaArray,
					 Callback* cb) = 0;
    //
    //    When interval expires
    //
    virtual Callback* subscribeInterval (Callback* cb) = 0;
    //
    //    When BCS/MPS fault is asserted
    //
    virtual Callback* subscribeFault    (Callback* cb) = 0;
    //
    //  Remove a callback (does not delete)
    //
    virtual void      cancel            (Callback* cb) = 0;
    //
    //  Enable asynchronous notification
    //
    //    Sequence checkpoints
    //
    virtual void enableSequenceIrq(bool) = 0;
    //
    //    Interval expiration
    //
    virtual void enableIntervalIrq(bool) = 0;
    //
    //    BSA complete
    //
    virtual void enableBsaIrq     (bool) = 0;
    //
    //    Fault latched
    //
    virtual void enableFaultIrq   (bool) = 0;

    //  Diagnostics
    virtual void dump() const = 0;
  };
};

#endif
