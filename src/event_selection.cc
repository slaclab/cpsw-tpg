//////////////////////////////////////////////////////////////////////////////
// This file is part of 'tpg'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'tpg', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#include "event_selection.hh"
#include "frame.hh"

using namespace TPGen;

//
//  Three time selections: Fixed rate, AC rate, control seq
//    (12:11) = 00   Fixed rate
//       (3:0) = rate marker
//            = 01   AC rate
//       (2:0) = rate marker
//       (8:3) = timeslot mask
//            = 10   control seq
//       (4:0) = seq bit
//       (10:5) = seq #
//  Destination Selection
//    (30:29) = 10   Dont care
//            = 01   None
//            = 00   One of
//       (28:13) = mask of destinations
//
FixedRateSelect::FixedRateSelect(unsigned rateSelect,
				 unsigned destination_mask) :
  _word((0<<11) | (rateSelect&0xf) | 
	(destination_mask ? destination_mask<<13 : 2<<29))
{
}

FixedRateSelect::~FixedRateSelect() {}

uint32_t FixedRateSelect::word() const { return _word; }


ACRateSelect::ACRateSelect(unsigned rateSelect,
			   unsigned timeslot_mask,
			   unsigned destination_mask) :
  _word((1<<11) | ((timeslot_mask&0x3f)<<3) | (rateSelect&7) | 
	(destination_mask ? destination_mask<<13 : 2<<29))
{
}

ACRateSelect::~ACRateSelect() {}

uint32_t ACRateSelect::word() const { return _word; }


ExpSeqSelect::ExpSeqSelect(unsigned seq,
			   unsigned bit,
			   unsigned destination_mask) :
  _word((2<<11) | ((seq&0x3f)<<5) | (bit&0x1f) | 
	(destination_mask ? destination_mask<<13 : 2<<29))
{
}

ExpSeqSelect::~ExpSeqSelect() {}


uint32_t ExpSeqSelect::word() const { return _word; }


EventSelectionSim::EventSelectionSim(const EventSelection& sel) : _sel(sel.word()) {}

EventSelectionSim::~EventSelectionSim() { delete &_sel; }

bool EventSelectionSim::select(const Frame& f) const
{
  bool result=true;
  uint32_t v = _sel;
  switch((v>>11)&0x3) {
  case 0:
    result &= ((f.rates>>(v&0xf))&1);
    break;
  case 1:
    result &= ((f.rates>>((v&0x7)+10))&1);
    result &= ((v>>3)&0x3f)&(1<<(f.acTimeslot-1));
    break;
  case 2:
    result &= (f.expCntl[(v>>5)&0x3f]&(1<<(v&0x1f)));
    break;
  default:
    result = false;
    break;
  }

  if (((v>>29)&3)==0)
    result &= (v>>16)&(1<<((f.beamReq>>4)&0xf));
  
  return result;
}

