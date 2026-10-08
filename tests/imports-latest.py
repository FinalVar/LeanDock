"""Inspect PE imports/exports without loading or executing the artifacts."""
import json, struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def inspect(path):
    b=path.read_bytes(); pe=struct.unpack_from('<I',b,0x3c)[0]
    assert b[pe:pe+4]==b'PE\0\0'
    machine,sections=struct.unpack_from('<HH',b,pe+4)
    assert machine==0x8664
    opt_size=struct.unpack_from('<H',b,pe+20)[0]; opt=pe+24
    assert struct.unpack_from('<H',b,opt)[0]==0x20b
    table=opt+opt_size
    def offset(rva):
        for i in range(sections):
            pos=table+i*40
            vs,va,rs,raw=struct.unpack_from('<IIII',b,pos+8)
            if va<=rva<va+max(vs,rs):return raw+rva-va
        raise ValueError(rva)
    def string(rva):
        pos=offset(rva);return b[pos:b.index(0,pos)].decode('ascii')
    imp_rva=struct.unpack_from('<I',b,opt+112+8)[0]
    pos=offset(imp_rva); imports=[]
    while any(b[pos:pos+20]):
        imports.append(string(struct.unpack_from('<I',b,pos+12)[0]));pos+=20
    ex_rva=struct.unpack_from('<I',b,opt+112)[0]; exports=[]
    if ex_rva:
        ex=offset(ex_rva); num=struct.unpack_from('<I',b,ex+24)[0]
        names=offset(struct.unpack_from('<I',b,ex+32)[0])
        exports=[string(struct.unpack_from('<I',b,names+i*4)[0]) for i in range(num)]
    prohibited=['windhawk','libc++','libunwind','libgcc','vcruntime','msvcp','python','node']
    assert not any(word in dll.lower() for word in prohibited for dll in imports), imports
    if path==ROOT/'start'/'latest.dll':
        assert 'LeanDockStartHook' in exports
    elif path.suffix=='.dll':
        assert 'NativeDock_Start' in exports and 'DllGetClassObject' in exports
    return {'file':str(path.relative_to(ROOT)),'bytes':len(b),'imports':imports,'exports':exports}
result=[inspect(ROOT/'latest.exe'),inspect(ROOT/'latest.dll'),inspect(ROOT/'start'/'latest.dll')]
(ROOT/'tests'/'imports-latest.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
