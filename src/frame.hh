#ifndef TPGEN_FRAME_HH
#define TPGEN_FRAME_HH

//
//  Timing pattern frame format breakdown
//
//
#include <stdint.h>
#include <stdio.h>

namespace TPGen {
#pragma pack(push,2)
  class Frame {
  public:
    Frame();
  public:
    void printHeader();
    void print() const;
    void dump (FILE*) const;
    void load (FILE*);
  public:
    bool     resync () const { return (acTimeslot>>15)&0x1; }
    unsigned tsPhase() const { return (acTimeslot>> 3)&0xfff; }
    unsigned ts     () const { return (acTimeslot>> 0)&0x7; }
  public:
    uint32_t wsel;
    uint16_t vsn[4];
    uint64_t pulseId;
    uint32_t timeStamp_nanoseconds;
    uint32_t timeStamp_seconds;
    uint16_t rates;
    uint16_t acTimeslot;
    uint32_t beamReq;
    uint16_t status;
    uint16_t mps[5];
    uint16_t diag[4];
    uint64_t bsaInit;
    uint64_t bsaActive;
    uint64_t bsaAvgDone;
    uint64_t bsaUpdate;
    uint32_t expCntl[9];
    uint16_t address;
    uint16_t partition[8];
    uint16_t crc;
  };
#pragma pack(pop)
};

#endif
