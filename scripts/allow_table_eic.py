import sys
##############################################################################
## This file is part of 'tpg'.
## It is subject to the license terms in the LICENSE.txt file found in the 
## top-level directory of this distribution and at: 
##    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
## No part of 'tpg', including this file, 
## may be copied, modified, propagated, or distributed except according to 
## the terms contained in the LICENSE.txt file.
##############################################################################
import argparse
from psp import Pv
from PyQt4 import QtCore, QtGui
import time
from seq_ca_new import *
from evtsel import *

Do_Sim = False
Prefix = 'TPG:SYS2:2'
fixedRates = ['929kHz','71.4kHz','10.2kHz','1.02kHz','102Hz','10.2Hz','1.02Hz']
NMpsSeq = 14
charge = 100
MaxPower = 2
rateStep  = [0,13,91,10,10,10,10]
rateSuper = [0, 0, 0, 2, 2, 2, 2]
defPalette = QtGui.QPalette()
limPalette = QtGui.QPalette(QtGui.QColor(255,0,0))

def powerClass(rate):
    pclass = 3
    if rate*charge <= 300000000:
        pclass = 2
    if rate <= 10 and charge <= 300:
        pclass = 1
    if rate == 0:
        pclass = 0
    print 'power class [%u] = %u'%(rate,pclass)
    return pclass

#  returns column#, sync, nocc, power class, label
def baseColGen():
    yield( 0, 6, 0, 0,'None')
    yield( 1, 6, 1, powerClass(      1),   '1Hz')
    yield( 2, 5, 1, powerClass(     10),  '10Hz')
    yield( 3, 4, 1, powerClass(    100), '100Hz')
    yield( 4, 3, 1, powerClass(   1000),  '1kHz')
    yield( 5, 2, 1, powerClass(  10000), '10kHz')
    yield( 6, 2,10, powerClass( 100000),'100kHz')
    yield( 7, 0, 1, powerClass( 910000),'910kHz')
#    yield( 2, 6, 2, powerClass(      2),   '2Hz')
#    yield( 3, 5, 1, powerClass(     10),  '10Hz')
#    yield( 4, 5, 2, powerClass(     20),  '20Hz')
#    yield( 5, 4, 1, powerClass(    100), '100Hz')
#    yield( 6, 4, 2, powerClass(    200), '200Hz')
#    yield( 7, 3, 1, powerClass(   1000),  '1kHz')
#    yield( 8, 3, 2, powerClass(   2000),  '2kHz')
#    yield( 9, 2, 1, powerClass(  10000), '10kHz')
#    yield(10, 2, 2, powerClass(  20000), '20kHz')
#    yield(11, 1, 1, powerClass(  70000), '70kHz')
#    yield(12, 1, 2, powerClass( 140000),'140kHz')
#    yield(13, 0, 1, powerClass(1000000),  '1MHz')

class PvRate(PvInt):

    def __init__(self,pv):
        super(PvRate,self).__init__(pv)
        self.setMaximumWidth(60)
        self.pv.monitor_start()

class PvSim:

    def __init__(self,name):
        self.name  = name
        self.value = 'Empty'

    def put(self,value):
        self.value = value

    def get(self):
        return self.value

    def monitor_start(self):
        pass

    def add_monitor_callback(self,callback):
        pass

def PvW(name):
    if Do_Sim:
        return PvSim(name)
    else:
        return Pv.Pv(name)
            
class BasicSequence(object):

    def __init__(self, base, index):

        prefix = Prefix+':'+base+'%02d:'%index

        self.desc    = PvW(prefix+'DESCINSTRS')
        self.instr   = PvW(prefix+'INSTRS')
        self.idxseq  = PvW(prefix+'SEQ00IDX')
        self.seqname = PvW(prefix+'SEQ00DESC')
        self.idxseqr = PvW(prefix+'RMVIDX')
        self.seqr    = PvW(prefix+'RMVSEQ')
        self.insert  = PvW(prefix+'INS')
        self.idxrun  = PvW(prefix+'RUNIDX')
        self.start   = PvW(prefix+'SCHEDRESET')
        self.reset   = PvW(prefix+'FORCERESET')
        
        self.seq = [-1]*NMpsSeq

        self.prefix = prefix
        self.removeAll()

    def removeAll(self):
        idx = self.idxseq.get()
        try:
            while (idx>0):
                self.seqr.put(0)
                print 'Removing seq %d'%idx+' '+self.prefix
                self.idxseqr.put(idx)
                self.seqr.put(1)
                time.sleep(1.0)
                idx = self.idxseq.get()
        except TypeError:
            pass

    def remove(self, ridx):
        if ridx > 0:
            print 'Removing seq %d'%ridx+' '+self.prefix
            self.idxseqr.put(ridx)
            self.seqr.put(1)
            self.seqr.put(0)

    def _program(self, line, sync, nreq, title):

