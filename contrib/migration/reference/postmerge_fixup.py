import re, sys, os
WT = sys.argv[1] if len(sys.argv) > 1 else "I:/TrinityCore/_rebuild-integ"

def read(rel):
    p = f"{WT}/{rel}"
    d = open(p, encoding="utf-8", newline="").read()
    return p, d, ("\r\n" if "\r\n" in d else "\n")

# 1) enum-LINE deduper (DB statement enums): drop duplicate enumerator lines by name
def dedupe_enum_lines(rel):
    p, data, nl = read(rel)
    lines = data.split(nl); seen = set(); out = []; removed = 0
    ent = re.compile(r'^\s*([A-Z][A-Z0-9_]+)\s*(=.*)?,?\s*$')
    for ln in lines:
        m = ent.match(ln)
        if m:
            n = m.group(1)
            if n in seen: removed += 1; continue
            seen.add(n)
        out.append(ln)
    if removed: open(p, "w", encoding="utf-8", newline="").write(nl.join(out))
    print(f"  {rel.split('/')[-1]}: -{removed} dup enum lines")

# 1b) reclose top-level struct/enum blocks whose closing }; the union merge dropped
def reclose_blocks(rel):
    p, data, nl = read(rel)
    lines = data.split(nl); out = []; depth = 0; removed = 0
    hdr = re.compile(r'^\s*(struct|class|enum(?:\s+class)?)\b')
    for ln in lines:
        # a new top-level definition starts while we're still inside a previous one -> close it
        if hdr.match(ln) and depth > 0:
            while depth > 0:
                out.append("};"); depth -= 1; removed += 1
            if out and out[-1] == "};":
                out.append("")  # blank line separator
        out.append(ln)
        depth += ln.count('{') - ln.count('}')
    # close any trailing open block before EOF sentinels handled by file's own tail
    if removed:
        open(p, "w", encoding="utf-8", newline="").write(nl.join(out))
    print(f"  {rel.split('/')[-1]}: reclosed {removed} dropped '}};'")

# 2) struct/enum BLOCK deduper (robust name = last identifier before {/:/final)
def block_name(line):
    head = re.split(r'[{:]', line, 1)[0]
    head = re.sub(r'\b(struct|class|enum|final)\b', ' ', head)
    toks = re.findall(r'[A-Za-z_]\w*', head)
    return toks[-1] if toks else None

def dedupe_blocks(rel):
    p, data, nl = read(rel)
    lines = data.split(nl); out = []; seen = set(); i = 0; removed = []
    hdr = re.compile(r'^\s*(struct|class|enum(?:\s+class)?)\b')
    while i < len(lines):
        if hdr.match(lines[i]):
            name = block_name(lines[i])
            j = i; depth = 0; started = False; end = None
            while j < len(lines):
                depth += lines[j].count('{') - lines[j].count('}')
                if '{' in lines[j]: started = True
                if started and depth <= 0: end = j; break
                j += 1
            if end is not None and started and name:
                if name in seen:
                    removed.append(name); i = end + 1
                    if i < len(lines) and lines[i].strip() == "": i += 1
                    continue
                seen.add(name); out.extend(lines[i:end+1]); i = end + 1; continue
        out.append(lines[i]); i += 1
    if removed: open(p, "w", encoding="utf-8", newline="").write(nl.join(out))
    print(f"  {rel.split('/')[-1]}: -{len(removed)} dup blocks {sorted(set(removed))[:10]}")

# 3) repair the GROUP_FINDER_ACTIVITY locale splice in HotfixDatabase.cpp (union dropped continuation)
def fix_group_finder_splice(rel):
    p, data, nl = read(rel)
    broken = ('    PREPARE_LOCALE_STMT(HOTFIX_SEL_GROUP_FINDER_ACTIVITY, "SELECT ID, FullName_lang, ShortName_lang FROM group_finder_activity_locale"' + nl +
              '    PrepareStatement(HOTFIX_SEL_UI_TEXTURE_KIT,')
    if 'group_finder_activity_locale"' + nl + '    PrepareStatement(HOTFIX_SEL_UI_TEXTURE_KIT' in data:
        data = data.replace(
            '    PREPARE_LOCALE_STMT(HOTFIX_SEL_GROUP_FINDER_ACTIVITY, "SELECT ID, FullName_lang, ShortName_lang FROM group_finder_activity_locale"' + nl + '    PrepareStatement(HOTFIX_SEL_UI_TEXTURE_KIT',
            '    PREPARE_LOCALE_STMT(HOTFIX_SEL_GROUP_FINDER_ACTIVITY, "SELECT ID, FullName_lang, ShortName_lang FROM group_finder_activity_locale"' + nl +
            '        " WHERE (`VerifiedBuild` > 0) = ? AND locale = ?", CONNECTION_SYNCH);' + nl + '    PrepareStatement(HOTFIX_SEL_UI_TEXTURE_KIT')
        open(p, "w", encoding="utf-8", newline="").write(data); print("  HotfixDatabase.cpp: GROUP_FINDER splice repaired")
    else:
        print("  HotfixDatabase.cpp: no GROUP_FINDER splice (ok)")

print("=== post-merge fixup ===")
for f in ["src/server/database/Database/Implementation/HotfixDatabase.h",
          "src/server/database/Database/Implementation/LoginDatabase.h",
          "src/server/database/Database/Implementation/CharacterDatabase.h"]:
    dedupe_enum_lines(f)
for f in ["src/server/game/DataStores/DB2Structure.h",
          "src/server/game/DataStores/DB2LoadInfo.h",
          "src/server/game/DataStores/DB2Metadata.h"]:
    reclose_blocks(f)   # first repair union-dropped '};' so the file is structurally valid
    dedupe_blocks(f)    # then remove true duplicate blocks
fix_group_finder_splice("src/server/database/Database/Implementation/HotfixDatabase.cpp")
