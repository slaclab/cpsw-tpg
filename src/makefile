CCACHE_DISABLE=1


#CPSW_DIR=/afs/slac/g/lcls/package/cpsw/framework/R4.4.1/src
CPSW_DIR=/afs/slac/g/lcls/package/cpsw/framework/R4.2.0/src
BSA_DIR=/afs/slac/g/lcls/package/timing/bsa
BSA_VERSION=R1.2.0
BSA=$(BSA_DIR)/$(BSA_VERSION)
#FIRML_DIR=/afs/slac/g/lcls/package/cpsw/FirmwareLoader/R1.3.0/deviceLibrary/src
FIRML_DIR=/afs/slac/g/lcls/package/cpsw/FirmwareLoader/R1.2.0/deviceLibrary/src

# may override CPSW_DIR / DEVICELIB_DIR from release.mak
# must set SRCDIR (is redefined by recursive make)
SRCDIR=.
-include $(SRCDIR)/release.mak

include $(CPSW_DIR)/defs.mak

LDFLAGS+= -Wl,-Bstatic -static-libgcc
LDFLAGS+=-L$(BSA)/$(TARCH)/lib

INCLUDE_DIRS += $(BSA)/$(TARCH)/include 
INSTALL_DIR=../

INCLUDE_DIRS += $(FIRML_DIR)
LDFLAGS+=-L$(FIRML_DIR)/O.$(TARCH) -ldeviceLib

CXXFLAGS = -g -DFRAMEWORK_R3_4
HEADERS  = TPG.hh SeqState.hh SeqJump.hh SeqMem.hh RamControl.hh Sy56040.hh AxiStreamDmaRingWrite.hh RssiCore.hh
HEADERS += sequence_engine.hh sequence_engine_cpsw.hh sequence_engine_yaml.hh
HEADERS += tpg.hh tpg_cpsw.hh user_sequence.hh event_selection.hh frame.hh tpg_yaml.hh

tpg_SRCS  = TPG.cc SeqState.cc SeqJump.cc SeqMem.cc RamControl.cc Sy56040.cc AxiStreamDmaRingWrite.cc RssiCore.cc
tpg_SRCS += sequence_engine_cpsw.cc sequence_engine_yaml.cc
tpg_SRCS += tpg_cpsw.cc user_sequence.cc event_selection.cc tpg_yaml.cc

ncpsw_SRCS = Reg.cc

hps_SRCS = hps_utils.cc

#STATIC_LIBRARIES+=tpg
#STATIC_LIBRARIES+=ncpsw
#STATIC_LIBRARIES+=hps

tpg_tst_SRCS = tpg_tst.cc app.cc
tpg_tst_LIBS = tpg bsa hps $(CPSW_LIBS)
PROGRAMS    += tpg_tst

tpg_dump_SRCS = tpg_dump.cc app.cc
tpg_dump_LIBS = tpg bsa hps $(CPSW_LIBS)
PROGRAMS    += tpg_dump

async_poll_SRCS = async_poll.cc
async_poll_LIBS = $(CPSW_LIBS)
#PROGRAMS    += async_poll

async_tst_SRCS = async_tst.cc
async_tst_LIBS = tpg $(CPSW_LIBS)
#PROGRAMS    += async_tst

reg_tst_SRCS = reg_tst.cc
reg_tst_LIBS = tpg $(CPSW_LIBS)
PROGRAMS    += reg_tst

poll_tst_SRCS = poll_tst.cc
poll_tst_LIBS = tpg $(CPSW_LIBS)
#PROGRAMS    += poll_tst

time_tst_SRCS = time_tst.cc
time_tst_LIBS = tpg bsa $(CPSW_LIBS)
#PROGRAMS    += time_tst

bsa_tst_SRCS = bsa_tst.cc
bsa_tst_LIBS = tpg bsa $(CPSW_LIBS)
#PROGRAMS    += bsa_tst

yaml_tst_SRCS = yaml_tst.cc
yaml_tst_LIBS = $(CPSW_LIBS)
#PROGRAMS    += yaml_tst

strm_tst_SRCS = strm_tst.cc
strm_tst_LIBS = $(CPSW_LIBS)
#PROGRAMS    += strm_tst

mps_tst_SRCS = mps_tst.cc
mps_tst_LIBS = tpg $(CPSW_LIBS)
#PROGRAMS    += mps_tst

mpssim_tst_SRCS = mpssim_tst.cc
mpssim_tst_LIBS = ncpsw pthread rt dl
#PROGRAMS    += mpssim_tst

hps_control_SRCS = hps_control.cc
hps_control_LIBS = $(CPSW_LIBS) hps pthread rt dl
PROGRAMS    += hps_control

hps_checkout_SRCS = hps_checkout.cc
hps_checkout_LIBS = $(CPSW_LIBS) hps pthread rt dl
PROGRAMS    += hps_checkout

hps_peek_SRCS = hps_peek.cc
hps_peek_LIBS = $(CPSW_LIBS) hps pthread rt dl
PROGRAMS    += hps_peek

hps_eyescan_SRCS = hps_eyescan.cc
hps_eyescan_LIBS = $(CPSW_LIBS) hps pthread rt dl
#PROGRAMS    += hps_eyescan

cryo_peek_SRCS = cryo_peek.cc
cryo_peek_LIBS = $(CPSW_LIBS) hps pthread rt dl
#PROGRAMS    += cryo_peek

tprtest_SRCS = tprtest.cc tpr.cc
tprtest_LIBS = $(CPSW_LIBS) hps pthread rt dl
PROGRAMS    += tprtest

tpr_trig_SRCS = tpr_trig.cc tpr.cc
tpr_trig_LIBS = hps pthread rt dl
PROGRAMS    += tpr_trig

tpr_scan_SRCS = tpr_scan.cc
tpr_scan_LIBS = ncpsw pthread rt dl
#PROGRAMS    += tpr_scan

rfof_stream_SRCS = rfof_stream.cc
rfof_stream_LIBS = $(CPSW_LIBS) hps pthread rt dl
#PROGRAMS    += rfof_stream

prom_read_SRCS = prom_read.cc
prom_read_LIBS = $(CPSW_LIBS) rt dl
PROGRAMS = prom_read

include $(CPSW_DIR)/rules.mak
