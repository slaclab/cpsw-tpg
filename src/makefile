CCACHE_DISABLE=1


CPSW_DIR=/afs/slac/g/lcls/package/cpsw/framework/R3.5.3/src
BSA_DIR=/afs/slac/g/lcls/package/timing/bsa
BSA_VERSION=master
BSA=$(BSA_DIR)/$(BSA_VERSION)

# may override CPSW_DIR / DEVICELIB_DIR from release.mak
# must set SRCDIR (is redefined by recursive make)
SRCDIR=.
-include $(SRCDIR)/release.mak

include $(CPSW_DIR)/defs.mak

LDFLAGS+=-L$(BSA)/$(TARCH)/lib

INCLUDE_DIRS = $(BSA)/$(TARCH)/include
INSTALL_DIR=../

CXXFLAGS = -g -DFRAMEWORK_R3_4
HEADERS  = TPG.hh SeqState.hh SeqJump.hh SeqMem.hh RamControl.hh Sy56040.hh AxiStreamDmaRingWrite.hh RssiCore.hh
HEADERS += sequence_engine.hh sequence_engine_cpsw.hh sequence_engine_yaml.hh
HEADERS += tpg.hh tpg_cpsw.hh user_sequence.hh event_selection.hh frame.hh tpg_yaml.hh

tpg_SRCS  = TPG.cc SeqState.cc SeqJump.cc SeqMem.cc RamControl.cc Sy56040.cc AxiStreamDmaRingWrite.cc RssiCore.cc
tpg_SRCS += sequence_engine_cpsw.cc sequence_engine_yaml.cc
tpg_SRCS += tpg_cpsw.cc user_sequence.cc event_selection.cc tpg_yaml.cc

STATIC_LIBRARIES+=tpg

tpg_tst_SRCS = tpg_tst.cc app.cc
tpg_tst_LIBS = tpg bsa $(CPSW_LIBS)
PROGRAMS    += tpg_tst

async_poll_SRCS = async_poll.cc
async_poll_LIBS = $(CPSW_LIBS)
PROGRAMS    += async_poll

async_tst_SRCS = async_tst.cc
async_tst_LIBS = tpg $(CPSW_LIBS)
PROGRAMS    += async_tst

reg_tst_SRCS = reg_tst.cc
reg_tst_LIBS = tpg $(CPSW_LIBS)
PROGRAMS    += reg_tst

poll_tst_SRCS = poll_tst.cc
poll_tst_LIBS = tpg $(CPSW_LIBS)
PROGRAMS    += poll_tst

time_tst_SRCS = time_tst.cc
time_tst_LIBS = tpg bsa $(CPSW_LIBS)
PROGRAMS    += time_tst

bsa_tst_SRCS = bsa_tst.cc
bsa_tst_LIBS = tpg bsa $(CPSW_LIBS)
PROGRAMS    += bsa_tst

yaml_tst_SRCS = yaml_tst.cc
yaml_tst_LIBS = $(CPSW_LIBS)
PROGRAMS    += yaml_tst

strm_tst_SRCS = strm_tst.cc
strm_tst_LIBS = $(CPSW_LIBS)
PROGRAMS    += strm_tst

mps_tst_SRCS = mps_tst.cc
mps_tst_LIBS = $(CPSW_LIBS)
PROGRAMS    += mps_tst

include $(CPSW_DIR)/rules.mak