#        print line, sync, nreq, power
#        print 'Program line(%d)'%line+' sync(%d)'%sync+' nreq(%d)'%nreq+' power(%d)'%power

        #  Construct the set of instructions
        instrset = []

        if nreq>0:
            instrset.append( FixedRateSync(marker=sync,occ=1) )
            instrset.append( BeamRequest(charge) )
        if nreq>1:
            nocc = rateStep[sync]/nreq
            instrset.append( FixedRateSync(marker=rateSuper[sync],occ=nocc) )
            instrset.append( Branch.conditional(1, 0, nreq-1) )
        instrset.append( Branch.unconditional(0) )

#        title = 'Sync[%d]'%sync

        if False:
            print title+': line[%d]'%line
            i=0
            for instr in instrset:
                print 'Put instruction(%d): '%i, 
                print instr.print_()
                i += 1

        self.insert.put(0)

        self.remove( self.seq[line])

        self.desc.put(title)

        encoding = [len(instrset)]
        for instr in instrset:
            encoding = encoding + instr.encoding()
        self.instr.put( tuple(encoding) )

        self.insert.put(1)
        self.insert.put(0)

        #  Get the assigned sequence num
        idx = self.idxseq.get()
#        print 'Sequence '+self.seqname.get()+' found at index %d'%idx
        self.seq[line]=idx
        return idx

    def program(self, line, sync, nreq, power, title):
        return self._program(line, sync, nreq, title)


class MpsSequence(BasicSequence):

    def __init__(self, index):
        super(MpsSequence,self).__init__('ALW',index)

        prefix = Prefix+':ALW%02d:'%index

        self.seqp    = []
        self.pclass  = []
        for i in range(NMpsSeq):
            self.seqp  .append(PvW(prefix+'MPS%02dIDX'%i))
            self.pclass.append(PvW(prefix+'MPS%02dPCLASS'%i))

    def program(self, line, sync, nreq, power, title):
        idx = self._program(line, sync, nreq, title)

        #  Write MPS table entry
        self.seqp  [line].put(idx)
        self.pclass[line].put(power)
        return idx


class RateButton(QtGui.QPushButton):

    def __init__(self, label, parent, blsegm, rate, pclass):
        super(RateButton,self).__init__(label)

        self.setCheckable(True)
        self.setMinimumWidth(60)
        self.parent = parent
        self.blsegm = blsegm
        self.rate   = rate
        self.pclass = pclass
        self.pressed.connect(self.update)

    def update(self):
        self.parent.request(self.blsegm,self.rate)

    def limit(self, enable):
        print 'limit blsegm(%d)'%self.blsegm+' rate(%d)'%self.rate
        self.setPalette(limPalette)
        if enable:
            self.setPalette(limPalette)
        else:
            self.setPalette(defPalette)

segmName = ['LINAC']
rateBase = ['','0','00']
rateUnit = ['Hz','kHz','MHz']


class TableDisplay(QtGui.QWidget):

    def __init__(self, columnLabel, rowGen, columnGen):
        super(TableDisplay,self).__init__()

        layout = QtGui.QGridLayout()
        #  Header Row
        layout.addWidget(QtGui.QLabel('BL Segment'), 0, 0, QtCore.Qt.AlignHCenter)
        layout.addWidget(QtGui.QLabel(columnLabel), 0, 1, 1, 13, QtCore.Qt.AlignHCenter)

        self.bgroups = []
        self.seq     = []
        idx = 0
        for r in rowGen():
#            print r
            if r[0]<16:
                seq    = MpsSequence(r[0])
            elif r[0]<32:
                seq    = BasicSequence('DST',r[0]-16)
            else:
                seq    = BasicSequence('EXP',r[0]-32)
            
            layout.addWidget(QtGui.QLabel(r[1]),idx+1,0,QtCore.Qt.AlignHCenter)
            group       = QtGui.QButtonGroup()
            group.setExclusive(True)
            for p in columnGen():
                pidx=seq.program(p[0],
                                 p[1],
                                 p[2],
                                 p[3],
                                 p[4])
#                print p,pidx
                button = RateButton(p[4],self,idx,p[0],p[3])
                layout.addWidget(button,idx+1,p[0]+1,QtCore.Qt.AlignHCenter)
                group.addButton(button)
            self.bgroups.append(group)
            self.seq.append(seq)
            idx += 1

        self.setLayout(layout)

    def request(self,row,value):
        pass

