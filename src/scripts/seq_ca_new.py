import sys
import argparse
from psp import Pv
from PyQt4 import QtCore, QtGui
import time

NBeamSeq   = 16
NExptSeq   = 17
NMpsSeq    = 16
NPowers    = 4

evtsel     = ['Fixed Rate','AC Rate','Sequence']
fixedRates = ['929kHz','71.4kHz','10.2kHz','1.02kHz','102Hz','10.2Hz','1.02Hz']
intervals  = [1, 13, 91, 910, 9100, 91000, 910000]
Prefix='TPG2'

#  Instruction encoding/decoding
instrName  = ['FixedRateSync', 'ACRateSync', 'Branch', 'CheckPoint', 'BeamRequest', 'ExptRequest']

class QHexValidator(QtGui.QValidator):

    def __init__(self):
        super(QHexValidator,self).__init__()

    def setRange(self,rlo,rhi):
        self.rlo = rlo
        self.rhi = rhi

    def validate(self,text,pos):
        if text.size()==0:
            return (QtGui.QValidator.Intermediate,pos)
        (v,ok) = text.toUInt(16)
        if ok == False:
            return (QtGui.QValidator.Invalid,pos)
        if v < self.rlo or v > self.rhi:
            return (QtGui.QValidator.Invalid,pos)
        return (QtGui.QValidator.Acceptable,pos)

class Instruction(object):

    def __init__(self, args):
        self.args = args

    def encoding(self):
        return tuple(self.args)

class FixedRateSync(Instruction):

    opcode = 0

    def __init__(self, marker, occ):
        super(FixedRateSync, self).__init__( (self.opcode, marker, occ) )

    def print_(self):
        return 'FixedRateSync(%s) # occ(%d)'%(fixedRates[self.args[1]],self.args[2])
    

class Branch(Instruction):

    opcode = 2

    def __init__(self, args):
        super(Branch, self).__init__(args)

    @classmethod
    def unconditional(cls, line):
        return cls((cls.opcode, line))

    @classmethod
    def conditional(cls, line, counter, value):
        return cls((cls.opcode, line, counter, value))

    def print_(self):
        if len(self.args)==2:
            return 'Branch unconditional to line %d'%self.args[1]
        else:
            return 'Branch to line %d until ctr%d=%d'%(self.args[1:])
    
class BeamRequest(Instruction):

    opcode = 4
    
    def __init__(self, charge):
        super(BeamRequest, self).__init__((self.opcode, charge))

    def print_(self):
        return 'BeamRequest charge %d'%self.args[1]


class ControlRequest(Instruction):

    opcode = 5
    
    def __init__(self, word):
        super(ControlRequest, self).__init__((self.opcode, word))

    def print_(self):
        return 'ControlRequest word %d'%self.args[1]


