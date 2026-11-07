#!/usr/bin/env python3
"""Desensamblador minimo de dex: muestra donde se llama a metodos de una clase y con que argumentos (constantes/strings)."""
import struct, sys
from dexclass import load, uleb, short

FMT = {}
def setf(rng, size):
    for o in rng: FMT[o] = size
setf([0x00,0x01,0x04,0x07,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x1d,0x1e,0x21,0x27,0x28], 1)
setf([0x02,0x05,0x08,0x13,0x15,0x16,0x19,0x1a,0x1c,0x1f,0x20,0x22,0x23,0x29], 2)
setf([0x03,0x06,0x09,0x14,0x17,0x1b,0x24,0x25,0x26,0x2a,0x2b,0x2c], 3)
setf([0x18], 5)
setf(range(0x2d,0x32), 2); setf(range(0x32,0x3e), 2); setf(range(0x3e,0x44), 1)
setf(range(0x44,0x52), 2); setf(range(0x52,0x60), 2); setf(range(0x60,0x6e), 2)
setf(range(0x6e,0x73), 3); setf([0x73], 1); setf(range(0x74,0x79), 3); setf([0x79,0x7a], 1)
setf(range(0x7b,0x90), 1); setf(range(0x90,0xb0), 2); setf(range(0xb0,0xd0), 1)
setf(range(0xd0,0xd8), 2); setf(range(0xd8,0xe3), 2); setf(range(0xe3,0xfa), 1)
setf([0xfa,0xfb], 4); setf([0xfc,0xfd], 3); setf([0xfe,0xff], 2)

def run(path, target_classes, names=None):
    d,types,meths,classes=load(path)
    sn,so=struct.unpack_from('<II',d,0x38)
    strs=[]
    for i in range(sn):
        o,=struct.unpack_from('<I',d,so+4*i); _,p=uleb(d,o); e=d.index(b'\0',p); strs.append(d[p:e].decode('utf-8','replace'))
    cn,co=struct.unpack_from('<II',d,0x60)
    mn,mo=struct.unpack_from('<II',d,0x58)
    hits=0
    for i in range(cn):
        ci,af,sup,ifs,sf,ann,cd,sv=struct.unpack_from('<IIIIIIII',d,co+32*i)
        if not cd: continue
        owner=types[ci]
        p=cd; sfn,p=uleb(d,p); ifn,p=uleb(d,p); dmn,p=uleb(d,p); vmn,p=uleb(d,p)
        for _ in range(sfn+ifn): _,p=uleb(d,p);_,p=uleb(d,p)
        idx=0
        for n in (dmn,vmn):
            idx=0
            for _ in range(n):
                dl,p=uleb(d,p); fl,p=uleb(d,p); code,p=uleb(d,p); idx+=dl
                if not code: continue
                regs,ins,outs,tries,dbg,isz=struct.unpack_from('<HHHHII',d,code)
                base=code+16; u=lambda k: struct.unpack_from('<H',d,base+2*k)[0]
                val={}; pc=0; lastres=None; trace=[]
                while pc<isz:
                    w=u(pc); op=w&0xff
                    if op==0 and (w>>8) in (1,2,3):
                        if w>>8==1: sz=u(pc+1); pc+=4+2*sz
                        elif w>>8==2: sz=u(pc+1); pc+=2+4*sz
                        else:
                            wd=u(pc+1); sz=u(pc+2)|(u(pc+3)<<16); pc+=4+(sz*wd+1)//2
                        continue
                    ln=FMT.get(op,1)
                    if op==0x12: val[(w>>8)&0xf]='%d'%(((w>>12)&0xf) if ((w>>12)&0xf)<8 else ((w>>12)&0xf)-16)
                    elif op==0x13: val[w>>8]=str(struct.unpack('<h',struct.pack('<H',u(pc+1)))[0])
                    elif op==0x14: val[w>>8]=str(struct.unpack('<i',struct.pack('<HH',u(pc+1),u(pc+2)))[0])
                    elif op==0x15: val[w>>8]=hex(u(pc+1)<<16)
                    elif op==0x1a: val[w>>8]='"%s"'%strs[u(pc+1)]
                    elif op==0x1b: val[w>>8]='"%s"'%strs[u(pc+1)|(u(pc+2)<<16)]
                    elif op==0x1c: val[w>>8]='class '+types[u(pc+1)]
                    elif op in (0x0a,0x0c) and lastres: val[w>>8]=lastres
                    elif op in (0x6e,0x6f,0x70,0x71,0x72,0x74,0x75,0x76,0x77,0x78):
                        mi=u(pc+1); c,(rt,ps),nm=meths[mi]
                        if op<0x73:
                            cnt=w>>12; r=u(pc+2); regsl=[r&0xf,(r>>4)&0xf,(r>>8)&0xf,(r>>12)&0xf,(w>>8)&0xf][:cnt]
                        else:
                            cnt=w>>8; first=u(pc+2); regsl=list(range(first,first+cnt))
                        lastres='%s.%s()'%(c.split('/')[-1].rstrip(';'),nm)
                        if c in target_classes and (not names or nm in names):
                            args=[val.get(x,'v%d'%x) for x in regsl]
                            kind='static' if op in (0x71,0x77) else 'virt'
                            trace.append((pc,nm,args,short(rt),ps))
                    pc+=ln
                if trace:
                    mc,mp,mnm=meths[idx]
                    for pc2,nm,args,rt,ps in trace:
                        hits+=1
                        print('%s.%s @%04x: %s(%s)'%(owner.split('/')[-1].rstrip(';'),mnm,pc2,nm,', '.join(args)))
    return hits
if __name__=='__main__':
    tc={'Lcom/ubisoft/uaf/UAFJNILib;'}
    names=set(sys.argv[2:]) if len(sys.argv)>2 else None
    run(sys.argv[1],tc,names)
