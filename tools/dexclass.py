#!/usr/bin/env python3
"""Lista metodos (nombre + prototipo + flags) de clases de un .dex. uso: dexclass.py file.dex 'Lcom/x/Clase;' ..."""
import struct, sys
def uleb(d,p):
    r=0;s=0
    while True:
        b=d[p];p+=1;r|=(b&0x7f)<<s;s+=7
        if not b&0x80:return r,p
def load(path):
    d=open(path,'rb').read()
    sn,so=struct.unpack_from('<II',d,0x38); tn,to=struct.unpack_from('<II',d,0x40)
    pn,po=struct.unpack_from('<II',d,0x48); mn,mo=struct.unpack_from('<II',d,0x58); cn,co=struct.unpack_from('<II',d,0x60)
    strs=[]
    for i in range(sn):
        o,=struct.unpack_from('<I',d,so+4*i); _,p=uleb(d,o); e=d.index(b'\0',p); strs.append(d[p:e].decode('utf-8','replace'))
    types=[strs[struct.unpack_from('<I',d,to+4*i)[0]] for i in range(tn)]
    protos=[]
    for i in range(pn):
        sh,rt,po2=struct.unpack_from('<III',d,po+12*i); params=[]
        if po2:
            n,=struct.unpack_from('<I',d,po2); params=[types[struct.unpack_from('<H',d,po2+4+2*j)[0]] for j in range(n)]
        protos.append((types[rt],params))
    meths=[]
    for i in range(mn):
        c,p,nm=struct.unpack_from('<HHI',d,mo+8*i); meths.append((types[c],protos[p],strs[nm]))
    classes={}
    for i in range(cn):
        ci,af,sup,ifs,sf,ann,cd,sv=struct.unpack_from('<IIIIIIII',d,co+32*i)
        classes[types[ci]]=(af,types[sup] if sup!=0xffffffff else None,cd)
    return d,types,meths,classes
def short(t):
    m={'V':'void','Z':'boolean','B':'byte','S':'short','C':'char','I':'int','J':'long','F':'float','D':'double'}
    a=''
    while t.startswith('['): a+='[]';t=t[1:]
    return (m.get(t) or t[1:-1].split('/')[-1])+a
def methods_of(d,meths,classes,cname):
    if cname not in classes: return None
    af,sup,cd=classes[cname]
    if not cd: return []
    p=cd; sfn,p=uleb(d,p); ifn,p=uleb(d,p); dmn,p=uleb(d,p); vmn,p=uleb(d,p)
    for _ in range(sfn+ifn): _,p=uleb(d,p);_,p=uleb(d,p)
    out=[]
    for n,kind in ((dmn,'direct'),(vmn,'virtual')):
        idx=0
        for _ in range(n):
            dl,p=uleb(d,p); fl,p=uleb(d,p); _,p=uleb(d,p); idx+=dl
            c,(rt,ps),nm=meths[idx]
            out.append((fl,nm,rt,ps))
    return out
if __name__=='__main__':
    d,types,meths,classes=load(sys.argv[1])
    for cname in sys.argv[2:]:
        r=methods_of(d,meths,classes,cname)
        if r is None: continue
        print('==',cname,'(%s)'%sys.argv[1])
        for fl,nm,rt,ps in r:
            mods=('static ' if fl&8 else '')+('native ' if fl&0x100 else '')
            print('  %s%s %s(%s)'%(mods,short(rt),nm,', '.join(short(x) for x in ps)))
