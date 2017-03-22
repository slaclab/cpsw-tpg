#include "device.hh"

//
//  Generator hardware address mapping
//
//
using namespace TPGen;

#define ADDM(name) name(p->findByName(#name))

BsaDef::BsaDef(Path p) :
  ADDM(_word0),
  ADDM(_word1)
{
}

SeqState::SeqState(Path p) :
  ADDM(address),
  ADDM(counts)
{
}

IntTrig::IntTrig(Path p) :
  ADDM(evcode),
  ADDM(delay)
{
}

JumpTable::JumpTable(Path p) :
  ADDM(mps),
  ADDM(bcs),
  ADDM(trg)
{
}

Device::Device(Path p) :
  //  ADDM(internalClock),
  ADDM(baseRateControl),
  //  ADDM(acDelayControl),
  ADDM(pulseId),
  ADDM(tstamp),
  ADDM(acRate1_4),
  ADDM(acRate5_6),
  ADDM(fixedRate[10]), 
  ADDM(rateReload),
  ADDM(seqReset),
  ADDM(histControl),
  ADDM(genStatus),
  ADDM(destnPriority),
  ADDM(fwVersion),
  ADDM(seqAddr[28]),
  ADDM(irqCntl),
  ADDM(irqStatus),
  ADDM(irqFifoData),
  ADDM(fifoCntl),
  ADDM(fifoData),
  ADDM(resources),
  //  IntTrig intTrig[7]),
  ADDM(bsaComplUpper),
  ADDM(bsaComplLower),
  //  BsaDef  bsaDef[64]),
  ADDM(bsaStat[64]),
  ADDM(cntPLL),
  ADDM(cnt186M),
  ADDM(cntSyncE),
  ADDM(cntInterval),
  ADDM(cntBRT),
  ADDM(cntTrig[12]),
  ADDM(cntSeq[14]),
  JumpTable seqJump[16],
  SeqState seqstate[32])
{
}

#endif
