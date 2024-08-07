import subprocess
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

def from_ascii(s,beg,end):
    l = s.split()[beg:end]
    o = ''
    for i in l:
        o = o+chr(int(i,16))
#    print l,o
    return o

def convert(s,beg,f):
    l = s.split()
    v = int(l[beg],16)*256 + int(l[beg+1],16)
    return v*f

def bmask(s,beg,end):
    l = s.split()[beg:end]
    print 'bmask',s,beg,end,l
    v = 0
    for i in reversed(l):
        v = 256*v+int(i,16)
    return v

class SFF8472:

    def __init__(self, shm, slot, chan):
        
        self.shm  = shm
        self.adr  = 0x80+2*slot
        self.chan = chan
        self.eeprom_data = self.read(0,0,96)
        self.eeprom_diag = self.read(1,56,62)

    def read(self, prom, adx, len):
        
        base = 'ipmitool -I lan -H '+self.shm+' -t %d'%self.adr+' -b 0 -A NONE raw 0x34 0xfc %d'%self.chan+' %d'%prom
        result = ''

        adx_end = adx+len
        while adx<adx_end:
            cmd = base + ' 0x%x 0x18'%adx
#            print 'cmd: ',cmd
#            result.append(subprocess.run(cmd,stdout=subprocess.PIPE).stdout())
            result += subprocess.check_output(cmd,shell=True)
            adx = adx+24

        return result

    def vendor(self):
        return from_ascii(self.eeprom_data,20,36)

    def temp(self):
        return convert(self.eeprom_diag,40,1./256.)

    def txpwr(self):
        return convert(self.eeprom_diag,46,0.0001)

    def rxpwr(self):
        return convert(self.eeprom_diag,48,0.0001)

    def dump(self):
        print 'eeprom_data:',self.eeprom_data
        print 'eeprom_diag:',self.eeprom_diag
        print 'Vendor: ',self.vendor()
        print 'SN    : ',from_ascii(self.eeprom_data,69,84)
        print 'temp (raw): {:} degC',self.temp()
        print 'vcc  (raw): {:} V' ,convert(self.eeprom_diag,42,0.0001)
        print 'ibias(raw): {:} mW',convert(self.eeprom_diag,44,0.002)
        print 'txpwr(raw): {:} mW',self.txpwr()
        print 'rxpwr(raw): {:} mW',self.rxpwr()

class TFO:
    def __init__(self, shm, slot):
        self.shm  = shm
        self.adr  = 0x80+2*slot
        
        cmd = 'ipmitool -I lan -H '+self.shm+' -t 0x%x'%self.adr+' -b 0 -A NONE raw 0x34 0x12 0'
        result = subprocess.check_output(cmd,shell=True)
        self.fault  = bmask(result,0,3)
        self.los    = bmask(result,3,6)
        self.modabs = bmask(result,6,9)
        self.sfp    = []
        for i in range(20):
            if 0 == (self.modabs & (1<<i)):
                self.sfp.append(SFF8472(shm,slot,i))

    def dump(self,verbose):
        print 'fault : 0x%x'%self.fault
        print 'los   : 0x%x'%self.los
        print 'modabs: 0x%x'%self.modabs

        if verbose:
            for i in self.sfp:
                i.dump()

        print(' SFP |      Vendor      |  Temp | TxPwr | RxPwr ')
        print('------------------------------------------')
        fmt = ' {:3d} | {:16s} | {:2.2f} | {:2.2f} | {:2.2f}'
        for i in self.sfp:
            print(fmt.format(i.chan,i.vendor(),i.temp(),i.txpwr(),i.rxpwr()))
        print('------------------------------------------')

if __name__ == '__main__':

    parser = argparse.ArgumentParser(description='TFO SFP explorer')
    parser.add_argument("shm", help="shelf manager")
    parser.add_argument("slot", help="slot number",type=int)
    parser.add_argument("--verbose", help="verbose",action='store_true')
    args = parser.parse_args()

    tfo = TFO(args.shm, args.slot)
    tfo.dump(args.verbose)
