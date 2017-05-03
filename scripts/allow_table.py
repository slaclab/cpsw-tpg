import sys
import argparse
from psp import Pv
from PyQt4 import QtCore, QtGui
import time
from seq_ca_new import *
from evtsel import *

Do_Sim = False
Prefix = 'TPG2'
fixedRates = ['929kHz','71.4kHz','10.2kHz','1.02kHz','102Hz','10.2Hz','1.02Hz']
NMpsSeq = 14

defPalette = QtGui.QPalette()
limPalette = QtGui.QPalette(QtGui.QColor(255,0,0))

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

    def __init__(self, index):

        self.desc    = PvW(Prefix+':ENG%d:DESCINSTRSET'%index)
        self.instr   = PvW(Prefix+':ENG%d:WFINSTRUCTION'%index)
        self.idxseq  = PvW(Prefix+':ENG%d:IDXSEQ0'%index)
        self.seqname = PvW(Prefix+':ENG%d:DESCSEQ0'%index)
        self.idxseqr = PvW(Prefix+':ENG%d:IDXSEQREMOVE'%index)
        self.seqr    = PvW(Prefix+':ENG%d:SEQREMOVE'%index)
        self.insert  = PvW(Prefix+':ENG%d:INSERTINSTRSET'%index)
        self.idxrun  = PvW(Prefix+':ENG%d:IDXSEQRESET'%index)
        self.start   = PvW(Prefix+':ENG%d:JUMPSEQRESET'%index)
        self.reset   = PvW(Prefix+':ENG%d:PROMPTRESET'%index)
        
        self.seq = [-1]*NMpsSeq

        self.removeAll()

    def removeAll(self):
        idx = self.idxseq.get()
        try:
            while (idx>0):
#                print 'Removing seq %d'%idx
                self.idxseqr.put(idx)
                self.seqr.put(1)
                self.seqr.put(0)
                idx = self.idxseq.get()
        except TypeError:
            pass

    def remove(self, ridx):
        if ridx > 0:
            print 'Removing seq %d'%ridx
            self.idxseqr.put(ridx)
            self.seqr.put(1)
            self.seqr.put(0)

    def _program(self, line, sync, nreq):

#        print line, sync, nreq, power
#        print 'Program line(%d)'%line+' sync(%d)'%sync+' nreq(%d)'%nreq+' power(%d)'%power

        #  Construct the set of instructions
        instrset = []

        if nreq>0:
            instrset.append( FixedRateSync(marker=sync,occ=1) )
            instrset.append( BeamRequest(0xabcd) )
        while nreq>1:
            instrset.append( FixedRateSync(marker=sync-1,occ=1) )
            instrset.append( BeamRequest(0xabcd) )
            nreq -= 1
        instrset.append( Branch.unconditional(0) )

        if False:
            i=0
            for instr in instrset:
                print 'Put instruction(%d): '%i, 
                print instr.print_()
                i += 1

        self.insert.put(0)

        self.remove( self.seq[line])

        title = 'Sync[%d]'%sync
        self.desc.put(title)
        for instr in instrset:
            self.instr.put( instr.encoding() )
        self.insert.put(1)

        #  Get the assigned sequence num
        idx = self.idxseq.get()
#        print 'Sequence '+self.seqname.get()+' found at index %d'%idx
        self.seq[line]=idx
        return idx

    def program(self, line, sync, nreq, power):
        return self._program(line, sync, nreq)


class MpsSequence(BasicSequence):

    def __init__(self, index):
        super(MpsSequence,self).__init__(index)

        self.seqp    = []
        self.pclass  = []
        for i in range(NMpsSeq):
            self.seqp  .append(PvW(Prefix+':ENG%d'%index+':MPSCTRL%d_IDXSEQ'%i))
            self.pclass.append(PvW(Prefix+':ENG%d'%index+':MPSCTRL%d_PCLASS'%i))

    def program(self, line, sync, nreq, power):
        idx = self._program(line, sync, nreq)

        #  Write MPS table entry
        self.seqp  [line].put(idx)
        self.pclass[line].put(power)
        return idx


class RateButton(QtGui.QPushButton):

    def __init__(self, label, parent, blsegm, rate):
        super(RateButton,self).__init__(label)

        self.setCheckable(True)
        self.setMinimumWidth(60)
        self.parent = parent
        self.blsegm = blsegm
        self.rate   = rate
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

