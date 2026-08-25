--[[============================================================================
 Comms.lua  —  PlayerbotControl ↔ PlayerbotV2 server wire protocol

 ----------------------------------------------------------------------------
 PROTOCOL OVERVIEW
 ----------------------------------------------------------------------------
 Transport ............ CHAT_MSG_ADDON / C_ChatInfo.SendAddonMessage
 Prefix ............... "PBC"   (must be registered both client & server)
 Channel .............. "WHISPER" for owner<->bot, "GUILD" for fleet broadcast.
                        We default to WHISPER → owner's main character; the
                        server-side handler intercepts addon-whispers whose
                        target is a managed bot (or the magic name "PBCFLEET")
                        and routes them through PlayerbotV2 plumbing.
 Max payload .......... 255 bytes per chunk (Blizzard hard limit on the
                        message body field). We chunk over 240 bytes and
                        reassemble using sequence numbers.
 Encoding ............. ASCII text, pipe-delimited fields. JSON was rejected
                        because (a) Blizzard's chat layer mangles backslashes
                        and quotes and (b) per-frame budget matters: 60-bot
                        roster broadcast is ~3.6KB of frames.

 ----------------------------------------------------------------------------
 WIRE FRAME (every PBC payload, before chunking)
 ----------------------------------------------------------------------------
   v|t|seq|tot|MTYPE|f1|f2|...|fN
   ^ ^  ^   ^    ^
   | |  |   |    +-- message type (uppercase, see table below)
   | |  |   +------- total chunks for this logical message (1 if not chunked)
   | |  +----------- 1-based chunk index
   | +-------------- transaction id (uint32, hex) — echoed in replies
   +---------------- protocol version, currently "1"

 Fields are escaped: literal "|" becomes "\p", literal "\" becomes "\\".
 Use Comms.escape / Comms.unescape — never concat raw.

 ----------------------------------------------------------------------------
 MESSAGE TYPES
 ----------------------------------------------------------------------------
   Owner → Server:
     ROSTER_REQ      <flags>
                     flags: bitmask. 1=include offline, 2=include intents,
                                     4=include hp/mana, 8=include distance
     BOT_DETAIL_REQ  <botGuidLow>   uint32 low-half of GUID, decimal
     CMD             <addr>|<verb>|<arg1>|...|<argN>
                     addr:  "all" | "squad" | "tank" | "healer" | "dps" |
                            "mage" | "warlock" | ... | "<botName>"
                     verb:  follow | stop | engage | engage_focus | hold |
                            squad | role | mark | login | logout | promote |
                            whisper | pause | resume
     STATS_REQ       (no args)
     ALTS_REQ        (no args) — list same-account characters so the
                     addon can show a Spawn picker. Allowed for accounts
                     with zero owned bots (first-time-spawn path).
     SUMMON          <charName>     spawn one of caller's alts as a bot;
                     server responds via EVENT_PUSH (info=summoned /
                     warn=summon_failed). Allowed for accounts with zero
                     owned bots — first-spawn bootstraps fleet ownership.
     LOGIN_REQ       <charName>     headless-login an OFFLINE character the
                     caller is authorized for (same-account alt OR a bot
                     owned by the caller's account). Server responds via
                     EVENT_PUSH (info=login_submitted / warn=login_failed).
     LOGOUT_REQ      <charName>     log out a HEADLESS bot session. Real
                     client sessions are always refused — only sessions
                     driven by the server's BotSessionMgr can be kicked.
                     EVENT_PUSH (info=logged_out / warn=logout_failed).
     SELF            on|off|status  toggle V2 AI on caller's OWN
                     character. Server replies SELF_RESP|attached=…|
                     marked=… with the post-operation state.
     ACK             <originSeq>     client acks a server EVENT_PUSH

   Server → Owner:
     ROSTER_RESP     <count>|<bot1>|<bot2>|...
                     each bot: guidLow,name,level,class,race,role,zone,
                               online,hpPct,manaPct,dist,intent,lastRule,
                               groupId,spec,headless
                     comma-separated within the bot record so we can
                     splice safely without losing the outer pipe layout.
                     headless=1 ⇔ the online session is a server-side
                     BotSession (Logout offered); 0 for offline rows and
                     for the owner's own self-AI character.
     BOT_DETAIL_RESP <guidLow>|<key=value>|<key=value>|...
                     keys: name,level,class,spec,role,zone,subzone,
                           pos_x,pos_y,pos_z,map,group_id,leader,
                           hp,hp_max,mana,mana_max,power_type,
                           intent,intent_age,last_rule,paused,
                           tickperf_ms,recent_intents (json-ish list)
     STATS_RESP      total|online|tanks|healers|dps|intents_per_sec|
                     tick_budget_ms|tick_used_ms|wedged|extra=k=v,k=v
     EVENT_PUSH      <severity>|<botGuid>|<event>|<detail>
                     severity: info|warn|error
                     event:    died|intent_failed|wedge|level_up|loot|whisper|
                               aggro|bg_end|dungeon_end|charter_signed
     PONG            <serverTimeMs>

 ----------------------------------------------------------------------------
 RETRY / RELIABILITY
 ----------------------------------------------------------------------------
   * Owner → server requests carry a sequence id. If no reply arrives within
     2.5s we retransmit up to 3 times before surfacing a "server stalled"
     warning to the BotStats panel.
   * Server → owner EVENT_PUSH frames carry a seq the owner ACKs. The server
     spool keeps unacked events for 10s and resends once.
   * STATS_RESP is broadcast every 5s when any PBC UI frame is visible
     (the addon sends an implicit STATS_REQ every 5s while shown). Hidden
     UI suppresses polling — important at 2000-bot fleet scale where every
     KB of upload matters.
============================================================================]]

local _ADDON, NS = ...
NS = NS or {}
_G.PlayerbotControl = _G.PlayerbotControl or {}
local PBC = _G.PlayerbotControl

------------------------------------------------------------------ constants
PBC.PREFIX           = "PBC"
PBC.PROTO_VERSION    = "1"
PBC.MAX_CHUNK_BYTES  = 240       -- Blizz hard cap is 255; leave envelope room.
PBC.RETRY_TIMEOUT    = 2.5
PBC.RETRY_MAX        = 3
PBC.STATS_INTERVAL   = 5.0
PBC.FLEET_TARGET     = "PBCFLEET" -- magic whisper target intercepted server-side

------------------------------------------------------------------ module state
local Comms = {}
PBC.Comms = Comms

Comms._seq          = 0
Comms._pending      = {}   -- [seq] = { msg, when, tries, onReply, onTimeout }
Comms._rxAssembly   = {}   -- [seq] = { total, parts = { [i] = str } }
Comms._handlers     = {}   -- [MTYPE] = function(fields, sender)
Comms._eventListeners = {} -- generic { fn = fn, owner = label }

------------------------------------------------------------------ logging
local function dbg(fmt, ...)
    if PBC.DB and PBC.DB.debug then
        print("|cff66ccff[PBC]|r " .. string.format(fmt, ...))
    end
end
Comms.dbg = dbg

------------------------------------------------------------------ escape helpers
function Comms.escape(s)
    if type(s) ~= "string" then s = tostring(s or "") end
    s = s:gsub("\\", "\\\\")
    s = s:gsub("|", "\\p")
    return s
end

function Comms.unescape(s)
    if type(s) ~= "string" then return s end
    -- Decode in a single pass to avoid \\p ambiguity.
    local out, i, n = {}, 1, #s
    while i <= n do
        local c = s:sub(i, i)
        if c == "\\" and i < n then
            local nx = s:sub(i + 1, i + 1)
            if nx == "p" then
                out[#out + 1] = "|"; i = i + 2
            elseif nx == "\\" then
                out[#out + 1] = "\\"; i = i + 2
            else
                out[#out + 1] = nx; i = i + 2
            end
        else
            out[#out + 1] = c; i = i + 1
        end
    end
    return table.concat(out)
end

------------------------------------------------------------------ split fields
local function splitPipes(s)
    local fields, last = {}, 1
    local n = #s
    local i = 1
    while i <= n do
        local c = s:sub(i, i)
        if c == "\\" and i < n then
            -- skip the escape sequence — they're processed during unescape
            i = i + 2
        elseif c == "|" then
            fields[#fields + 1] = Comms.unescape(s:sub(last, i - 1))
            last = i + 1
            i = i + 1
        else
            i = i + 1
        end
    end
    fields[#fields + 1] = Comms.unescape(s:sub(last))
    return fields
end
Comms.splitPipes = splitPipes

------------------------------------------------------------------ seq + transmit
local function nextSeq()
    Comms._seq = (Comms._seq + 1) % 0xFFFFFFFF
    if Comms._seq == 0 then Comms._seq = 1 end
    return Comms._seq
end

-- Build a logical frame body. Caller passes already-escaped fields.
local function buildFrame(seq, mtype, fields)
    local body = table.concat(fields, "|")
    -- We'll fill seq/total per chunk in chunkSend.
    return string.format("%s|%08x|MTYPE|%s|%s",
        PBC.PROTO_VERSION, seq, mtype, body)
end

-- Chunk a payload to <= MAX_CHUNK_BYTES.
local function chunkPayload(payload)
    local max = PBC.MAX_CHUNK_BYTES
    if #payload <= max then return { payload } end
    local out, i = {}, 1
    while i <= #payload do
        out[#out + 1] = payload:sub(i, i + max - 1)
        i = i + max
    end
    return out
end

-- Send a raw text payload (handles chunking transparently).
local function sendRaw(target, channel, version, seq, mtype, escapedFields)
    -- Pre-build full body, then split.
    local body = table.concat(escapedFields, "|")
    local chunks = chunkPayload(body)
    local total = #chunks
    for idx, chunk in ipairs(chunks) do
        local frame = string.format("%s|%08x|%d|%d|%s|%s",
            version, seq, idx, total, mtype, chunk)
        if C_ChatInfo and C_ChatInfo.SendAddonMessage then
            C_ChatInfo.SendAddonMessage(PBC.PREFIX, frame, channel, target)
        elseif SendAddonMessage then
            SendAddonMessage(PBC.PREFIX, frame, channel, target)
        end
    end
end

------------------------------------------------------------------ public API
-- target: a bot name (whisper) or PBC.FLEET_TARGET to broadcast through fleet
-- channel: usually "WHISPER"; "GUILD" or "PARTY" supported by Blizzard
function Comms.Send(target, channel, mtype, fields, opts)
    opts = opts or {}
    channel = channel or "WHISPER"
    fields = fields or {}
    local escaped = {}
    for i, v in ipairs(fields) do escaped[i] = Comms.escape(v) end
    local seq = nextSeq()
    sendRaw(target, channel, PBC.PROTO_VERSION, seq, mtype, escaped)
    if opts.expectReply or opts.onReply then
        Comms._pending[seq] = {
            target = target, channel = channel, mtype = mtype,
            escaped = escaped, when = GetTime(), tries = 0,
            onReply = opts.onReply, onTimeout = opts.onTimeout,
        }
    end
    dbg("→ %s [%s] seq=%08x t=%s ch=%s", mtype, tostring(target),
        seq, target or "?", channel)
    return seq
end

-- Convenience wrappers used across the addon.
function Comms.RosterReq(flags, target)
    return Comms.Send(target or PBC.FLEET_TARGET, "WHISPER", "ROSTER_REQ",
        { tostring(flags or 15) }, { expectReply = true })
end

function Comms.BotDetailReq(guidLow, target)
    return Comms.Send(target or PBC.FLEET_TARGET, "WHISPER", "BOT_DETAIL_REQ",
        { tostring(guidLow) }, { expectReply = true })
end

function Comms.Cmd(addr, verb, args, target)
    local fields = { addr, verb }
    if args then
        for _, a in ipairs(args) do fields[#fields + 1] = a end
    end
    return Comms.Send(target or PBC.FLEET_TARGET, "WHISPER", "CMD", fields)
end

function Comms.StatsReq(target)
    return Comms.Send(target or PBC.FLEET_TARGET, "WHISPER", "STATS_REQ",
        {}, { expectReply = true })
end

function Comms.Ack(originSeq, target)
    return Comms.Send(target or PBC.FLEET_TARGET, "WHISPER", "ACK",
        { string.format("%08x", originSeq) })
end

-- ALTS_REQ → server replies with same-account character list so we can
-- show a "Spawn one of your alts" picker. No flags yet; the server always
-- returns the full set (typical accounts have <20 chars).
function Comms.AltsReq(target)
    return Comms.Send(target or PBC.FLEET_TARGET, "WHISPER", "ALTS_REQ",
        {}, { expectReply = true })
end

-- SUMMON <charName> — addon counterpart to .playerbot summon. Server
-- replies via EVENT_PUSH (info=summoned / warn=summon_failed) rather
-- than a dedicated SUMMON_RESP so the existing toast pipeline carries
-- the message into the chat frame.
function Comms.Summon(charName, target)
    return Comms.Send(target or PBC.FLEET_TARGET, "WHISPER", "SUMMON",
        { tostring(charName or "") })
end

-- LOGIN_REQ <charName> — headless-login an offline character the caller
-- is authorized for (same-account alt OR account-owned bot). Feedback
-- arrives via EVENT_PUSH (info=login_submitted / warn=login_failed) so
-- the existing toast pipeline carries it; no dedicated RESP frame.
function Comms.LoginReq(charName, target)
    return Comms.Send(target or PBC.FLEET_TARGET, "WHISPER", "LOGIN_REQ",
        { tostring(charName or "") })
end

-- LOGOUT_REQ <charName> — kick a HEADLESS bot session. The server refuses
-- real client sessions unconditionally, so this is always safe to fire.
-- EVENT_PUSH (info=logged_out / warn=logout_failed) carries the outcome.
function Comms.LogoutReq(charName, target)
    return Comms.Send(target or PBC.FLEET_TARGET, "WHISPER", "LOGOUT_REQ",
        { tostring(charName or "") })
end

-- SELF on|off|status — toggle the V2 AI on the caller's OWN character.
-- The server always replies SELF_RESP|attached=0|1|marked=0|1 so the
-- caller can update the toggle's visible state to match server truth
-- (not just whatever we guessed locally).
function Comms.Self(mode, target)
    return Comms.Send(target or PBC.FLEET_TARGET, "WHISPER", "SELF",
        { tostring(mode or "status") }, { expectReply = true })
end

------------------------------------------------------------------ dispatch
function Comms.RegisterHandler(mtype, fn)
    Comms._handlers[mtype] = fn
end

function Comms.AddEventListener(fn, owner)
    Comms._eventListeners[#Comms._eventListeners + 1] = { fn = fn, owner = owner }
end

local function fireEventListeners(mtype, fields, sender)
    for _, l in ipairs(Comms._eventListeners) do
        local ok, err = pcall(l.fn, mtype, fields, sender)
        if not ok then
            dbg("listener %s err: %s", tostring(l.owner), tostring(err))
        end
    end
end

------------------------------------------------------------------ receive path
local function dispatchAssembled(seq, mtype, body, sender)
    local fields = splitPipes(body)
    -- If this frame matches a pending request, fulfil it.
    local pend = Comms._pending[seq]
    if pend then
        Comms._pending[seq] = nil
        if pend.onReply then
            local ok, err = pcall(pend.onReply, mtype, fields, sender)
            if not ok then dbg("onReply err: %s", tostring(err)) end
        end
    end
    -- Always invoke registered handler (a server-pushed message has no pend).
    local h = Comms._handlers[mtype]
    if h then
        local ok, err = pcall(h, fields, sender, seq)
        if not ok then dbg("handler %s err: %s", mtype, tostring(err)) end
    end
    fireEventListeners(mtype, fields, sender)
end

function Comms.OnAddonMsg(prefix, message, channel, sender)
    if prefix ~= PBC.PREFIX then return end
    -- Frame layout: v|seq|idx|tot|MTYPE|payload
    -- IMPORTANT: we cannot use splitPipes here because the payload itself
    -- contains escaped pipes. Strip the 5 envelope fields by index.
    local v, rest = message:match("^([^|]+)|(.*)$")
    if not v then return end
    if v ~= PBC.PROTO_VERSION then
        dbg("dropping frame: proto v=%s want=%s", tostring(v), PBC.PROTO_VERSION)
        return
    end
    local seqHex, rest2 = rest:match("^([^|]+)|(.*)$"); if not seqHex then return end
    local idxStr, rest3 = rest2:match("^([^|]+)|(.*)$"); if not idxStr then return end
    local totStr, rest4 = rest3:match("^([^|]+)|(.*)$"); if not totStr then return end
    local mtype,  body  = rest4:match("^([^|]+)|(.*)$"); if not mtype then return end
    local seq = tonumber(seqHex, 16)
    local idx = tonumber(idxStr)
    local tot = tonumber(totStr)
    if not (seq and idx and tot) then return end

    dbg("← %s [%s] seq=%08x %d/%d ch=%s", mtype, sender or "?", seq, idx, tot, channel)

    if tot == 1 then
        dispatchAssembled(seq, mtype, body, sender)
        return
    end
    local asm = Comms._rxAssembly[seq]
    if not asm then
        asm = { total = tot, parts = {}, mtype = mtype, sender = sender,
                started = GetTime() }
        Comms._rxAssembly[seq] = asm
    end
    asm.parts[idx] = body
    local have = 0
    for _ in pairs(asm.parts) do have = have + 1 end
    if have == asm.total then
        local glued = {}
        for i = 1, asm.total do glued[i] = asm.parts[i] or "" end
        Comms._rxAssembly[seq] = nil
        dispatchAssembled(seq, asm.mtype, table.concat(glued), asm.sender)
    end
end

------------------------------------------------------------------ retry pump
function Comms.OnUpdate(elapsed)
    local now = GetTime()
    for seq, pend in pairs(Comms._pending) do
        if now - pend.when > PBC.RETRY_TIMEOUT then
            if pend.tries >= PBC.RETRY_MAX then
                if pend.onTimeout then
                    pcall(pend.onTimeout, pend.mtype)
                end
                dbg("× timeout seq=%08x mtype=%s", seq, pend.mtype)
                Comms._pending[seq] = nil
            else
                pend.tries = pend.tries + 1
                pend.when = now
                sendRaw(pend.target, pend.channel, PBC.PROTO_VERSION, seq,
                    pend.mtype, pend.escaped)
                dbg("↻ retry %d seq=%08x mtype=%s", pend.tries, seq, pend.mtype)
            end
        end
    end
    -- Reap stale partial assemblies after 5s.
    for seq, asm in pairs(Comms._rxAssembly) do
        if now - asm.started > 5.0 then
            dbg("× assembly drop seq=%08x mtype=%s have=%d/%d",
                seq, asm.mtype, (function() local c=0 for _ in pairs(asm.parts) do c=c+1 end return c end)(),
                asm.total)
            Comms._rxAssembly[seq] = nil
        end
    end
end

------------------------------------------------------------------ encoders for
------------------------------------------------------------------ ROSTER_RESP records: comma-delimited within a pipe field.
function Comms.SplitBotRecord(s)
    -- We cannot use simple gsub split because a bot name MIGHT contain a comma
    -- via the escape layer. Bot names cannot, but defensive nonetheless.
    local out, last = {}, 1
    for i = 1, #s do
        local c = s:sub(i, i)
        if c == "," then
            out[#out + 1] = s:sub(last, i - 1); last = i + 1
        end
    end
    out[#out + 1] = s:sub(last)
    return out
end

-- ROSTER_RESP bot record schema (16 fields, positional).
-- `headless` (16) appended 2026-06-12: 1 ⇔ online session is a server-side
-- BotSession that the owner may log out. Always 0 for offline rows and for
-- the owner's own self-AI character (a real client session).
Comms.ROSTER_FIELDS = {
    "guidLow", "name", "level", "class", "race", "role", "zone",
    "online",  "hpPct", "manaPct", "dist", "intent", "lastRule",
    "groupId", "spec", "headless",
}

function Comms.DecodeRosterRecord(s)
    local parts = Comms.SplitBotRecord(s)
    local out = {}
    for i, key in ipairs(Comms.ROSTER_FIELDS) do
        out[key] = parts[i]
    end
    -- Type-coerce useful numerics.
    out.level    = tonumber(out.level)   or 0
    out.hpPct    = tonumber(out.hpPct)   or 0
    out.manaPct  = tonumber(out.manaPct) or 0
    out.dist     = tonumber(out.dist)    or -1
    out.online   = (out.online == "1") or (out.online == "true")
    out.groupId  = tonumber(out.groupId) or 0
    out.headless = (out.headless == "1")
    return out
end

-- BOT_DETAIL_RESP key=value fields → table.
function Comms.DecodeDetail(fields)
    local out = { guidLow = fields[1] }
    for i = 2, #fields do
        local k, v = fields[i]:match("^([^=]+)=(.*)$")
        if k then out[k] = v end
    end
    -- Numeric coercion for the common ones.
    for _, k in ipairs({ "level", "hp", "hp_max", "mana", "mana_max",
                         "pos_x", "pos_y", "pos_z", "map", "intent_age",
                         "tickperf_ms", "group_id", "paused" }) do
        if out[k] then out[k] = tonumber(out[k]) or out[k] end
    end
    return out
end

-- STATS_RESP positional decode.
function Comms.DecodeStats(fields)
    local out = {
        total            = tonumber(fields[1]) or 0,
        online           = tonumber(fields[2]) or 0,
        tanks            = tonumber(fields[3]) or 0,
        healers          = tonumber(fields[4]) or 0,
        dps              = tonumber(fields[5]) or 0,
        intents_per_sec  = tonumber(fields[6]) or 0,
        tick_budget_ms   = tonumber(fields[7]) or 0,
        tick_used_ms     = tonumber(fields[8]) or 0,
        wedged           = tonumber(fields[9]) or 0,
        extra            = fields[10] or "",
    }
    return out
end

-- ALTS_RESP record schema (9 fields, positional).
-- `isHeadless` (9) appended 2026-06-12: 1 ⇔ the character is online as a
-- server-driven headless bot session (Logout offered). online=1 with
-- isHeadless=0 means a REAL client session — row is locked "In World".
Comms.ALT_FIELDS = {
    "guidLow", "name", "class", "race", "level",
    "online", "isBot", "isSelf", "isHeadless",
}

function Comms.DecodeAltRecord(s)
    local parts = Comms.SplitBotRecord(s)
    local out = {}
    for i, key in ipairs(Comms.ALT_FIELDS) do
        out[key] = parts[i]
    end
    out.level      = tonumber(out.level)   or 0
    out.online     = (out.online     == "1")
    out.isBot      = (out.isBot      == "1")
    out.isSelf     = (out.isSelf     == "1")
    out.isHeadless = (out.isHeadless == "1")
    out.guidLow    = tonumber(out.guidLow) or 0
    return out
end

------------------------------------------------------------------ ping/diagnostics
function Comms.Ping()
    return Comms.Send(PBC.FLEET_TARGET, "WHISPER", "PING",
        { tostring(GetTime() * 1000) }, { expectReply = true })
end

function Comms.PendingCount()
    local c = 0; for _ in pairs(Comms._pending) do c = c + 1 end; return c
end

------------------------------------------------------------------ menu polyfill
-- Blizzard's legacy `EasyMenu` is gone in retail 12.0+. Roster/toolbar both
-- pop context menus, so we ship our own EasyMenu-shaped helper that works
-- across both menu APIs in the wild:
--   * MenuUtil.CreateContextMenu  — modern (Dragonflight+) replacement
--   * UIDropDownMenu_Initialize   — still present on most retail builds
-- Items are the legacy shape: { text, isTitle, notCheckable, func,
-- hasArrow, menuList }. Lives in Comms.lua so it loads before any UI file.
function PBC.OpenMenu(items, anchor, displayMode)
    displayMode = displayMode or "MENU"

    -- MenuUtil.CreateContextMenu requires a real frame as the owner
    -- region — strings like "cursor" crash inside AcquireMenu (it tries
    -- to read GetFrameStrata / parent off a non-frame). Detect and
    -- skip to the legacy path in that case.
    local anchorIsFrame = (type(anchor) == "table"
        and type(anchor.GetObjectType) == "function")

    -- Modern retail menu framework. Top-level only carries titles +
    -- buttons + nested submenus. Checkable states aren't translated yet
    -- because nothing in this addon uses them.
    if anchorIsFrame and MenuUtil and MenuUtil.CreateContextMenu then
        MenuUtil.CreateContextMenu(anchor or UIParent, function(_, root)
            local function addList(parent, list)
                for _, item in ipairs(list or {}) do
                    if item.isTitle then
                        parent:CreateTitle(item.text or "")
                    elseif item.hasArrow and item.menuList then
                        local sub = parent:CreateButton(item.text or "")
                        if sub and sub.CreateButton then
                            addList(sub, item.menuList)
                        end
                    elseif item.text then
                        parent:CreateButton(item.text, item.func)
                    end
                end
            end
            addList(root, items)
        end)
        return
    end

    -- Legacy fallback. Build a singleton dropdown frame on first use so
    -- repeated opens don't leak frames. Initialize takes a callback that
    -- emits buttons for the requested level (sub-menu support included).
    if UIDropDownMenu_Initialize and ToggleDropDownMenu then
        if not PBC._menuFrame then
            PBC._menuFrame = CreateFrame("Frame", "PBCSharedMenu",
                                         UIParent, "UIDropDownMenuTemplate")
        end
        UIDropDownMenu_Initialize(PBC._menuFrame,
            function(_self, level, menuList)
                local list = (level == 1) and items or menuList
                for _, item in ipairs(list or {}) do
                    local info = {}
                    for k, v in pairs(item) do info[k] = v end
                    UIDropDownMenu_AddButton(info, level)
                end
            end, displayMode)
        ToggleDropDownMenu(1, nil, PBC._menuFrame,
                           anchor or "cursor", 0, 0)
        return
    end

    -- Neither API found — degrade gracefully by dumping the labels.
    if PBC.Warn then PBC.Warn("no menu API available; items:") end
    for _, it in ipairs(items or {}) do
        if it.text and not it.isTitle and PBC.Print then
            PBC.Print(" · %s", it.text)
        end
    end
end

function Comms.AssemblyCount()
    local c = 0; for _ in pairs(Comms._rxAssembly) do c = c + 1 end; return c
end

-- vim: ts=4 sw=4 et
