import struct,sys
d=open(sys.argv[1],'rb').read()
n,off=struct.unpack_from('<II',d,0x38)
out=[]
for i in range(n):
    so,=struct.unpack_from('<I',d,off+4*i)
    p=so
    l=0;sh=0
    while True:
        b=d[p];p+=1
        l|=(b&0x7f)<<sh;sh+=7
        if not b&0x80:break
    e=d.index(b'\0',p)
    out.append(d[p:e].decode('utf-8','replace'))
for s in out: print(s)