class Sequencer(QtGui.QWidget):
    def __init__(self, index):
        super(Sequencer,self).__init__()

        self.index  = index

        self.intervalv = QtGui.QIntValidator()
        self.intervalv.setRange(1,2000000)
        self.interval  = QtGui.QLineEdit('1')
        self.interval.setValidator(self.intervalv)

        self.sync     = QtGui.QComboBox()
        self.sync.addItems(fixedRates)
        self.sync.setCurrentIndex(6)  # Default to 1Hz Sync

        self.execb = QtGui.QPushButton('Exec')
        self.execb.pressed.connect(self.execute)

        self.stopb  = QtGui.QPushButton('Stop')
        self.stopb.pressed.connect(self.stop)

        self.descw = QtGui.QLineEdit('-')
        self.descw.setEnabled(False)

        self.ninstr = Pv.Pv(Prefix+':ENG%d:COUNTERINSTRUCTION_RBV'%index)
        self.ninstrw = QtGui.QLineEdit('0')
        self.ninstrw.setEnabled(False)

        self.subidx = QtGui.QLineEdit('-1')
        self.subidx.setEnabled(False)

        self.log = QtGui.QTextEdit()

        self.desc    = Pv.Pv(Prefix+':ENG%d:DESCINSTRSET'%index)
        self.instr   = Pv.Pv(Prefix+':ENG%d:WFINSTRUCTION'%index)
        self.idxseq  = Pv.Pv(Prefix+':ENG%d:IDXSEQ0'%index)
        self.seqname = Pv.Pv(Prefix+':ENG%d:DESCSEQ0'%index)
        self.idxseqr = Pv.Pv(Prefix+':ENG%d:IDXSEQREMOVE'%index)
        self.seqr    = Pv.Pv(Prefix+':ENG%d:SEQREMOVE'%index)
        self.insert  = Pv.Pv(Prefix+':ENG%d:INSERTINSTRSET'%index)
        self.idxrun  = Pv.Pv(Prefix+':ENG%d:IDXSEQRESET'%index)
        self.start   = Pv.Pv(Prefix+':ENG%d:JUMPSEQRESET'%index)
        self.reset   = Pv.Pv(Prefix+':ENG%d:PROMPTRESET'%index)

    def wait(self, rset):
        (intv,ok) = self.interval.text().toInt()
        iinstr = len(rset)
        if intv >= 2048:
            #  Wait for 2048 intervals
            rset.append( FixedRateSync(marker=0, occ=2048) )
            if intv >= 4096:
                #  Branch conditionally to previous instruction
                rset.append( Branch.conditional(line=iinstr, counter=3, value=int(intv/2048)-1) )

        rint = intv%2048
        if rint:
            rset.append( FixedRateSync(marker=0, occ=rint ) )

    def remove(self):
        return -1   # By default, remove all user sequences

    def title(self):
        (intv,ok) = self.interval.text().toInt()
        rate = 1300000./1.4/float(intv)  
        if rate > 1000.:
            return '%f kHz'%(rate*0.001)
        else:
            return '%f Hz'%rate

    def post(self,idx,title,ninstr):
        pass        # post execute hook

    def execute(self):

        #  Construct the set of instructions
        (intv,ok) = self.interval.text().toInt()
        print intv

        instrset = []

        #  Insert global sync instruction (1Hz?)
        instrset.append(FixedRateSync(marker=self.sync.currentIndex(),occ=1))
        
        #  How many times to repeat beam requests in "1 second"
        #  nint = 910000/intv
        #  Global sync counts as 1 cycle
        nint = (910000-1)/intv

        rint = nint % 256
        if rint:
            startreq = len(instrset)
            instrset.append( self.request() )
            self.wait( instrset )
            instrset.append( Branch.conditional(startreq, 0, rint-1) )
            nint = nint - rint

        rint = (nint/256) % 256
        if rint:
            startreq = len(instrset)
            instrset.append( self.request() )
            self.wait( instrset )
            instrset.append( Branch.conditional(startreq, 0, 255) )
            instrset.append( Branch.conditional(startreq, 1, rint-1) )
            nint = nint - rint*256

        rint = (nint / (256*256)) % 256
        if rint:
            startreq = len(instrset)
            instrset.append( self.request() )
            self.wait( instrset )
            instrset.append( Branch.conditional(startreq, 0, 255) )
            instrset.append( Branch.conditional(startreq, 1, 255) )
            instrset.append( Branch.conditional(startreq, 2, rint-1) )
            nint = nint - rint*256*256

        instrset.append(self.request())

        #  Unconditional branch (opcode 2) to instruction 0 (1Hz sync)
        instrset.append( Branch.unconditional(0) )

        i=0
        for instr in instrset:
            print 'Put instruction(%d): '%i, 
            print instr.print_()
            self.log.append(instr.print_())
            i += 1

        self.insert.put(0)

        # Remove existing sub sequences
        ridx = self.remove()
        print 'Remove %d'%ridx
        if ridx < 0:
            idx = self.idxseq.get()
            while (idx>0):
                print 'Removing seq %d'%idx
                self.log.append('Removing seq %d'%idx)
                self.idxseqr.put(idx)
                self.seqr.put(1)
                self.seqr.put(0)
                idx = self.idxseq.get()
        elif ridx > 1:
            print 'Removing seq %d'%ridx
            self.log.append('Removing seq %d'%ridx)
            self.idxseqr.put(ridx)
            self.seqr.put(1)
            self.seqr.put(0)
            
        # rate in Hz
        title = self.title()
        self.desc.put(title)
        self.descw.setText(title)
        print title
        self.log.append(title)
        self.log.append('----')

        for instr in instrset:
            self.instr.put( instr.encoding() )

        time.sleep(0.1)

        ninstr = self.ninstr.get()
        self.ninstrw.setText('%d'%ninstr)

        self.insert.put(1)

        #  Get the assigned sequence num
        idx = self.idxseq.get()
        print 'Sequence '+self.seqname.get()+' found at index %d'%idx
        self.subidx.setText('%d'%idx)

        self.post(idx,title,ninstr)

        #  Start it
        self.idxrun.put(idx)
        self.start .put(0)
        self.reset .put(1)
        self.reset .put(0)

    def stop(self):
        self.idxrun.put(0)   # the do-nothing sequence
        self.start .put(0)
        self.reset .put(1)
        self.reset .put(0)

