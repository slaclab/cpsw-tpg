#from evtsel import *
##############################################################################
## This file is part of 'tpg'.
## It is subject to the license terms in the LICENSE.txt file found in the 
## top-level directory of this distribution and at: 
##    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
## No part of 'tpg', including this file, 
## may be copied, modified, propagated, or distributed except according to 
## the terms contained in the LICENSE.txt file.
##############################################################################
import sys
import argparse
from sequser import *
#from seq_ca_new import *

Prefix='TPG:SYS2:2'

if __name__ == '__main__':

    parser = argparse.ArgumentParser(description='simple exp seq setup')
    parser.add_argument('--pv' , help="TPG pv base", default='TPG:SYS2:2')
    parser.add_argument('--seq', help="sequence number", type=int, default=15)
    args = parser.parse_args()

    sync_marker = 6
    
    instrset = []
    #  Insert global sync instruction (1Hz?)
    instrset.append(FixedRateSync(marker=sync_marker,occ=1))

    for i in range(4):
        sh = i*4

        b0 = len(instrset)
        instrset.append(ControlRequest(0xf<<sh))
        instrset.append(FixedRateSync(marker=0,occ=i+1))
        instrset.append(Branch.conditional(line=b0, counter=0, value=1))

        b0 = len(instrset)
        instrset.append(ControlRequest(0xe<<sh))
        instrset.append(FixedRateSync(marker=0,occ=i+1))
        instrset.append(Branch.conditional(line=b0, counter=0, value=1))

        b0 = len(instrset)
        instrset.append(ControlRequest(0xc<<sh))
        instrset.append(FixedRateSync(marker=0,occ=i+1))
        instrset.append(Branch.conditional(line=b0, counter=0, value=3))

        b0 = len(instrset)
        instrset.append(ControlRequest(0x8<<sh))
        instrset.append(FixedRateSync(marker=0,occ=i+1))
        instrset.append(Branch.conditional(line=b0, counter=0, value=7))

    instrset.append(Branch.unconditional(line=0))

    i=0
    for instr in instrset:
        print 'Put instruction(%d): '%i, 
        print instr.print_()
        i += 1


    title = 'BurstTest'

    seq = SeqUser(args.pv+':EXP',args.seq)

    seq.execute(title,instrset)
