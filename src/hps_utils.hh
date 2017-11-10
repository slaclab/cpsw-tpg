#ifndef hps_utils_hh
#define hps_utils_hh

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>

#include <string>

#include <cpsw_api_user.h>

namespace Cphw {

  class AxiVersion {
  public:
    AxiVersion(Path);
    std::string buildStamp() const;
  public:
    Path _root;
  };

  class XBar {
  public:
    enum Map { RTM0, FPGA, BP, RTM1 };
    XBar(Path);
    void setOut( Map out, Map in );
    void dump  () const;
  public:
    Path _root;
  };

  class RingBuffer {
  public:
    RingBuffer(Path);
  public:
    void     enable (bool);
    void     clear  ();
    void     dump   ();
    void     clear_and_dump();
  private:
    ScalVal _csr;
    ScalVal _dump;
  };

  class TimingStats {
  public:
    unsigned rxclks;
    unsigned txclks;
    unsigned sof;
    unsigned linkup;
    unsigned polarity;
    unsigned crc;
    unsigned dsp;
    unsigned dec;
  };

  class AmcTiming {
  public:
    AmcTiming(Path);
    void setPolarity     (bool);
    void setLCLS         ();
    void setLCLSII       ();
    void resetStats      ();
    void rxReset         ();
    void bbReset         ();
    TimingStats  getStats() const;
    void dumpStats       () const;
    void setRxAlignTarget(unsigned);
    void setRxResetLength(unsigned);
    void dumpRxAlign     () const;
    void measureClks     () const;
  public:
    AxiVersion version() { return AxiVersion(_root->findByName("AxiVersion")); }
    XBar       xbar   () { return XBar(_root->findByName("AxiSy56040")); }
    RingBuffer ring0  () { return RingBuffer(_root->findByName("ring0")); }
    RingBuffer ring1  () { return RingBuffer(_root->findByName("ring1")); }
  public:
    Path _root;
    Path _timingRx;
    Path _alignRx;
  };

  class GthEyeScan {
  public:
    GthEyeScan(Path);
    bool enabled() const;
    void enable (bool);
    void scan   (const char* ofile,
                 unsigned    prescale=0,
                 unsigned    xscale=0,
                 bool        lsparse=false);
    void run    (unsigned&   error_count,
                 unsigned&   sample_count);
    static void progress(unsigned& row,
                         unsigned& col);
  public:
    Path _root;
  };

  class IpAddrFixup : public IYamlFixup {
  public:
    IpAddrFixup(const char* ip) : _ip(ip) {}
    void operator()(YAML::Node& node);
  private:
    const char* _ip;
  };
};

#endif