class RateDisplay(QtGui.QWidget):

    def __init__(self):
        super(RateDisplay,self).__init__()

        layout = QtGui.QGridLayout()
        layout.setColumnMinimumWidth(4,20)
        #  Header Row
        layout.addWidget(QtGui.QLabel('BL Dest')     , 0, 0, QtCore.Qt.AlignHCenter)
        layout.addWidget(QtGui.QLabel('Req\nRate')   , 0, 1, QtCore.Qt.AlignHCenter)
        layout.addWidget(QtGui.QLabel('Allow\nRate') , 0, 2, QtCore.Qt.AlignHCenter)
        layout.addWidget(QtGui.QLabel('Actual\nRate'), 0, 3, QtCore.Qt.AlignHCenter)
#        layout.addWidget(QtGui.QSpacer(), 0, 4, QtCore.Qt.AlignHCenter)
        layout.addWidget(QtGui.QLabel('Allow\nClass'), 0, 5, QtCore.Qt.AlignHCenter)
        layout.addWidget(QtGui.QLabel('Deliv\nClass'), 0, 6, QtCore.Qt.AlignHCenter)

        for idx in range(len(segmName)):
            layout.addWidget(QtGui.QLabel(segmName[idx]),idx+1,0,QtCore.Qt.AlignHCenter)
            if Do_Sim==False:
                layout.addWidget(PvRate(Prefix+':DST%02d:REQRATE'%idx),idx+1,1,QtCore.Qt.AlignHCenter)
                layout.addWidget(PvRate(Prefix+':ALW%02d:REQRATE'%idx),idx+1,2,QtCore.Qt.AlignHCenter)
                #  Program the rate counter
                pv = PvW(Prefix+':RM%02d:RATEMODE'%idx)
                pv.put(0)   # Fixed Rate
                pv = PvW(Prefix+':RM%02d:FIXEDRATE'%idx)
                pv.put(0)   # 1MHz
                pv = PvW(Prefix+':RM%02d:DESTMODE'%idx)
                pv.put(2)   # Inclusion
                pv = PvW(Prefix+':RM%02d:DESTMASK'%idx)
                pv.put(1<<idx)
                #  Toggle it to load new programming
                pv = PvW(Prefix+':RM%02d:CTRL'%idx)
                pv.put(0)
                pv.put(1)
#                pv.put(0)
                layout.addWidget(PvRate(Prefix+':RM%02d:CNT'%idx),idx+1,3,QtCore.Qt.AlignHCenter)
                layout.addWidget(PvRate(Prefix+':ALW%02d:MPSSTATE'%idx),idx+1,5,QtCore.Qt.AlignHCenter)
                layout.addWidget(PvRate(Prefix+':ALW%02d:MPSLATCH'%idx),idx+1,6,QtCore.Qt.AlignHCenter)

        self.setLayout(layout)


class AllowEngine:

    def __init__(self, engine, buttons):
        self.buttons = buttons

        prefix = Prefix+':ALW%02d:'%engine

        print prefix

        self.pvsetstate = PvW(prefix+'MPSSETSTATE')

        self.pvstate = PvW(prefix+'MPSSTATE')
        self.pvstate.monitor_start()
        self.pvstate.add_monitor_callback(self.updateState)

        self.pvlatch = PvW(prefix+'MPSLATCH')
        self.pvlatch.monitor_start()
        self.pvlatch.add_monitor_callback(self.updateLatch)

        self.state = 0
        self.buttons[0].limit(True)
        self.buttons[0].setChecked(True)
        self.setLatch(self.pvlatch.value)
        self.updateState(0)

    def setLatch(self,value):
        self.request = value
        print 'AllowEngine setLatch value[%u] state[%u]'%(value,self.pvstate.value)
        if value <= self.pvstate.value:
            self.pvsetstate.put(value)

    def updateState(self,err):
        value = self.pvstate.value
        if value >= len(self.buttons):
            value = len(self.buttons)-1
        if value != self.state:
            lim = 0
            for i in range(len(self.buttons)):
                self.buttons[i].limit(False)
                if self.buttons[i].pclass <= value:
                    self.buttons[i].setEnabled(True)
                    lim = i
                else:
                    self.buttons[i].setEnabled(False)
            print 'updateState from(%d)'%self.state+' to(%d)'%value+' yields %d'%lim
            self.buttons[lim].limit(True)
            self.state = value

    def updateLatch(self,err):
        value = self.pvlatch.value
        print 'updateLatch to(%d)'%value
        self.buttons[value].setChecked(True)

def allowRowGen():
    #  returns engine number, label
    for index in range(len(segmName)):
        yield (index, segmName[index])

