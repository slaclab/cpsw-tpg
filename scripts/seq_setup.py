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
#
# Setup a sequence that will assert:
#   bit 0 at 929kHz/divisor
#   bit 1 at 929kHz/divisor/2
#   bit 2 at 929kHz/divisor/4
#   bit 3 at 929kHz/divisor/8
#

    parser = argparse.ArgumentParser(description='simple exp seq setup')
    parser.add_argument('--pv' , help="TPG pv base", default='TPG:SYS2:1')
    parser.add_argument('--seq', help="sequence number", type=int, default=0)
    parser.add_argument('--div', help="divisor", type=int, default=2)
    args = parser.parse_args()

    sync_marker = 6

    if args.div<2:
        print 'Parameter div must be > 1'
        sys.exit(1)

    repeat = (intervals[sync_marker]-1)/(128*args.div)
    if repeat < 1:
        print 'Parameter div is too high.  Cannot complete the sequence between sync markers.'
        sys.exit(2)

    if repeat > 256:
        repeat = 256
        print 'The sequence is limited to 256 repetitions between sync markers. (%u/%u)'%(repeat*128*args.div,intervals[sync_marker])

    instrset = []
    #  Insert global sync instruction (1Hz?)
    instrset.append(FixedRateSync(marker=sync_marker,occ=1))
        
    instrset.append(ControlRequest(0x1))  # line 1
    instrset.append(FixedRateSync(marker=0,occ=args.div))
    instrset.append(ControlRequest(0x3))
    instrset.append(FixedRateSync(marker=0,occ=args.div))
    instrset.append(ControlRequest(0x1))
    instrset.append(FixedRateSync(marker=0,occ=args.div))
    instrset.append(ControlRequest(0x7))
    instrset.append(FixedRateSync(marker=0,occ=args.div))
    instrset.append(ControlRequest(0x1))
    instrset.append(FixedRateSync(marker=0,occ=args.div))
    instrset.append(ControlRequest(0x3))
    instrset.append(FixedRateSync(marker=0,occ=args.div))
    instrset.append(ControlRequest(0x1))
    instrset.append(FixedRateSync(marker=0,occ=args.div))
    instrset.append(ControlRequest(0xf))
    instrset.append(FixedRateSync(marker=0,occ=args.div))
    instrset.append(ControlRequest(0x1))
    instrset.append(FixedRateSync(marker=0,occ=args.div))
    instrset.append(ControlRequest(0x3))
    instrset.append(FixedRateSync(marker=0,occ=args.div))
    instrset.append(ControlRequest(0x1))
    instrset.append(FixedRateSync(marker=0,occ=args.div))
    instrset.append(ControlRequest(0x7))
    instrset.append(FixedRateSync(marker=0,occ=args.div))
    instrset.append(ControlRequest(0x1))
    instrset.append(FixedRateSync(marker=0,occ=args.div))
    instrset.append(ControlRequest(0x3))
    instrset.append(FixedRateSync(marker=0,occ=args.div))
    instrset.append(ControlRequest(0x1))
    instrset.append(FixedRateSync(marker=0,occ=args.div))

    #  These next branches will jump ahead.  
    #  Put in place-holder instructions (branch) until we know the target lines
    b0 = len(instrset)
    instrset.append(Branch.conditional(line=1, counter=0, value=1))
    instrset.append(Branch.conditional(line=1, counter=1, value=1))
    instrset.append(Branch.conditional(line=1, counter=2, value=1))

    instrset.append(ControlRequest(0xff))
    instrset.append(FixedRateSync(marker=0,occ=args.div))
    instrset.append(Branch.conditional(line=1, counter=3, value=repeat-1))
    instrset.append(Branch.unconditional(line=0))

    #  This is the target of the third branch
    b3 = len(instrset)
    instrset.append(ControlRequest(0x7f))
    instrset.append(FixedRateSync(marker=0,occ=args.div))
    instrset.append(Branch.unconditional(line=1))

    #  This is the target of the second branch
    b2 = len(instrset)
    instrset.append(ControlRequest(0x3f))
    instrset.append(FixedRateSync(marker=0,occ=args.div))
    instrset.append(Branch.unconditional(line=1))

    #  This is the target of the first branch
    b1 = len(instrset)
    instrset.append(ControlRequest(0x1f))
    instrset.append(FixedRateSync(marker=0,occ=args.div))
    instrset.append(Branch.unconditional(line=1))

    #  Replace the place holder instructions
    instrset[b0+0] = Branch.conditional(line=b1, counter=0, value=1)
    instrset[b0+1] = Branch.conditional(line=b2, counter=1, value=1)
    instrset[b0+2] = Branch.conditional(line=b3, counter=2, value=1)


    i=0
    for instr in instrset:
        print 'Put instruction(%d): '%i, 
        print instr.print_()
        i += 1


    title = 'LoopTest'

    seq = MySequencer('EXP',1)

    seq.execute(title,instrset)