segmName = ['LINAC','SXU','HXU','LINAC*']
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
            print r
            if r[0]<16:
                seq    = MpsSequence(r[0])
            else:
                seq    = BasicSequence(r[0])
            
            layout.addWidget(QtGui.QLabel(r[1]),idx+1,0,QtCore.Qt.AlignHCenter)
            group       = QtGui.QButtonGroup()
            group.setExclusive(True)
            for p in columnGen():
                pidx=seq.program(p[0],
                                 p[1],
                                 p[2],
                                 p[3])
                print p,pidx
                button = RateButton(p[4],self,idx,p[0])
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
        #  Header Row
        layout.addWidget(QtGui.QLabel('BL Segment'), 0, 0, QtCore.Qt.AlignHCenter)
        layout.addWidget(QtGui.QLabel('Request')   , 0, 1, QtCore.Qt.AlignHCenter)
        layout.addWidget(QtGui.QLabel('Allow')     , 0, 2, QtCore.Qt.AlignHCenter)
        layout.addWidget(QtGui.QLabel('Actual')    , 0, 3, QtCore.Qt.AlignHCenter)

        for idx in range(3):
            layout.addWidget(QtGui.QLabel(segmName[idx]),idx+1,0,QtCore.Qt.AlignHCenter)
            if Do_Sim==False:
                layout.addWidget(PvInt(Prefix+':ENG%d:REQRATE'%(idx+16)),idx+1,1,QtCore.Qt.AlignHCenter)
                layout.addWidget(PvInt(Prefix+':ENG%d:REQRATE'%(idx+ 0)),idx+1,2,QtCore.Qt.AlignHCenter)
                #  Program the rate counter
                pv = PvW(Prefix+':RM%d:RATEMODE'%idx)
                pv.put(0)   # Fixed Rate
                pv = PvW(Prefix+':RM%d:FIXEDRATE'%idx)
                pv.put(0)   # 1MHz
                pv = PvW(Prefix+':RM%d:DESTMODE'%idx)
                pv.put(2)   # Inclusion
                pv = PvW(Prefix+':RM%d:DESTMASK'%idx)
                pv.put(1<<idx)
                #  Toggle it to load new programming
                pv = PvW(Prefix+':RM%d:CTRL'%idx)
                pv.put(0)
                pv.put(1)
                layout.addWidget(PvInt(Prefix+':RM%d:COUNTER'%idx),idx+1,3,QtCore.Qt.AlignHCenter)

        self.setLayout(layout)


def allowRowGen():
    #  returns engine number, label
    for index in range(3):
        yield (index, segmName[index])

def allowColGen():
    #  returns column#, sync, nocc, power class
    yield (0, 6, 0, 0, 'None')
    for index in range(13):
        label = '%d'%(1+(index%2))
        label += rateBase[(index/2)%3]
        label += rateUnit[(index/6)]
        yield (index+1, 6-index/2, 1+(index%2), index+1, label)

class AllowEngine:

    def __init__(self, engine, buttons):
        self.buttons = buttons
        self.state   = len(buttons)-1

        self.pvsetstate = PvW(Prefix+':ENG%d:MPSSTATE'%engine)

        self.pvstate = PvW(Prefix+':ENG%d:MPSST_STATE'%engine)
        self.pvstate.monitor_start()
        self.pvstate.add_monitor_callback(self.updateState)

        self.pvlatch = PvW(Prefix+':ENG%d:MPSST_LATCH'%engine)
        self.pvlatch.monitor_start()
        self.pvlatch.add_monitor_callback(self.updateLatch)

        self.state = 0
        self.updateState(0)
        self.setLatch(self.pvlatch.value)

    def setLatch(self,value):
        if value <= self.pvstate.value:
            self.pvsetstate.put(value)

    def updateState(self,err):
        #  Limit state to values we can display
        value = self.pvstate.value
        if value >= len(self.buttons):
            value = len(self.buttons)-1
        print 'updateState from(%d)'%self.state+' to(%d)'%value
        if value != self.state:
            self.buttons[self.state].limit(False)
            self.state = value
            self.buttons[self.state].limit(True)

    def updateLatch(self,err):
        value = self.pvlatch.value
        print 'updateLatch to(%d)'%value
        self.buttons[value].setChecked(True)

class AllowTable(TableDisplay):

    def __init__(self):
        super(AllowTable,self).__init__('MPS Class',allowRowGen,allowColGen)

        self.engines = []
        for row in allowRowGen():
            eng = AllowEngine(row[0],self.bgroups[row[0]].buttons())
            self.engines.append(eng)

    def request(self,row,value):
        print 'Request row %d'%row+' value %d'%value
        self.engines[row].setLatch(value)

    def setState(self,row,value):
        self.bgroups[row].buttons()[self.limits[row]].limit(False)
        self.bgroups[row].buttons()[value].limit(True)

    def setLatch(self,row,value):
        self.bgroups[row].buttons()[value].setChecked(True)

def requestRowGen():
    #  returns engine number, label, destination, required mask
    for index in range(4):
        yield (index+16, segmName[index], index%3, (1<<(index%3))|1)

def requestColGen():
    #  returns column#, sync, nocc, power class
    yield (0, 6, 0, 0, 'None')
#    for index in range(7):
#        label = '1'
#        label += rateBase[index%3]
#        label += rateUnit[index/3]
#        yield (index+1, 6-index, 1, index+1, label)
    for index in range(13):
        label = '%d'%(1+(index%2))
        label += rateBase[(index/2)%3]
        label += rateUnit[(index/6)]
        yield (index+1, 6-index/2, 1+(index%2), index+1, label)
    
class RateTable(TableDisplay):

    def __init__(self):
        super(RateTable,self).__init__('Rate Request',requestRowGen,requestColGen)

        idx=0
        for row in requestRowGen():
            if row[0]>15:
                dest = PvW(Prefix+':ENG%d:DESTINATION'%row[0])
                dest.put(row[2])
                reqd = PvW(Prefix+':ENG%d:REQUIREDMASK'%row[0])
                reqd.put(row[3])
                self.bgroups[idx].buttons()[0].setChecked(True)
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

    app = QtGui.QApplication([])
    MainWindow = QtGui.QMainWindow()
    ui = Ui_AllowWindow()
    ui.setupUi(MainWindow)
    MainWindow.updateGeometry()

    MainWindow.show()
    sys.exit(app.exec_())
