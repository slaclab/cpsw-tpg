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
from evtsel import *

NCounters = 24

states     = ['Done','Running']

Prefix=':RM'

class CheckBox(QtGui.QCheckBox):

    valueSet = QtCore.pyqtSignal(int, name='valueSet')

    def __init__(self,label):
        super(CheckBox, self).__init__(label)

    def connect_signal(self):
        self.valueSet.connect(self.boxClicked)

    def boxClicked(self, state):
        self.setChecked(state)

class PvEditBox(CheckBox):

    def __init__(self, pv, label):
        super(CheckBox, self).__init__(label)
        self.connect_signal()
        self.clicked.connect(self.setPv)

        self.pv = Pv.Pv(pv)
        self.pv.monitor_start()
        self.pv.add_monitor_callback(self.update)

    def setPv(self):
        q = self.isChecked()
        self.pv.put(q)

    def update(self, err):
        q = self.pv.value != 0
        if err is None:
            if q != self.isChecked():  self.valueSet.emit(q)
        else:
            print err

class PvEvtTab(QtGui.QStackedWidget):
    
    def __init__(self, pvname, evtcmb):
        super(PvEvtTab,self).__init__()

        self.addWidget(PvEditCmb(pvname+'FIXEDRATE',fixedRates))

        acw = QtGui.QWidget()
        acl = QtGui.QVBoxLayout()
        acl.addWidget(PvEditCmb(pvname+'ACRATE',acRates))
        acl.addWidget(PvEditCmb(pvname+'TSLOTMASK',acTS))
        acw.setLayout(acl)
        self.addWidget(acw)
        
        sqw = QtGui.QWidget()
        sql = QtGui.QVBoxLayout()
        sql.addWidget(PvEditCmb(pvname+'EXPSEQ',seqIdxs))
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
#        vbox.addSpacer()
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


class RMonDisplay(QtGui.QWidget):
    def __init__(self, pvname, index):
        super(RMonDisplay,self).__init__()

        vbox = QtGui.QVBoxLayout()

        prefix=pvname+Prefix+'%02i:'%index

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
        hbox.addWidget( PvEditBox(prefix+'CTRL','Start') )
        hbox.addStretch()
        vbox.addLayout(hbox)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget( QtGui.QLabel('Rate') )
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

        idx = QtGui.QComboBox()
        rmonList = []
        for i in range(NCounters):
            rmonList.append( 'RM%02d'%i )
        idx.addItems( rmonList )

        display = QtGui.QStackedWidget()
        for i in range(NCounters):
            display.addWidget( RMonDisplay(pvname,i) )

        idx.currentIndexChanged.connect(display.setCurrentIndex)

        hbox = QtGui.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget(idx)
        hbox.addStretch()
        vlayout.addLayout(hbox)
        vlayout.addWidget(display)

        self.centralWidget.setLayout(vlayout)
        self.centralWidget.resize(300,550)
        MainWindow.resize(300,550)
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
