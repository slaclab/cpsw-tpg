import subprocess
import argparse

def from_ascii(s,beg,end):
    l = s.split()[beg:end]
    o = ''
    for i in l:
        o = o+chr(int(i,16))
#    print l,o
    return o

def convert(s,beg,f,u):
    l = s.split()
    v = int(l[beg],16)*256 + int(l[beg+1],16)
    return '%f %s'%(v*f,u)

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
            
    def dump(self):
        print 'eeprom_data:',self.eeprom_data
        print 'eeprom_diag:',self.eeprom_diag
        print 'Vendor: ',from_ascii(self.eeprom_data,20,36)
        print 'SN    : ',from_ascii(self.eeprom_data,69,84)
        print 'temp (raw): ',convert(self.eeprom_diag,40,1./256.,'degC')
        print 'vcc  (raw): ',convert(self.eeprom_diag,42,0.0001,'V')
        print 'ibias(raw): ',convert(self.eeprom_diag,44,0.002,'mA')
        print 'txpwr(raw): ',convert(self.eeprom_diag,46,0.0001,'mW')
        print 'rxpwr(raw): ',convert(self.eeprom_diag,48,0.0001,'mW')

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

    def dump(self):
        print 'fault : 0x%x'%self.fault
        print 'los   : 0x%x'%self.los
        print 'modabs: 0x%x'%self.modabs
        for i in self.sfp:
            i.dump()
        
if __name__ == '__main__':

    parser = argparse.ArgumentParser(description='TFO SFP explorer')
    parser.add_argument("shm", help="shelf manager")
    parser.add_argument("slot", help="slot number",type=int)
    args = parser.parse_args()

    tfo = TFO(args.shm, args.slot)
    tfo.dump()
