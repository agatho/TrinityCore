import re, sys, os, glob
WT = "I:/TrinityCore/_migrate121"

HDR = re.compile(r'^(\s*)(?:class|struct)\s+\w+')
NS  = re.compile(r'^(\s*)namespace\s+\w+')
def indent(l): return len(l) - len(l.lstrip())

def reclose(lines, nl):
    """Insert dropped '};' before a class/struct/namespace header that follows an unclosed
    sibling: header indent strictly shallower than the preceding member decl line."""
    out = []; inserted = 0
    def prev_code():
        for x in reversed(out):
            s = x.strip()
            if not s or s.startswith('//') or s.startswith('/*') or s.startswith('*'): continue
            return x
        return ""
    for ln in lines:
        m = HDR.match(ln) or NS.match(ln)
        if m:
            p = prev_code(); ps = p.strip()
            # prior line is a member/field decl (ends ';', not a close/brace/label) and is MORE indented
            if ps.endswith(';') and not ps.endswith('};') and ps not in ('public:','private:','protected:') \
               and indent(p) > indent(ln):
                out.append(" " * indent(ln) + "};"); out.append(""); inserted += 1
        out.append(ln)
    return out, inserted

def dedupe_blocks(lines):
    """Remove duplicate top-level class/struct/enum definitions by name (keep first)."""
    out=[]; seen=set(); i=0; removed=[]
    hdr=re.compile(r'^\s*(class|struct|enum(?:\s+class)?)\b')
    def name(line):
        head=re.split(r'[{:]',line,1)[0]
        head=re.sub(r'\b(class|struct|enum|final)\b',' ',head)
        toks=re.findall(r'[A-Za-z_]\w*',head)
        return toks[-1] if toks else None
    while i<len(lines):
        if hdr.match(lines[i]) and not lines[i].strip().endswith(';'):
            nm=name(lines[i]); j=i; depth=0; started=False; end=None
            while j<len(lines):
                depth+=lines[j].count('{')-lines[j].count('}')
                if '{' in lines[j]: started=True
                if started and depth<=0: end=j; break
                j+=1
            if end is not None and started and nm:
                if nm in seen:
                    removed.append(nm); i=end+1
                    if i<len(lines) and lines[i].strip()=="": i+=1
                    continue
                seen.add(nm); out.extend(lines[i:end+1]); i=end+1; continue
        out.append(lines[i]); i+=1
    return out, removed

def dedupe_inline(lines):
    """Remove duplicate inline method/member decls (any line, keep first) that look like
    a declaration: contains '(' or 'm_'/'_x', ends with ';' or '}', len>18, not a label/brace."""
    seen=set(); out=[]; removed=0
    for ln in lines:
        code=ln.strip().split('//')[0].strip()
        key=None
        if len(code)>18 and (code.endswith(';') or code.endswith('}')) and ('(' in code) \
           and code[:1].isalpha() and not code.startswith(('namespace','using','typedef','friend','return','if','for','while')):
            key=re.sub(r'\s+',' ',code)
        if key is not None:
            if key in seen: removed+=1; continue
            seen.add(key)
        out.append(ln)
    return out, removed

files = []
for pat in ["src/server/game/Server/Packets/*.h",
            "src/server/game/Battlegrounds/*.h",
            "src/server/game/Entities/Player/CollectionMgr.h"]:
    files += glob.glob(f"{WT}/{pat}")
for f in sorted(set(files)):
    data=open(f,encoding="utf-8",newline="").read(); nl="\r\n" if "\r\n" in data else "\n"
    lines=data.split(nl)
    lines,ins=reclose(lines,nl)
    lines,db=dedupe_blocks(lines)
    if ins or db:
        open(f,"w",encoding="utf-8",newline="").write(nl.join(lines))
        print(f"{os.path.basename(f)}: reclose+{ins}, dupblocks-{len(db)} {sorted(set(db))[:6]}")
