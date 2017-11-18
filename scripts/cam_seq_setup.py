from evtsel import *
from sequser import *

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

    seq = SeqUser(args.pv+':EXP',args.seq)

    seq.execute(title,instrset)
