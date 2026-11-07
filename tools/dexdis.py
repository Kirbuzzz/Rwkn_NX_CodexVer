#!/usr/bin/env python3
"""Listado legible de un metodo de dex: invoke/const/sget/iget con nombres resueltos. uso: dexdis.py f.dex 'Clase' metodo"""
import struct, sys
from dexclass import load, uleb, short
from dexcalls import FMT
def main(path, cls, mname):
    d,types,meths,classes=load(path)
    sn,so=struct.unpack_from('<II',d,0x38); fn,fo=struct.unpack_from('<II',d,0x50)
    strs=[]
    for i in range(sn):
        o,=struct.unpack_from('<I',d,so+4*i); _,p=uleb(d,o); e=d.index(b'\0',p); strs.append(d[p:e].decode('utf-8','replace'))
    fields=[]
    for i in range(fn):
        c,t,nm=struct.unpack_from('<HHI',d,fo+8*i); fields.append((types[c].split('/')[-1].rstrip(';'),strs[nm]))
    cn,co=struct.unpack_from('<II',d,0x60)
    for i in range(cn):
        ci,af,sup,ifs,sf,ann,cd,sv=struct.unpack_from('<IIIIIIII',d,co+32*i)
        if types[ci]!=cls or not cd: continue
        p=cd; sfn,p=uleb(d,p); ifn,p=uleb(d,p); dmn,p=uleb(d,p); vmn,p=uleb(d,p)
        for _ in range(sfn+ifn): _,p=uleb(d,p);_,p=uleb(d,p)
        for n in (dmn,vmn):
            idx=0
            for _ in range(n):
                dl,p=uleb(d,p); fl,p=uleb(d,p); code,p=uleb(d,p); idx+=dl
                if meths[idx][2]!=mname or not code: continue
                regs,ins,outs,tries,dbg,isz=struct.unpack_from('<HHHHII',d,code)
                print('--- %s.%s  regs=%d ins=%d'%(cls,mname,regs,ins))
                base=code+16; u=lambda k: struct.unpack_from('<H',d,base+2*k)[0]
                pc=0
                while pc<isz:
                    w=u(pc); op=w&0xff
                    if op==0 and (w>>8) in (1,2,3):
                        if w>>8==1: sz=u(pc+1); pc+=4+2*sz
                        elif w>>8==2: sz=u(pc+1); pc+=2+4*sz
                        else: wd=u(pc+1); sz=u(pc+2)|(u(pc+3)<<16); pc+=4+(sz*wd+1)//2
                        continue
                    ln=FMT.get(op,1); s=None
                    if op==0x12: v=(w>>12)&0xf; s='const/4 v%d, %d'%((w>>8)&0xf, v if v<8 else v-16)
                    elif op==0x13: s='const/16 v%d, %d'%(w>>8,struct.unpack('<h',struct.pack('<H',u(pc+1)))[0])
                    elif op==0x14: s='const v%d, %d'%(w>>8,struct.unpack('<i',struct.pack('<HH',u(pc+1),u(pc+2)))[0])
                    elif op==0x15: s='const/high16 v%d, %#x'%(w>>8,u(pc+1)<<16)
                    elif op==0x1a: s='const-string v%d, "%s"'%(w>>8,strs[u(pc+1)])
                    elif op==0x1b: s='const-string v%d, "%s"'%(w>>8,strs[u(pc+1)|(u(pc+2)<<16)])
                    elif op==0x1c: s='const-class v%d, %s'%(w>>8,types[u(pc+1)])
                    elif op==0x22: s='new-instance v%d, %s'%(w>>8,types[u(pc+1)])
                    elif 0x60<=op<=0x6d: c,n=fields[u(pc+1)]; s='%s v%d, %s.%s'%(('sget','sput')[op>=0x67],w>>8,c,n)
                    elif 0x52<=op<=0x5f: c,n=fields[u(pc+1)]; s='%s v%d, v%d, %s.%s'%(('iget','iput')[op>=0x59],(w>>8)&0xf,(w>>12)&0xf,c,n)
                    elif op in (0x6e,0x6f,0x70,0x71,0x72,0x74,0x75,0x76,0x77,0x78):
                        mi=u(pc+1); c,(rt,ps),nm=meths[mi]
                        if op<0x73: cnt=w>>12; r=u(pc+2); rl=[r&0xf,(r>>4)&0xf,(r>>8)&0xf,(r>>12)&0xf,(w>>8)&0xf][:cnt]
                        else: cnt=w>>8; f=u(pc+2); rl=list(range(f,f+cnt))
                        k={0x6e:'virt',0x6f:'super',0x70:'direct',0x71:'static',0x72:'iface'}.get(op,'range')
                        s='invoke-%s {%s} %s.%s(%s)->%s'%(k,','.join('v%d'%x for x in rl),c.split('/')[-1].rstrip(';'),nm,','.join(short(x) for x in ps),short(rt))
                    elif op in (0x0a,0x0b,0x0c): s='move-result v%d'%(w>>8)
                    elif op in (0x0e,0x0f,0x10,0x11): s='return'
                    elif op in (0x07,0x01): s='move v%d,v%d'%((w>>8)&0xf,(w>>12)&0xf)
                    if s: print('%04x  %s'%(pc,s))
                    pc+=ln
if __name__=='__main__': main(sys.argv[1],sys.argv[2],sys.argv[3])
