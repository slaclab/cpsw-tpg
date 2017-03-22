#ifndef TPGEN_EVENTSELECTION_HH
#define TPGEN_EVENTSELECTION_HH

//
//  Event selection logic for BSA and timing receivers
//
//

#include <stdint.h>

namespace TPGen {
  class EventSelection {
  public:
    virtual ~EventSelection() {}
    //  Encoding of selection logic for hardware
    virtual uint32_t word() const = 0;
  };

  class FixedRateSelect : public EventSelection {
  public:
    //  
    //  Construct a selection using one fixed rate marker and
    //  an optional bit mask of destinations.  Default 
    //  destination_mask (0) is "Don't Care".
    //
    FixedRateSelect(unsigned rateSelect,
		    unsigned destination_mask=0);
    ~FixedRateSelect();
  public:
    uint32_t word() const;
  private:
    uint32_t _word;
  };

  class ACRateSelect : public EventSelection {
  public:
    //  
    //  Construct a selection using one powerline-synchronized 
    //  rate marker, a bit mask of timeslots (bits 0-5), and
    //  an optional bit mask of destinations.  Default 
    //  destination_mask (0) is "Don't Care".
    //
    ACRateSelect(unsigned rateSelect,
		 unsigned timeslot_mask,
		 unsigned destination_mask=0);
    ~ACRateSelect();
  public:
    uint32_t word() const;
  private:
    uint32_t _word;
  };

  class ExpSeqSelect : public EventSelection {
  public:
    //  
    //  Construct a selection using one bit from an experiment
    //  control sequence word and
    //  an optional bit mask of destinations.  Default 
    //  destination_mask (0) is "Don't Care".
    //
    ExpSeqSelect(unsigned seq,
		 unsigned bit,
		 unsigned destination_mask=0);
    ~ExpSeqSelect();
  public:
    uint32_t word() const;
  private:
    uint32_t _word;
  };

  class Frame;
  class EventSelectionSim {
  public:
    EventSelectionSim(const EventSelection&);
    ~EventSelectionSim();
  public:
    bool select(const Frame&) const;
  private:
    uint32_t _sel;
  };
};

#endif
