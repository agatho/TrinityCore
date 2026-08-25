import re, os
WT="I:/TrinityCore/_migrate121"
SC="C:/Users/daimon/AppData/Local/Temp/claude/c--dumps/d61f229a-840a-4add-ab28-452509f4a22b/scratchpad/housing_int"

def rd(p):
    return open(f"{WT}/{p}",encoding="utf-8",newline="").read()
def wr(p,s):
    open(f"{WT}/{p}","w",encoding="utf-8",newline="").write(s)
def nlof(s): return "\r\n" if "\r\n" in s else "\n"
def staged(name): return open(f"{SC}/{name}",encoding="utf-8").read()

def extract_decls(hdr_text, names):
    """Return dict name->full decl (may be multi-line) from a header (class body decls only, no Class::)."""
    L=hdr_text.split('\n'); out={}
    for i,l in enumerate(L):
        for n in names:
            if re.search(rf"\b{n}\b\s*[\(;]", l) and "::" not in l.split(n)[0][-2:] and n not in out:
                # gather until ';'
                blk=[l.rstrip()]; j=i
                while ";" not in L[j]:
                    j+=1; blk.append(L[j].rstrip())
                out[n]='\n'.join(blk)
    return out

def extract_members(hdr_text, names):
    L=hdr_text.split('\n'); out={}
    for l in L:
        for n in names:
            if re.search(rf"\b{n}\s*[;=]", l) and n not in out and "(" not in l:
                out[n]=l.rstrip()
    return out

def extract_body(cpp_text, cls, method):
    """Extract 'RetType Cls::method(...) {...}' up to a line that is '}' at column 0."""
    L=cpp_text.split('\n')
    for i,l in enumerate(L):
        if re.search(rf"\b{cls}::{method}\b\s*\(", l) and not l.lstrip().startswith(("//","*")):
            # back up to start of return-type line (this line begins the signature)
            blk=[]; j=i
            # signature may span lines until '{'
            while "{" not in L[j]:
                blk.append(L[j]); j+=1
            blk.append(L[j])  # line with {
            depth=L[j].count("{")-L[j].count("}")
            j+=1
            while j<len(L) and depth>0:
                depth+=L[j].count("{")-L[j].count("}")
                blk.append(L[j]); j+=1
            return '\n'.join(blk)
    return None

def graft_header_decls(path, staged_hdr, names, anchor, members=False):
    cur=rd(path); nl=nlof(cur)
    have=set(re.findall(r"\b([A-Za-z_][A-Za-z0-9_]*)\b\s*[\(;]", cur))
    d = extract_members(staged_hdr, names) if members else extract_decls(staged_hdr, names)
    add=[d[n] for n in names if n in d and (members or n not in have)]
    if members:
        add=[d[n] for n in names if n in d and d[n].strip() not in set(x.strip() for x in cur.split(nl))]
    if not add: return 0
    L=cur.split(nl); ai=next((i for i,l in enumerate(L) if anchor in l), None)
    if ai is None: print("  MISS anchor",anchor,"in",path); return 0
    L[ai+1:ai+1]=[x.replace('\n',nl) for x in add]
    wr(path,nl.join(L)); return len(add)

def graft_cpp_bodies(path, staged_cpp, cls, methods):
    cur=rd(path); nl=nlof(cur); added=0; blocks=[]
    for m in methods:
        if re.search(rf"\b{cls}::{m}\b\s*\(", cur): continue  # already defined
        b=extract_body(staged_cpp, cls, m)
        if b: blocks.append(b.replace('\n',nl)); added+=1
        else: print("  MISS body",cls,m)
    if blocks:
        wr(path, cur.rstrip(nl)+nl+nl+ (nl+nl).join(blocks)+nl)
    return added

if __name__=="__main__":
    import json,sys
    # 1) Object members
    n=graft_header_decls("src/server/game/Entities/Object/Object.h", staged("Object.h"),
        ["m_housingDecorData","m_housingFixtureData","m_housingRoomComponentMeshData","m_housingRoomData"],
        "protected:", members=True)
    print("Object members:",n)
    # 2) GameObject decls + bodies
    gom=["InitHousingCornerstoneData","InitHousingDecorData","InitHousingDecorMirroredPosition"]
    print("GameObject decls:",graft_header_decls("src/server/game/Entities/GameObject/GameObject.h",staged("GameObject.h"),gom,"class TC_GAME_API GameObject"))
    print("GameObject bodies:",graft_cpp_bodies("src/server/game/Entities/GameObject/GameObject.cpp",staged("GameObject.cpp"),"GameObject",gom))
    # 3) Player decls + bodies
    plm=["GetAllHousings","GetHousing","GetHousingForNeighborhood","SetCurrentHouse","SetHousingEditorModeUpdateField","UpdateHousingMapId","UpdateInitiativeFavor"]
    print("Player decls:",graft_header_decls("src/server/game/Entities/Player/Player.h",staged("Player.h"),plm,"void SendDirectMessage"))
    print("Player bodies:",graft_cpp_bodies("src/server/game/Entities/Player/Player.cpp",staged("Player.cpp"),"Player",plm))
    # 4) AreaTrigger decls + bodies
    atm=["CreateStaticAreaTrigger","InitHousingPlotVisuals"]
    print("AreaTrigger decls:",graft_header_decls("src/server/game/Entities/AreaTrigger/AreaTrigger.h",staged("AreaTrigger.h"),atm,"class TC_GAME_API AreaTrigger"))
    print("AreaTrigger bodies:",graft_cpp_bodies("src/server/game/Entities/AreaTrigger/AreaTrigger.cpp",staged("AreaTrigger.cpp"),"AreaTrigger",atm))
    print("DONE")
