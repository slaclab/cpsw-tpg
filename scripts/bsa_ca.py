import sys
import argparse
from psp import Pv
from PyQt4 import QtCore, QtGui
from evtsel import *

NArrays = 8

states     = ['Done','Running']

BsaPrefix=':BSA'

class PvPushB(QtGui.QPushButton):

    valueSet = QtCore.pyqtSignal(QtCore.QString,name='valueSet')

    def __init__(self,label):
        super(PvPushB, self).__init__(label)
        self.pressed.connect(self.setPv)
        self.setMinimumWidth(40)
        
    def connect_signal(self):
        self.valueSet.connect(self.setValue)

    def setValue(self,value):
        if value>0:
            self.setDown(True)
        else:
            self.setDown(False)

class PvEditPushB(PvPushB):

    def __init__(self, pv, label):
        super(PvEditPushB, self).__init__(label)

        self.pv = Pv.Pv(pv)
        self.pv.monitor_start()
        self.pv.add_monitor_callback(self.update)

    def setPv(self):
        if self.isDown:
            value = 1
        else:
            value = 0
        self.pv.put(value)

    def update(self, err):
        q = self.pv.value
        self.valueSet.emit(QtCore.QString(q))

class PvEvtTab(QtGui.QStackedWidget):
    
    def __init__(self, pvname, evtcmb):
        super(PvEvtTab,self).__init__()

        self.addWidget(PvEditCmb(pvname+'FIXEDRATE',fixedRates))

        acw = QtGui.QWidget()
        acl = QtGui.QVBoxLayout()
        acl.addWidget(PvEditCmb(pvname+'ACRATE',acRates))
#        acl.addWidget(PvEditCmb(pvname+'ATS'  ,acTS))
        acw.setLayout(acl)
        self.addWidget(acw)
        
        sqw = QtGui.QWidget()
        sql = QtGui.QVBoxLayout()
#        sql.addWidget(PvEditCmb(pvname+'SEQIDX',seqIdxs))
#        sql.addWidget(PvEditCmb(pvname+'SEQBIT',seqBits))
        sql.addWidget(PvEditCmb(pvname+'EXPSEQBIT',seqBits))
        sqw.setLayout(sql)
        self.addWidget(sqw)

        evtcmb.currentIndexChanged.connect(self.setCurrentIndex)
                 
class PvEditEvt(QtGui.QWidget):
    
    def __init__(self, pvname, mode):
        super(PvEditEvt, self).__init__()
        vbox = QtGui.QVBoxLayout()
        evtcmb = PvEditCmb(pvname+mode,evtsel2)
        vbox.addWidget(evtcmb)
        vbox.addWidget(PvEvtTab(pvname,evtcmb))
        self.setLayout(vbox)

class PvDstTab(QtGui.QWidget):
    
    def __init__(self, pvname):
        super(PvDstTab,self).__init__()

        self.pv = Pv.Pv(pvname)

        self.chkBox = []
        layout = QtGui.QGridLayout()
        for i in range(NBeamSeq):
            layout.addWidget( QtGui.QLabel('D%d'%i), i/4, 2*(i%4) )
            chkB = QtGui.QCheckBox()
            layout.addWidget( chkB, i/4, 2*(i%4)+1 )
            chkB.clicked.connect(self.update)
            self.chkBox.append(chkB)
        self.setLayout(layout)

    def update(self):
        v = 0
        for i in range(NBeamSeq):
            if self.chkBox[i].isChecked():
                v |= (1<<i)
        self.pv.put(v)

class PvEditDst(QtGui.QWidget):
    
    def __init__(self, pvname, mode, mask):
        super(PvEditDst, self).__init__()
        vbox = QtGui.QVBoxLayout()
        selcmb = PvEditCmb(pvname+mode,dstsel)
        
        vbox.addWidget(selcmb)
        vbox.addWidget(PvDstTab(pvname+mask))
        self.setLayout(vbox)


class BsaDisplay(QtGui.QWidget):
    def __init__(self, pvname, index):
        super(BsaDisplay,self).__init__()

        vbox = QtGui.QVBoxLayout()

        prefix=pvname+BsaPrefix+'%02i:'%index

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( PvEditDst(prefix,'DESTMODE','DESTMASK') )
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( PvEditEvt(prefix,'RATEMODE') )
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Max Severity') )
        hbox.addWidget( PvEditCmb(prefix+'MEASSEVR', sevrsel) )
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Num to Average') )
        hbox.addWidget( PvEditInt(prefix+'AVGCNT') )
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Num to Acquire') )
        hbox.addWidget( PvEditInt(prefix+'MEASCNT') )
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( PvEditPushB(prefix+'CTRL','Start') )
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Num Averaged') )
        hbox.addWidget( PvInt(prefix+'AVGCNTACT') )
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Num Acquired') )
        hbox.addWidget( PvInt(prefix+'CNT') )
        hbox.addStretch()
        vbox.addLayout(hbox)

        self.setLayout(vbox)

class Ui_MainWindow(object):
    def setupUi(self, MainWindow, pvname):
        MainWindow.setObjectName(QtCore.QString.fromUtf8("MainWindow"))
        self.centralWidget = QtGui.QWidget(MainWindow)
        self.centralWidget.setObjectName("centralWidget")

        vlayout = QtGui.QVBoxLayout()

        edefIdx = QtGui.QComboBox()
        edefList = []
        for i in range(NArrays):
            edefList.append( 'BSA%02d'%i )
        edefIdx.addItems( edefList )

        bsaDisplay = QtGui.QStackedWidget()
        for i in range(NArrays):
            bsaDisplay.addWidget( BsaDisplay(pvname,i) )

        edefIdx.currentIndexChanged.connect(bsaDisplay.setCurrentIndex)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget(edefIdx)
        hbox.addStretch()
        vlayout.addLayout(hbox)
        vlayout.addWidget(bsaDisplay)

        self.centralWidget.setLayout(vlayout)
        self.centralWidget.resize(300,500)
        MainWindow.resize(300,500)
        MainWindow.setWindowTitle(sys.argv[0])
            
if __name__ == '__main__':
    print QtCore.PYQT_VERSION_STR

    parser = argparse.ArgumentParser(description='simple bsa gui')
    parser.add_argument('--pv', help="TPG pv base", default='TPG:SYS2:1')
    args = parser.parse_args()

    app = QtGui.QApplication([])
    MainWindow = QtGui.QMainWindow()
    ui = Ui_MainWindow()
    ui.setupUi(MainWindow,args.pv)
    MainWindow.updateGeometry()

    MainWindow.show()
    sys.exit(app.exec_())