def allowColGen():
    #  returns column#, sync, nocc, power class, label
    i = 0
    q = (0,0,0,100,'None')
    for p in baseColGen():
        if p[3] > q[3]:
            while (i<p[3]):
                yield (i,q[1],q[2],q[3],q[4])
                i += 1
        q = p
    while (i<=MaxPower):
        yield (i,q[1],q[2],q[3],q[4])
        i += 1

class AllowTable(TableDisplay):

    def __init__(self):
        super(AllowTable,self).__init__('Allow Table',allowRowGen,allowColGen)

        self.engines = []
        for row in allowRowGen():
            eng = AllowEngine(row[0],self.bgroups[row[0]].buttons())
            self.engines.append(eng)
        self.request(row[0],0)

    def request(self,row,value):
        print 'Request row %d'%row+' value %d'%value
        self.engines[row].setLatch(value)
#        self.seq[row].idxrun.put(value+2)
#        self.seq[row].start.put(0)
#        self.seq[row].reset.put(1)
#        self.seq[row].reset.put(0)

    def setState(self,row,value):
        self.bgroups[row].buttons()[self.limits[row]].limit(False)
        self.bgroups[row].buttons()[value].limit(True)

    def setLatch(self,row,value):
        self.bgroups[row].buttons()[value].setChecked(True)

def requestRowGen():
    #  returns engine number, label, destination, required mask
    for index in range(len(segmName)):
        yield (index+16, segmName[index], index%3, (1<<(index%3)))

def requestColGen():
    for p in baseColGen():
        yield p
    
class RateTable(TableDisplay):

    def __init__(self):
        super(RateTable,self).__init__('Rate Request',requestRowGen,requestColGen)

        idx=0
        for row in requestRowGen():
            if row[0]>15:
                prefix = Prefix+':DST%02d:'%(row[0]-16)
                dest = PvW(prefix+'DEST')
                dest.put(row[2])
                reqd = PvW(prefix+'REQMASK')
                reqd.put(row[3])
                self.bgroups[idx].buttons()[0].setChecked(True)
                self.request(idx,0)
                idx += 1

    def request(self,row,value):
        print 'Rate request(%d)'%row+' start(%d)'%(value+2)
        self.seq[row].idxrun.put(value+2)
        self.seq[row].start.put(0)
        self.seq[row].reset.put(1)
        self.seq[row].reset.put(0)

class Ui_AllowWindow(object):
    def setupUi(self, MainWindow):
        MainWindow.setObjectName(QtCore.QString.fromUtf8("MainWindow"))
        self.centralWidget = QtGui.QWidget(MainWindow)
        self.centralWidget.setObjectName("centralWidget")

        vlayout = QtGui.QVBoxLayout()
        vlayout.addStretch()

        if True:
            hlayout = QtGui.QHBoxLayout()
            hlayout.addStretch()
            hlayout.addWidget(QtGui.QLabel('Charge: %u pC'%charge))
            hlayout.addStretch()
            vlayout.addLayout(hlayout)
            vlayout.addStretch()

        if True:
            hlayout = QtGui.QHBoxLayout()
            hlayout.addStretch()
            hlayout.addWidget(AllowTable())
            hlayout.addStretch()
            vlayout.addLayout(hlayout)
            vlayout.addStretch()

        if True:
            hlayout = QtGui.QHBoxLayout()
            hlayout.addStretch()
            hlayout.addWidget(RateTable())
            hlayout.addStretch()
            vlayout.addLayout(hlayout)

            vlayout.addStretch()

        if True:
            hlayout = QtGui.QHBoxLayout()
            hlayout.addStretch()
            hlayout.addWidget(RateDisplay())
            hlayout.addStretch()
            vlayout.addLayout(hlayout)

        self.centralWidget.setLayout(vlayout)
        self.centralWidget.resize(800,600)
        MainWindow.resize(800,600)
        MainWindow.setWindowTitle(sys.argv[0])


if __name__ == '__main__':
    print QtCore.PYQT_VERSION_STR

    parser = argparse.ArgumentParser(description='eic allow table example')
    parser.add_argument('--pv' , help="TPG pv base", default='TPG:SYS2:2')
    parser.add_argument('--q', help="bunch charge", type=int, default=100)
    args = parser.parse_args()
    Prefix = args.pv
    charge = args.q

    app = QtGui.QApplication([])
    MainWindow = QtGui.QMainWindow()
    ui = Ui_AllowWindow()
    ui.setupUi(MainWindow)
    MainWindow.updateGeometry()

    MainWindow.show()
    sys.exit(app.exec_())
