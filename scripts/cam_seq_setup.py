from evtsel import *
from seq_ca_new import *

class MySequencer:
    def __init__(self, base, index):
        prefix = Prefix+':'+base+'%02d'%index
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

        encoding = [i]
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

if __name__ == '__main__':

    parser = argparse.ArgumentParser(description='simple exp seq setup')
    parser.add_argument('--pv' , help="TPG pv base", default='TPG:SYS2:1')
    parser.add_argument('--seq', help="sequence number", type=int, default=0)
    args = parser.parse_args()

    sync_marker = 6
    
    instrset = []
    #  Insert global sync instruction (1Hz?)
    instrset.append(FixedRateSync(marker=sync_marker,occ=1))
        
    instrset.append(ControlRequest(0x7))  # line 1
    instrset.append(FixedRateSync(marker=4,occ=1)) # 100Hz
    instrset.append(ControlRequest(0x1))
    instrset.append(Branch.conditional(line=2, counter=0, value=8))
    instrset.append(FixedRateSync(marker=4,occ=1))
    instrset.append(ControlRequest(0x3))
    instrset.append(Branch.conditional(line=2, counter=1, value=8))
    b0 = len(instrset)
    instrset.append(FixedRateSync(marker=4,occ=1)) # 100Hz
    instrset.append(ControlRequest(0x1))
    instrset.append(Branch.conditional(line=b0, counter=0, value=8))
    instrset.append(Branch.unconditional(line=0))

    i=0
    for instr in instrset:
        print 'Put instruction(%d): '%i, 
        print instr.print_()
        i += 1


    title = 'LoopTest'

    seq = MySequencer('EXP',args.seq)

    seq.execute(title,instrset)
