from psp import Pv
import time

fixedRates = ['929kHz','71.4kHz','10.2kHz','1.02kHz','102Hz','10.2Hz','1.02Hz']
acRates    = ['60Hz','30Hz','10Hz','5Hz','1Hz','0.5Hz']

class Instruction(object):

    def __init__(self, args):
        self.args = args

    def encoding(self):
        args = [0]*7
        args[0] = len(self.args)-1
        args[1:len(self.args)+1] = self.args
        return args

class FixedRateSync(Instruction):

    opcode = 0

    def __init__(self, marker, occ):
        super(FixedRateSync, self).__init__( (self.opcode, marker, occ) )

    def print_(self):
        return 'FixedRateSync(%s) # occ(%d)'%(fixedRates[self.args[1]],self.args[2])
    

class ACRateSync(Instruction):

    opcode = 1

    def __init__(self, timeslotm, marker, occ):
        super(ACRateSync, self).__init__( (self.opcode, timeslotm, marker, occ) )

    def print_(self):
        return 'ACRateSync(%s/0x%x) # occ(%d)'%(acRates[self.args[2]],self.args[1],self.args[3])
    

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
        return 'ControlRequest word 0x%x'%self.args[1]

class SeqUser:
    def __init__(self, base, index):
        prefix = base+'%02d'%index
        self.index   = index
        self.ninstr  = Pv.Pv(prefix+':INSTRCNT')
        self.desc    = Pv.Pv(prefix+':DESCINSTRS')
        self.instr   = Pv.Pv(prefix+':INSTRS')
        self.idxseq  = Pv.Pv(prefix+':SEQ00IDX')
        self.seqname = Pv.Pv(prefix+':SEQ00DESC')
        self.idxseqr = Pv.Pv(prefix+':RMVIDX')
        self.seqr    = Pv.Pv(prefix+':RMVSEQ')
        self.insert  = Pv.Pv(prefix+':INS')
        self.idxrun  = Pv.Pv(prefix+':RUNIDX')
        self.start   = Pv.Pv(prefix+':SCHEDRESET')
        self.reset   = Pv.Pv(prefix+':FORCERESET')

    def execute(self, title, instrset):
        self.insert.put(0)

        # Remove existing sub sequences
        ridx = -1
        print 'Remove %d'%ridx
        if ridx < 0:
            idx = self.idxseq.get()
            while (idx>0):
                print 'Removing seq %d'%idx
                self.idxseqr.put(idx)
                self.seqr.put(1)
                self.seqr.put(0)
                idx = self.idxseq.get()
        elif ridx > 1:
            print 'Removing seq %d'%ridx
            self.idxseqr.put(ridx)
            self.seqr.put(1)
            self.seqr.put(0)

        self.desc.put(title)

        encoding = [len(instrset)]
        for instr in instrset:
            encoding = encoding + instr.encoding()

        print encoding

        self.instr.put( tuple(encoding) )

        time.sleep(0.1)

        ninstr = self.ninstr.get()
        print 'Confirmed ninstr %d'%ninstr

        self.insert.put(1)

        #  Get the assigned sequence num
        idx = self.idxseq.get()
        print 'Sequence '+self.seqname.get()+' found at index %d'%idx

        #  self.post(idx,title,ninstr)

        #  Start it
        self.idxrun.put(idx)
        self.start .put(0)
        self.reset .put(1)
        self.reset .put(0)
