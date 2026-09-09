--[[============================================================================
 BotDebug.lua  —  per-bot detail panel.

 Tabs:
   [Snapshot] [Intents] [Logs] [Perf]

 Snapshot: relevant fields from the server's BOT_DETAIL_RESP (selected, not
           the full 800-field snapshot — see V2 snapshot struct refactor).
 Intents:  last 10 intents fired by this bot with timestamps and outcomes
           (via EVENT_PUSH stream filtered by guidLow).
 Logs:     EVENT_PUSH events for this bot (rolling 50-entry buffer).
 Perf:     tickperf_ms + recent_intents histogram.

 Header has [Pause] [Resume] [Skip Intent] toggle buttons.
============================================================================]]

local PBC = _G.PlayerbotControl
local M = {}
PBC.BotDebug = M

M.frame   = nil
M.focus   = { guidLow = nil, name = nil, detail = nil }
M.events  = {}            -- ring buffer; per-guid filtered on render
M.intents = {}            -- [guidLow] = { {ts, name, outcome}, ... }
M.MAX_EVENTS_PER_BOT = 50
M.MAX_INTENTS        = 10

local TABS = { "Snapshot", "Intents", "Logs", "Perf" }

------------------------------------------------------------------ event ingest
function M.PushEvent(guid, evt, detail, severity)
    M.events[#M.events + 1] = {
        ts = GetTime(), guid = guid, evt = evt, detail = detail, severity = severity or "info",
    }
    if #M.events > 4000 then
        -- prune oldest 1000 to keep memory bounded for marathon sessions
        local keep = {}
        for i = 1001, #M.events do keep[#keep + 1] = M.events[i] end
        M.events = keep
    end
    -- Mirror intent_fired/intent_failed into the per-bot intent ring.
    if evt == "intent_fired" or evt == "intent_failed" then
        M.intents[guid] = M.intents[guid] or {}
        local list = M.intents[guid]
        list[#list + 1] = { ts = GetTime(), name = detail, outcome = evt }
        while #list > M.MAX_INTENTS do table.remove(list, 1) end
    end
    if M.focus.guidLow == guid then
        M.RefreshActiveTab()
    end
end

------------------------------------------------------------------ helpers
local function fmtNum(v)
    if type(v) ~= "number" then return tostring(v or "-") end
    if math.abs(v) >= 1000 then return string.format("%.0f", v) end
    if math.abs(v) >= 10   then return string.format("%.1f", v) end
    return string.format("%.2f", v)
end

local function fmtAge(ts)
    local d = GetTime() - ts
    if d < 1   then return "now" end
    if d < 60  then return string.format("%ds", math.floor(d)) end
    if d < 3600 then return string.format("%dm", math.floor(d/60)) end
    return string.format("%dh", math.floor(d/3600))
end

------------------------------------------------------------------ snapshot tab
local SNAPSHOT_ROWS = {
    -- displayLabel, snapshot field key (server-side wire field name)
    { "Name",       "name"     },
    { "Level",      "level"    },
    { "Class",      "class"    },
    { "Race",       "race"     },
    { "Spec",       "_spec"    },   -- composed (spec_id → name via lookup)
    { "Role",       "role"     },
    { "Zone",       "_zone"    },   -- composed (zone_id → name)
    { "Subzone",    "_area"    },   -- composed (area_id → name)
    { "Indoors",    "indoors"  },
    { "Map",        "map"      },
    { "Position",   "_pos"     },   -- composed
    { "HP",         "_hp"      },   -- composed
    { "Mana",       "_mp"      },   -- composed
    { "XP",         "_xp"      },   -- composed (xp / xp_for_level + rest)
    { "Last Rule",  "last_rule"},
    { "Intent Q",   "intent_depth" },
}

local function renderSnapshot(detail)
    local lines = {}
    if not detail then
        lines[#lines + 1] = "(no detail yet — request pending)"
        return table.concat(lines, "\n")
    end
    local function composed(key)
        if key == "_pos" then
            return string.format("(%s, %s, %s)",
                fmtNum(detail.pos_x), fmtNum(detail.pos_y), fmtNum(detail.pos_z))
        elseif key == "_hp" then
            return string.format("%s / %s",
                fmtNum(detail.hp), fmtNum(detail.hp_max))
        elseif key == "_mp" then
            return string.format("%s / %s",
                fmtNum(detail.mana), fmtNum(detail.mana_max))
        elseif key == "_zone" then
            -- C_Map.GetMapInfo expects a UI map ID, not a zone_id; the
            -- server sends WoW's zone_id (AreaTable rows whose ParentArea
            -- is 0). The client-side C_Map.GetAreaInfo resolves to a
            -- name from the same AreaTable table.
            local zid = tonumber(detail.zone_id)
            if not zid or zid == 0 then return "-" end
            if C_Map and C_Map.GetAreaInfo then
                local name = C_Map.GetAreaInfo(zid)
                if name and name ~= "" then return name end
            end
            return string.format("zone#%d", zid)
        elseif key == "_area" then
            local aid = tonumber(detail.area_id)
            if not aid or aid == 0 then return "-" end
            if C_Map and C_Map.GetAreaInfo then
                local name = C_Map.GetAreaInfo(aid)
                if name and name ~= "" then return name end
            end
            return string.format("area#%d", aid)
        elseif key == "_spec" then
            -- ChrSpecialization DB2 id → name via GetSpecializationInfoByID.
            local sid = tonumber(detail.spec_id)
            if not sid or sid == 0 then return "-" end
            if GetSpecializationInfoByID then
                local _, name = GetSpecializationInfoByID(sid)
                if name and name ~= "" then return name end
            end
            return string.format("spec#%d", sid)
        elseif key == "_xp" then
            local xp  = tonumber(detail.xp) or 0
            local cap = tonumber(detail.xp_for_level) or 0
            local rest = tonumber(detail.rest_xp) or 0
            if cap == 0 then return "max-level" end
            local pct = (xp * 100) / cap
            if rest > 0 then
                return string.format("%d / %d (%.0f%%) +%d rest",
                                     xp, cap, pct, rest)
            end
            return string.format("%d / %d (%.0f%%)", xp, cap, pct)
        end
    end

    for _, row in ipairs(SNAPSHOT_ROWS) do
        local label, key = row[1], row[2]
        local v
        if key:sub(1,1) == "_" then v = composed(key) else v = detail[key] end
        lines[#lines + 1] = string.format("|cffaaccff%-12s|r %s",
            label, tostring(v or "-"))
    end
    return table.concat(lines, "\n")
end

------------------------------------------------------------------ intents tab
local function renderIntents()
    local guid = M.focus.guidLow
    if not guid then return "(no bot focused)" end
    local list = M.intents[guid] or {}
    if #list == 0 then return "(no intents observed yet)" end
    local lines = { "|cffaaccffts        outcome           intent|r" }
    for i = #list, 1, -1 do
        local e = list[i]
        local color = (e.outcome == "intent_failed") and "|cffff5555" or "|cff66cc66"
        lines[#lines + 1] = string.format("%6s   %s%-15s|r  %s",
            fmtAge(e.ts), color, e.outcome, e.name or "?")
    end
    return table.concat(lines, "\n")
end

------------------------------------------------------------------ logs tab
local function renderLogs()
    local guid = M.focus.guidLow
    if not guid then return "(no bot focused)" end
    local lines = { "|cffaaccffts      sev    event             detail|r" }
    local count = 0
    for i = #M.events, 1, -1 do
        local e = M.events[i]
        if e.guid == guid then
            local color = "|cffaaaaaa"
            if e.severity == "warn"  then color = "|cffffcc33" end
            if e.severity == "error" then color = "|cffff5050" end
            lines[#lines + 1] = string.format("%6s  %s%-5s|r  %-16s  %s",
                fmtAge(e.ts), color, e.severity, e.evt, e.detail or "")
            count = count + 1
            if count >= M.MAX_EVENTS_PER_BOT then break end
        end
    end
    if count == 0 then lines[#lines + 1] = "(no events for this bot)" end
    return table.concat(lines, "\n")
end

------------------------------------------------------------------ perf tab
local function renderPerf()
    local d = M.focus.detail
    if not d then return "(no detail yet)" end
    local lines = {}
    lines[#lines + 1] = string.format("tick avg     %s ms", fmtNum(d.tickperf_ms))
    lines[#lines + 1] = string.format("intent       %s", tostring(d.intent))
    lines[#lines + 1] = string.format("intent_age   %s s", fmtNum(d.intent_age))
    lines[#lines + 1] = string.format("paused       %s", tostring(d.paused == 1 or d.paused == "1"))
    if d.recent_intents then
        lines[#lines + 1] = ""
        lines[#lines + 1] = "|cffaaccffRecent (server view)|r"
        lines[#lines + 1] = tostring(d.recent_intents)
    end
    return table.concat(lines, "\n")
end

------------------------------------------------------------------ tab body refresh
function M.RefreshActiveTab()
    if not M.frame or not M.frame:IsShown() then return end
    local active = PBC.DB.debugPanel.activeTab or "Snapshot"
    local body
    if active == "Snapshot" then body = renderSnapshot(M.focus.detail)
    elseif active == "Intents" then body = renderIntents()
    elseif active == "Logs"   then body = renderLogs()
    elseif active == "Perf"   then body = renderPerf()
    end
    if M.frame.body then M.frame.body:SetText(body or "") end

    -- Header
    local f = M.frame
    if M.focus.name then
        f.header:SetText(string.format("|cffffcc33%s|r  (guid=%s)",
            M.focus.name, tostring(M.focus.guidLow or "?")))
    else
        f.header:SetText("|cff999999(no bot focused — click one in the roster)|r")
    end
end

------------------------------------------------------------------ tabs
local function activateTab(name)
    PBC.DB.debugPanel.activeTab = name
    for _, btn in ipairs(M.frame.tabBtns) do
        if btn.tabName == name then
            btn:LockHighlight()
        else
            btn:UnlockHighlight()
        end
    end
    M.RefreshActiveTab()
end

------------------------------------------------------------------ build
function M.Build()
    if M.frame then return M.frame end

    local f = CreateFrame("Frame", "PlayerbotControlDebug", UIParent, "BackdropTemplate")
    f:SetSize(PBC.DB.debugPanel.w or 460, PBC.DB.debugPanel.h or 520)
    f:SetPoint(PBC.DB.debugPanel.point or "CENTER",
               UIParent,
               PBC.DB.debugPanel.relPoint or "CENTER",
               PBC.DB.debugPanel.x or 200,
               PBC.DB.debugPanel.y or 0)
    f:SetMovable(true)
    f:SetResizable(true)
    if f.SetResizeBounds then f:SetResizeBounds(360, 320, 700, 800) end
    f:SetClampedToScreen(true)
    f:SetFrameStrata("MEDIUM")
    f:Hide()
    if f.SetBackdrop then
        f:SetBackdrop({
            bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
            edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
            tile = true, tileSize = 16, edgeSize = 14,
            insets = { left = 4, right = 4, top = 4, bottom = 4 },
        })
        f:SetBackdropColor(0.05, 0.05, 0.07, 0.96)
    end

    -- titlebar
    f.titlebar = CreateFrame("Frame", nil, f)
    f.titlebar:SetPoint("TOPLEFT", 0, 0)
    f.titlebar:SetPoint("TOPRIGHT", 0, 0)
    f.titlebar:SetHeight(22)
    local tb = f.titlebar:CreateTexture(nil, "BACKGROUND")
    tb:SetAllPoints()
    tb:SetColorTexture(0.10, 0.12, 0.18, 0.92)
    f.title = f.titlebar:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    f.title:SetPoint("LEFT", 8, 0)
    f.title:SetText("Playerbot Debug")
    f.closeBtn = CreateFrame("Button", nil, f.titlebar, "UIPanelCloseButton")
    f.closeBtn:SetPoint("TOPRIGHT", 0, 0)
    f.closeBtn:SetScript("OnClick", function() f:Hide() end)
    f.titlebar:EnableMouse(true)
    f.titlebar:RegisterForDrag("LeftButton")
    f.titlebar:SetScript("OnDragStart", function() f:StartMoving() end)
    f.titlebar:SetScript("OnDragStop", function()
        f:StopMovingOrSizing()
        local p, _, rel, x, y = f:GetPoint()
        PBC.DB.debugPanel.point    = p
        PBC.DB.debugPanel.relPoint = rel
        PBC.DB.debugPanel.x        = x
        PBC.DB.debugPanel.y        = y
    end)

    -- header (active bot indicator)
    f.header = f:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    f.header:SetPoint("TOPLEFT", 10, -28)
    f.header:SetPoint("RIGHT", -10, 0)
    f.header:SetJustifyH("LEFT")
    f.header:SetText("|cff999999(no bot focused)|r")

    -- toolbar (pause/resume/skip)
    f.toolbar = CreateFrame("Frame", nil, f)
    f.toolbar:SetPoint("TOPLEFT", 6, -44)
    f.toolbar:SetPoint("TOPRIGHT", -6, -44)
    f.toolbar:SetHeight(22)

    local function makeBtn(parent, x, text, fn)
        local b = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
        b:SetSize(70, 20)
        b:SetPoint("LEFT", x, 0)
        b:SetText(text)
        b:SetScript("OnClick", fn)
        return b
    end
    f.pauseBtn  = makeBtn(f.toolbar,   0, "Pause",  function() if M.focus.name then PBC.Comms.Cmd(M.focus.name, "pause",  {}) end end)
    f.resumeBtn = makeBtn(f.toolbar,  74, "Resume", function() if M.focus.name then PBC.Comms.Cmd(M.focus.name, "resume", {}) end end)
    f.skipBtn   = makeBtn(f.toolbar, 148, "Skip",   function() if M.focus.name then PBC.Comms.Cmd(M.focus.name, "skip_intent", {}) end end)
    f.followBtn = makeBtn(f.toolbar, 222, "Follow Me", function()
        if M.focus.name then PBC.Comms.Cmd(M.focus.name, "follow", { UnitName("player") }) end
    end)
    f.refreshBtn= makeBtn(f.toolbar, 300, "Refresh", function()
        if M.focus.guidLow then PBC.Comms.BotDetailReq(M.focus.guidLow) end
    end)

    -- tab strip
    f.tabBtns = {}
    local x = 8
    for _, name in ipairs(TABS) do
        local b = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
        b:SetSize(78, 20)
        b:SetPoint("TOPLEFT", x, -70)
        b:SetText(name)
        b.tabName = name
        b:SetScript("OnClick", function() activateTab(name) end)
        f.tabBtns[#f.tabBtns + 1] = b
        x = x + 82
    end

    -- body scroll
    f.scroll = CreateFrame("ScrollFrame", nil, f, "UIPanelScrollFrameTemplate")
    f.scroll:SetPoint("TOPLEFT", 8, -94)
    f.scroll:SetPoint("BOTTOMRIGHT", -28, 8)
    f.bodyHost = CreateFrame("Frame", nil, f.scroll)
    f.bodyHost:SetSize(420, 600)
    f.scroll:SetScrollChild(f.bodyHost)

    f.body = f.bodyHost:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    f.body:SetPoint("TOPLEFT", 4, -2)
    f.body:SetPoint("TOPRIGHT", -4, -2)
    f.body:SetJustifyH("LEFT")
    f.body:SetJustifyV("TOP")
    f.body:SetSpacing(2)
    f.body:SetWordWrap(true)
    f.body:SetFontObject("GameFontHighlightSmall")

    -- resize grip
    f.resize = CreateFrame("Button", nil, f)
    f.resize:SetSize(14, 14)
    f.resize:SetPoint("BOTTOMRIGHT", 0, 0)
    f.resize:SetNormalTexture("Interface\\ChatFrame\\UI-ChatIM-SizeGrabber-Up")
    f.resize:SetPushedTexture("Interface\\ChatFrame\\UI-ChatIM-SizeGrabber-Down")
    f.resize:SetHighlightTexture("Interface\\ChatFrame\\UI-ChatIM-SizeGrabber-Highlight")
    f.resize:SetScript("OnMouseDown", function() f:StartSizing("BOTTOMRIGHT") end)
    f.resize:SetScript("OnMouseUp", function()
        f:StopMovingOrSizing()
        PBC.DB.debugPanel.w = f:GetWidth()
        PBC.DB.debugPanel.h = f:GetHeight()
    end)

    M.frame = f
    M.RegisterHandlers()
    activateTab(PBC.DB.debugPanel.activeTab or "Snapshot")
    return f
end

------------------------------------------------------------------ protocol handlers
function M.RegisterHandlers()
    PBC.Comms.RegisterHandler("BOT_DETAIL_RESP", function(fields, sender)
        local d = PBC.Comms.DecodeDetail(fields)
        if M.focus.guidLow and tostring(M.focus.guidLow) == tostring(d.guidLow) then
            M.focus.detail = d
            if d.name then M.focus.name = d.name end
            M.RefreshActiveTab()
        end
    end)
end

------------------------------------------------------------------ focus management
function M.FocusByGuid(guidLow, name)
    M.focus.guidLow = tostring(guidLow)
    M.focus.name    = name
    M.focus.detail  = nil
    PBC.CharDB.focusBotGuid = M.focus.guidLow
    PBC.Comms.BotDetailReq(M.focus.guidLow)
    M.RefreshActiveTab()
end

function M.FocusByName(name)
    if not PBC.BotRoster or not PBC.BotRoster.GetRoster then return end
    for _, r in ipairs(PBC.BotRoster.GetRoster()) do
        if (r.name or ""):lower() == name:lower() then
            M.FocusByGuid(r.guidLow, r.name)
            return
        end
    end
    PBC.Warn("bot '%s' not found in roster", name)
end

------------------------------------------------------------------ tick
local refreshAccum = 0
function M.OnTick(elapsed)
    if not M.frame or not M.frame:IsShown() then return end
    refreshAccum = refreshAccum + elapsed
    if refreshAccum >= 2.0 then
        refreshAccum = 0
        if M.focus.guidLow then PBC.Comms.BotDetailReq(M.focus.guidLow) end
    end
end

------------------------------------------------------------------ show/hide
function M.Show()
    if not M.frame then M.Build() end
    M.frame:Show()
    PBC.DB.debugPanel.shown = true
    M.RefreshActiveTab()
    -- Restore focus from CharDB if we have one and roster is loaded.
    if PBC.CharDB.focusBotGuid and not M.focus.guidLow then
        M.focus.guidLow = PBC.CharDB.focusBotGuid
        PBC.Comms.BotDetailReq(M.focus.guidLow)
    end
end

function M.Hide()
    if M.frame then M.frame:Hide() end
    PBC.DB.debugPanel.shown = false
end

function M.Toggle()
    if M.frame and M.frame:IsShown() then M.Hide() else M.Show() end
end

-- vim: ts=4 sw=4 et