class BeamControl(Sequencer):

    def __init__(self, idx):
        super(BeamControl,self).__init__(idx+NMpsSeq)
        
        vbox = QtGui.QVBoxLayout()

        self.reqv = QtGui.QIntValidator()
        self.reqv.setRange(0,0xffff)
        self.req  = QtGui.QLineEdit('1')
        self.req.setValidator(self.reqv)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Charge (pC)') )
        hbox.addWidget( self.req )
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Interval') )
        hbox.addWidget( self.interval )
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Sync') )
        hbox.addWidget(self.sync)
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget(self.execb)
        hbox.addWidget(self.stopb)
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Rate') )
        hbox.addWidget( self.descw )
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Instr') )
        hbox.addWidget( self.ninstrw )
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Index') )
        hbox.addWidget( self.subidx )
        hbox.addStretch()
        vbox.addLayout(hbox)

        vbox.addWidget(self.log)

        self.setLayout(vbox)

    def request(self):
        return BeamRequest(self.req.text().toUInt(16)[0])
        
class ExptControl(Sequencer):

    def __init__(self, idx):
        super(ExptControl,self).__init__(idx+NMpsSeq+NBeamSeq)

        vbox = QtGui.QVBoxLayout()

        self.reqv = QHexValidator()
        self.reqv.setRange(0,0xffff)
        self.req  = QtGui.QLineEdit('1')
        self.req.setValidator(self.reqv)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Request (hex)') )
        hbox.addWidget( self.req )
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Interval') )
        hbox.addWidget( self.interval )
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Sync') )
        hbox.addWidget(self.sync)
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget(self.execb)
        hbox.addWidget(self.stopb)
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Title') )
        hbox.addWidget( self.descw )
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Instr') )
        hbox.addWidget( self.ninstrw )
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Index') )
        hbox.addWidget( self.subidx )
        hbox.addStretch()
        vbox.addLayout(hbox)

        vbox.addWidget(self.log)

        self.setLayout(vbox)

    def request(self):
        return ControlRequest(self.req.text().toUInt(16)[0])

    def title(self):
        (intv,ok) = self.interval.text().toInt()
        rate = 1300000./1.4/float(intv)  
        req = self.req.text().toUInt(16)[0]
        if rate > 1000.:
            return '0x%x'%req+' at %f kHz'%(rate*0.001)
        else:
            return '0x%x'%req+' at %f Hz'%rate

        
class MpsControl(Sequencer):

    def __init__(self, idx):
        super(MpsControl,self).__init__(idx)

        self.sequences = {}

        vbox = QtGui.QVBoxLayout()

        self.power = QtGui.QComboBox()
        for i in range(NPowers):
            self.power.addItem('P%d'%i)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Power Class') )
        hbox.addWidget( self.power )
        hbox.addStretch()
        vbox.addLayout(hbox)
        self.power.currentIndexChanged.connect(self.powerChanged)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Interval') )
        hbox.addWidget( self.interval )
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Sync') )
        hbox.addWidget(self.sync)
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget(self.execb)
        hbox.addWidget(self.stopb)
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Title') )
        hbox.addWidget( self.descw )
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Instr') )
        hbox.addWidget( self.ninstrw )
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Index') )
        hbox.addWidget( self.subidx )
        hbox.addStretch()
        vbox.addLayout(hbox)

        vbox.addWidget(self.log)

        self.setLayout(vbox)

    def remove(self):
        # Should only remove the sequence associated with the current power or none
        power = self.power.currentIndex()
        if power in self.sequences:
            idx = self.sequences[power][0]
            del self.sequences[power]
            return idx
        else:
            return 0

    def powerChanged(self,power):
        if power in self.sequences:
            self.subidx .setText('%d'%self.sequences[power][0])
            self.descw  .setText(self.sequences[power][1])
            self.ninstrw.setText('%d'%self.sequences[power][2])
        else:
            self.subidx .setText('-1')
            self.descw  .setText('-')
            self.ninstrw.setText('0')

    def title(self):
        (intv,ok) = self.interval.text().toInt()
        rate = 1300000./1.4/float(intv)  
        power = self.power.currentIndex()
        if rate > 1000.:
            return 'P%d at %f kHz'%(power,rate*0.001)
        else:
            return 'P%d at %f Hz'%(power,rate)

    def request(self):
        return BeamRequest(1)
        
    def post(self, idx, title, ninstr):
        power = self.power.currentIndex()
        self.sequences[ power ] = (idx,title,ninstr)
        print 'Sequence [power=%d] = %d %s [%d]'%(power,idx,title,ninstr)

