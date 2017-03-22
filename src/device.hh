#ifndef TPGEN_DEVICE_HH
#define TPGEN_DEVICE_HH

//
//  Generator hardware address mapping
//
//
namespace TPGen {

  class DeviceInspector;

  class BsaDef {
  public:
    BsaDef() {}
  public:
    REGWORD_ _word0;
    REGWORD_ _word1;
  };

  class SeqState {
  public:
    SeqState() {}
  public:
    REGWORD_ address;
    REGWORD_ counts;
  };

  class JumpTable {
  public:
    JumpTable() {}
  public:
    REGWORD_ mps[14];
    REGWORD_ bcs;
    REGWORD_ man;
  };

  class Device {
  public:
    enum {SEQ_SIZE  =0x800};
    enum {NARRAYSBSA=2};
    enum {NBEAMSEQ=16};
    enum {MAXAVGBSA =0xffff};
    enum {MAXACQBSA =0xffff};
    enum {NUML1TRIG=7};
    Device() {}
  private:
    friend class TPG;
    friend class TPGPci;
    friend class TPGSim;
    friend class TPGCarrier;
    REGWORD_ resources;
    REGWORD_ clockControl;
    REGWORD_ baseRateControl;
    REGWORD_ acDelayControl;
    REGWORD_ pulseIdLower;
    REGWORD_ pulseIdUpper;
    REGWORD_ tstampLower;
    REGWORD_ tstampUpper;
    REGWORD_ acRate1_4;
    REGWORD_ acRate5_6;
    REGWORD_ reserved_10[6];
    REGWORD_ fixedRate[10]; 
    REGWORD_ rateReload;
    REGWORD_ histControl;
    REGWORD_ genStatus;
    REGWORD_ irqCntl;
    REGWORD_ irqStatus;
    REGWORD_ irqFifoData; // seqNotify
    REGWORD_ beamSeqConfig[NBEAMSEQ];
    REGWORD_ destnControl [NBEAMSEQ];
    REGWORD_ seqResetL;
    REGWORD_ seqResetU;
    REGWORD_ reserved_66[60];
    REGWORD_ bsaComplLower;
    REGWORD_ bsaComplUpper;
    BsaDef   bsaDef[64];
    REGWORD_ bsaStatus[64];
    REGWORD_ cntPLL;
    REGWORD_ cnt186M;
    REGWORD_ cntSyncE;
    REGWORD_ cntInterval;
    REGWORD_ cntBRT;
    REGWORD_ reserved_325[12]; // cntTrig[12];
    REGWORD_ cntSeq[64];
    REGWORD_ reserved_401[512-401];
//     REGWORD_ rcvrStat;
//     REGWORD_ rcvrClkCnt;
//     REGWORD_ rcvrDvCnt;
//     REGWORD_ reserved_388[512-388];
    SeqState seqstate[64];
    REGWORD_ reserved_640[1024-640];
    JumpTable seqJump[64];
    //    REGWORD_ reserved_2047; // seqaddr;
    REGWORD_ seq[0x20000-0x800];
    friend class DeviceInspector;
  };
};

#endif
