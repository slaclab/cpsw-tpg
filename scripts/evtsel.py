import sys
import argparse
from psp import Pv
from PyQt4 import QtCore, QtGui

NBeamSeq = 16

dstsel     = ['DontCare','Exclude','Include']
bmsel      = ['D%u'%i for i in range(NBeamSeq)]
evtsel1    = ['AC Rate','EventCode']
evtsel2    = ['Fixed Rate','AC Rate','Sequence']
fixedRates = ['929kHz','71.4kHz','10.2kHz','1.02kHz','102Hz','10.2Hz','1.02Hz']
acRates    = ['60Hz','30Hz','10Hz','5Hz','1Hz']
acTS       = ['TS%u'%(i+1) for i in range(6)]
seqIdxs    = ['E%u'%i for i in range(18)]
seqBits    = ['b%u'%i for i in range(16)]
sevrsel    = ['None','Minor','Major','Invalid']

class PvTextDisplay(QtGui.QLineEdit):

    valueSet = QtCore.pyqtSignal(QtCore.QString,name='valueSet')

    def __init__(self):
        super(PvTextDisplay, self).__init__("-")
        self.setMinimumWidth(60)
        
    def connect_signal(self):
        self.valueSet.connect(self.setValue)

    def setValue(self,value):
        self.setText(value)

        
class PvComboDisplay(QtGui.QComboBox):

    valueSet = QtCore.pyqtSignal(QtCore.QString,name='valueSet')

    def __init__(self, choices):
        super(PvComboDisplay, self).__init__()
        self.addItems(choices)
        
    def connect_signal(self):
        self.valueSet.connect(self.setValue)

    def setValue(self,value):
        self.setCurrentIndex(value)

class PvEditTxt(PvTextDisplay):

    def __init__(self, pv):
        super(PvEditTxt, self).__init__()
        self.connect_signal()
        self.editingFinished.connect(self.setPv)
        
        self.pv = Pv.Pv(pv)
        self.pv.monitor_start()
        self.pv.add_monitor_callback(self.update)

class PvEditInt(PvEditTxt):

    def __init__(self, pv):
        super(PvEditInt, self).__init__(pv)

    def setPv(self):
        value = self.text().toInt()
        self.pv.put(value)

    def update(self, err):
        q = self.pv.value
        if err is None:
            s = QtCore.QString('fail')
            try:
                s = QtCore.QString("%1").arg(QtCore.QString.number(long(q),10))
            except:
                v = ''
                for i in range(len(q)):
                    v = v + ' %f'%q[i]
                s = QtCore.QString(v)

            self.valueSet.emit(s)
        else:
            print err

class PvInt(PvEditInt):

    def __init__(self,pv):
        super(PvInt, self).__init__(pv)
        self.setEnabled(False)

class PvEditDbl(PvEditTxt):

    def __init__(self, pv):
        super(PvEditDbl, self).__init__(pv)

    def setPv(self):
        value = self.text().toDouble()
        self.pv.put(value)

    def update(self, err):
        q = self.pv.value
        if err is None:
            s = QtCore.QString('fail')
            try:
                s = QtCore.QString.number(q)
            except:
                v = ''
                for i in range(len(q)):
                    v = v + ' %f'%q[i]
                s = QtCore.QString(v)

            self.valueSet.emit(s)
        else:
            print err

class PvDbl(PvEditDbl):

    def __init__(self,pv):
        super(PvDbl, self).__init__(pv)
        self.setEnabled(False)


class PvEditCmb(PvComboDisplay):

    def __init__(self, pvname, choices, offset=0):
        super(PvEditCmb, self).__init__(choices)
        self.offset = offset
        self.connect_signal()
        self.currentIndexChanged.connect(self.setValue)
        self.pv = Pv.Pv(pvname)

    def setValue(self):
        value = self.currentIndex()+self.offset
        self.pv.put(value)

    #  Reassert PV when window is shown
    def showEvent(self,QShowEvent):
#        self.QWidget.showEvent()
        self.setValue()

class PvCmb(PvComboDisplay):

    def __init__(self, pvname, choices):
        super(PvCmb, self).__init__(choices)
        self.setEnabled(False)
        self.pv = Pv.Pv(pvname)
        self.pv.monitor_start()
        self.pv.add_monitor_callback(self.update)

    def update(self, err):
        q = self.pv.value
        if err is None:
            self.setCurrentIndex(q)
            self.valueSet.emit(q)
        else:
            print err



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


class PvMaskTab(QtGui.QWidget):
    
    def __init__(self, pvname, names):
        super(PvMaskTab,self).__init__()

        self.pv = Pv.Pv(pvname)

        self.chkBox = []
        layout = QtGui.QGridLayout()
        rows = (len(names)+3)/4
        cols = (len(names)+rows-1)/rows
        for i in range(len(names)):
            layout.addWidget( QtGui.QLabel(names[i]), i/cols, 2*(i%cols) )
            chkB = QtGui.QCheckBox()
            layout.addWidget( chkB, i/cols, 2*(i%cols)+1 )
            chkB.clicked.connect(self.update)
            self.chkBox.append(chkB)
        self.setLayout(layout)

    def update(self):
        v = 0
        for i in range(len(self.chkBox)):
            if self.chkBox[i].isChecked():
                v |= (1<<i)
        self.pv.put(v)

    #  Reassert PV when window is shown
    def showEvent(self,QShowEvent):
#        self.QWidget.showEvent()
        self.update()

class PvBtn(QtGui.QPushButton):

    def __init__(self, pvname, label):
        super(PvBtn,self).__init__(label)

        self.pv = Pv.Pv(pvname)
        self.pressed .connect(self.update)
        self.released.connect(self.update)

    def update(self):
        v = 0
        if self.isDown():
            v = 1
        self.pv.put(v)