class SeqDisplay(QtGui.QStackedWidget):

    def __init__(self, seqType, beamIdx, exptIdx, mpsIdx):
        super(SeqDisplay,self).__init__()

        #  Beam sequence control display
        beamDisplay = QtGui.QStackedWidget()
        for i in range(NBeamSeq):
            beamDisplay.addWidget( BeamControl(i) )
        beamIdx.currentIndexChanged.connect(beamDisplay.setCurrentIndex)
        self.addWidget(beamDisplay)

        #  Experiment sequence control display
        exptDisplay = QtGui.QStackedWidget()
        for i in range(NExptSeq):
            exptDisplay.addWidget( ExptControl(i) )
        exptIdx.currentIndexChanged.connect(exptDisplay.setCurrentIndex)
        self.addWidget(exptDisplay)

        #  MPS sequence control display
        mpsDisplay = QtGui.QStackedWidget()
        for i in range(NMpsSeq):
            mpsDisplay.addWidget( MpsControl(i) )
        mpsIdx.currentIndexChanged.connect(mpsDisplay.setCurrentIndex)
        self.addWidget(mpsDisplay)

        seqType.currentIndexChanged.connect(self.setCurrentIndex)


class Ui_MainWindow(object):
    def setupUi(self, MainWindow):
        MainWindow.setObjectName(QtCore.QString.fromUtf8("MainWindow"))
        self.centralWidget = QtGui.QWidget(MainWindow)
        self.centralWidget.setObjectName("centralWidget")
        
        vlayout = QtGui.QVBoxLayout()

        hlayout = QtGui.QHBoxLayout()
        hlayout.addStretch()

        #  Sequence type choice
        seqType = QtGui.QComboBox()
        seqType.addItems( ['Beam','Expt','MPS'] )
        hlayout.addWidget( seqType )

        #  Stack of sequence index choices (one for each type)
        seqIdx = QtGui.QStackedWidget()
        beamIdx = QtGui.QComboBox()
        idxList = []
        for i in range(NBeamSeq):
            idxList.append( 'D%d'%i )
        beamIdx.addItems( idxList )
        seqIdx.addWidget(beamIdx)

        exptIdx = QtGui.QComboBox()
        idxList = []
        for i in range(NExptSeq):
            idxList.append( 'E%d'%i )
        exptIdx.addItems( idxList )
        seqIdx.addWidget(exptIdx)
        
        mpsIdx = QtGui.QComboBox()
        idxList = []
        for i in range(NMpsSeq):
            idxList.append( 'M%d'%i )
        mpsIdx.addItems( idxList )
        seqIdx.addWidget(mpsIdx)

        #  Connect the stack to the sequence type choice
        seqType.currentIndexChanged.connect(seqIdx.setCurrentIndex)
        
        hlayout.addWidget(seqIdx)
        hlayout.addStretch()
        vlayout.addLayout(hlayout)

        #vlayout.addWidget( QtGui.QLabel('Display') )
        #  The top level display of sequence control
        seqDisplay = SeqDisplay(seqType, beamIdx, exptIdx,mpsIdx)
        vlayout.addWidget(seqDisplay)

        self.centralWidget.setLayout(vlayout)
        self.centralWidget.resize(300,400)
        MainWindow.resize(300,400)
        MainWindow.setWindowTitle(sys.argv[0])


if __name__ == '__main__':
    print QtCore.PYQT_VERSION_STR

    app = QtGui.QApplication([])
    MainWindow = QtGui.QMainWindow()
    ui = Ui_MainWindow()
    ui.setupUi(MainWindow)
    MainWindow.updateGeometry()

    MainWindow.show()
    sys.exit(app.exec_())
