--[[============================================================================
 BotRoster.lua  —  vertical list of owned bots.

 Each row shows:
   ┌────────────────────────────────────────────┐
   │ ⚔ Areon  L80 Warrior  TANK         12y     │
   │ HP  ████████████░░░░░░░░░░  62%             │
   │ MP  █████░░░░░░░░░░░░░░░░░  21% (rage)      │
   │ intent=engage_boss   rule=tank_pull         │
   └────────────────────────────────────────────┘

 Right-click → context menu:
   online row .... Follow Me / Stop / Engage / Promote / Whisper / Detail /
                   Pause / Resume / Logout (headless sessions only — the
                   owner's self-AI char is a real client and never shows it)
   offline row ... Log In / Detail (CMD verbs can't reach an offline bot)
 Offline rows swap the HP/MP bars for an inline [Log In] button.

 Sort: by role (default) | by name | by class | by level | by distance
 Filter: showOffline toggle in the footer (offline rows are fetched either
         way — ROSTER_REQ flags bit 1 — so toggling is instant/local).
 Scrollable via FauxScrollFrame for >20 visible.

 Data flow:
   * On Show or every 5s while shown: Comms.RosterReq(15)
   * On ROSTER_RESP: parse, replace internal roster table, re-layout rows.
   * Class color from Blizzard's RAID_CLASS_COLORS.
============================================================================]]

local PBC = _G.PlayerbotControl
local M = {}
PBC.BotRoster = M

M.frame    = nil
M.rows     = {}        -- recycled row pool
M.MAX_ROWS = 14
M.ROW_H    = 54
M.roster   = {}        -- array of bot records, sorted
M.byGuid   = {}        -- guidLow → record
M.scroll   = 0
M.menu     = nil
M._pendingRefresh = nil -- GetTime() deadline for a one-shot re-fetch

------------------------------------------------------------------ helpers
local CLASS_COLORS = RAID_CLASS_COLORS or {}
local ROLE_ORDER   = { TANK = 1, HEALER = 2, DPS = 3, UNKNOWN = 4 }

local function classColor(class)
    local c = CLASS_COLORS[class and class:upper() or ""]
    if c then return c.r, c.g, c.b end
    return 0.85, 0.85, 0.85
end

local function roleIcon(role)
    role = (role or ""):upper()
    if role == "TANK"   then return "|TInterface\\LFGFrame\\UI-LFG-ICON-PORTRAITROLES:14:14:0:0:64:64:0:19:22:41|t" end
    if role == "HEALER" then return "|TInterface\\LFGFrame\\UI-LFG-ICON-PORTRAITROLES:14:14:0:0:64:64:20:39:1:20|t" end
    if role == "DPS" or role == "DAMAGE" or role == "DAMAGER" then
        return "|TInterface\\LFGFrame\\UI-LFG-ICON-PORTRAITROLES:14:14:0:0:64:64:20:39:22:41|t"
    end
    return "·"
end

local function fmtDistance(d)
    if not d or d < 0 then return "--" end
    if d < 99 then return string.format("%dy", math.floor(d)) end
    return ">99y"
end

------------------------------------------------------------------ context menu
local function showContextMenu(row, record)
    if not M.menu then
        M.menu = CreateFrame("Frame", "PBCBotRosterMenu", UIParent, "UIDropDownMenuTemplate")
    end
    local menu = M.menu
    local function fire(addr, verb, args)
        PBC.Comms.Cmd(addr, verb, args)
        PBC.Print("→ %s %s", addr, verb)
    end

    -- OFFLINE bots get a minimal menu: CMD verbs route via the bot's live
    -- session, so the only meaningful actions are headless login + detail.
    if not record.online then
        local items = {
            { text = record.name .. " (offline)", isTitle = true, notCheckable = true },
            { text = "Log In",  notCheckable = true, func = function()
                PBC.Comms.LoginReq(record.name)
                PBC.Print("→ login %s", record.name)
                M.RequestRefresh(1.5)
            end },
            { text = "Detail",  notCheckable = true, func = function()
                PBC.BotDebug.Show()
                PBC.BotDebug.FocusByGuid(record.guidLow, record.name)
            end },
        }
        PBC.OpenMenu(items, row, "MENU")
        return
    end

    local items = {
        { text = record.name, isTitle = true, notCheckable = true },
        { text = "Follow Me",    notCheckable = true, func = function() fire(record.name, "follow", { UnitName("player") }) end },
        { text = "Stop",         notCheckable = true, func = function() fire(record.name, "stop", {}) end },
        { text = "Engage Target",notCheckable = true, func = function()
            local t = UnitName("target")
            if t then fire(record.name, "engage_focus", { t })
            else PBC.Warn("no target") end
        end },
        { text = "Promote",      notCheckable = true, hasArrow = true, menuList = {
            { text = "Tank",   notCheckable = true, func = function() fire(record.name, "role", { "tank" }) end },
            { text = "Healer", notCheckable = true, func = function() fire(record.name, "role", { "healer" }) end },
            { text = "DPS",    notCheckable = true, func = function() fire(record.name, "role", { "dps" }) end },
        } },
        { text = "Whisper…",     notCheckable = true, func = function()
            ChatFrame_OpenChat("/w " .. record.name .. " ")
        end },
        { text = "Detail",       notCheckable = true, func = function()
            PBC.BotDebug.Show()
            PBC.BotDebug.FocusByGuid(record.guidLow, record.name)
        end },
        { text = "Pause",        notCheckable = true, func = function() fire(record.name, "pause",  {}) end },
        { text = "Resume",       notCheckable = true, func = function() fire(record.name, "resume", {}) end },
    }
    -- Logout only for HEADLESS sessions (LOGOUT_REQ; the old CMD logout
    -- verb never existed server-side). The owner's own self-AI character
    -- shows in the roster too — that's a real client session, no Logout.
    if record.headless then
        items[#items + 1] = { text = "Logout", notCheckable = true, func = function()
            PBC.Comms.LogoutReq(record.name)
            PBC.Print("→ logout %s", record.name)
            M.RequestRefresh(1.5)
        end }
    end
    -- Anchor on the row itself; MenuUtil rejects the "cursor" string and
    -- needs a real frame. Visually equivalent — the row is right under
    -- the click already so the menu still pops where the user expects.
    PBC.OpenMenu(items, row, "MENU")
end

------------------------------------------------------------------ row factory
local function buildRow(parent, index)
    local f = CreateFrame("Button", "PBCRosterRow" .. index, parent)
    f:SetHeight(M.ROW_H)
    f:RegisterForClicks("LeftButtonUp", "RightButtonUp")

    f.bg = f:CreateTexture(nil, "BACKGROUND")
    f.bg:SetAllPoints()
    f.bg:SetColorTexture(0.06, 0.07, 0.09, 0.55)

    f.hl = f:CreateTexture(nil, "HIGHLIGHT")
    f.hl:SetAllPoints()
    f.hl:SetColorTexture(0.2, 0.4, 0.9, 0.25)

    -- Layout (54px row, top-to-bottom):
    --   y=-4..-18    name + meta (16px line)
    --   y=-21..-31   HP bar (10px)  with HP% text centered on the bar
    --   y=-33..-40   MP bar (7px) — text omitted, color codes the type
    --   y=-43..-52   intent + last-rule subtext (10px)
    -- 4px top + 4px bottom pads keep the row from kissing its neighbours.
    f.name = f:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    f.name:SetPoint("TOPLEFT", 6, -4)
    f.name:SetJustifyH("LEFT")

    f.meta = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    f.meta:SetPoint("TOPRIGHT", -6, -4)
    f.meta:SetJustifyH("RIGHT")

    -- HP bar
    f.hp = CreateFrame("StatusBar", nil, f)
    f.hp:SetPoint("TOPLEFT", 6, -21)
    f.hp:SetPoint("RIGHT", -6, 0)
    f.hp:SetHeight(10)
    f.hp:SetStatusBarTexture("Interface\\TargetingFrame\\UI-StatusBar")
    f.hp:SetStatusBarColor(0.30, 0.85, 0.35)
    f.hp:SetMinMaxValues(0, 100)
    f.hp.bg = f.hp:CreateTexture(nil, "BACKGROUND")
    f.hp.bg:SetAllPoints()
    f.hp.bg:SetColorTexture(0, 0, 0, 0.6)
    f.hp.text = f.hp:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    f.hp.text:SetPoint("CENTER", 0, 0)

    -- MP bar
    f.mp = CreateFrame("StatusBar", nil, f)
    f.mp:SetPoint("TOPLEFT", 6, -33)
    f.mp:SetPoint("RIGHT", -6, 0)
    f.mp:SetHeight(7)
    f.mp:SetStatusBarTexture("Interface\\TargetingFrame\\UI-StatusBar")
    f.mp:SetStatusBarColor(0.25, 0.45, 0.95)
    f.mp:SetMinMaxValues(0, 100)
    f.mp.bg = f.mp:CreateTexture(nil, "BACKGROUND")
    f.mp.bg:SetAllPoints()
    f.mp.bg:SetColorTexture(0, 0, 0, 0.6)

    -- subtext: intent + rule
    f.sub = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    f.sub:SetPoint("BOTTOMLEFT", 6, 4)
    f.sub:SetPoint("BOTTOMRIGHT", -6, 4)
    f.sub:SetJustifyH("LEFT")
    f.sub:SetHeight(11)

    -- Inline action button — only visible on OFFLINE rows ("Log In"),
    -- where the HP/MP bars carry no information and the context-menu CMD
    -- verbs can't reach the bot anyway. Sits where the bars would be.
    f.action = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    f.action:SetSize(72, 20)
    f.action:SetPoint("RIGHT", -6, -3)   -- below the meta line (top 18px)
    f.action:SetText("Log In")
    f.action:Hide()
    f.action:SetScript("OnClick", function(self)
        local rec = self:GetParent().record
        if not rec or rec.online then return end
        PBC.Comms.LoginReq(rec.name)
        PBC.Print("→ login %s", rec.name)
        M.RequestRefresh(1.5)
    end)

    f:SetScript("OnClick", function(self, button)
        if not self.record then return end
        if button == "RightButton" then
            showContextMenu(self, self.record)
        else
            PBC.BotDebug.Show()
            PBC.BotDebug.FocusByGuid(self.record.guidLow, self.record.name)
        end
    end)
    f:SetScript("OnEnter", function(self)
        if not self.record then return end
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        local r = self.record
        GameTooltip:AddLine(r.name)
        GameTooltip:AddDoubleLine("Level", tostring(r.level))
        GameTooltip:AddDoubleLine("Class", tostring(r.class))
        GameTooltip:AddDoubleLine("Role",  tostring(r.role))
        GameTooltip:AddDoubleLine("Zone",  tostring(r.zone))
        GameTooltip:AddDoubleLine("Distance", fmtDistance(r.dist))
        GameTooltip:AddDoubleLine("Intent", tostring(r.intent))
        GameTooltip:AddDoubleLine("Last rule", tostring(r.lastRule))
        GameTooltip:Show()
    end)
    f:SetScript("OnLeave", function() GameTooltip:Hide() end)

    return f
end

------------------------------------------------------------------ row population
local function populateRow(rowFrame, record)
    rowFrame.record = record
    local r, g, b = classColor(record.class)
    rowFrame.name:SetText(string.format("%s |cff%02x%02x%02x%s|r  L%d",
        roleIcon(record.role), r * 255, g * 255, b * 255,
        record.name or "?", record.level or 0))
    local p = PBC.DB.palette
    if not record.online then
        local gp = p.offlineGray
        rowFrame.bg:SetColorTexture(gp[1] * 0.2, gp[2] * 0.2, gp[3] * 0.2, 0.7)
    elseif record.hpPct <= 0 then
        local er = p.enemyRed
        rowFrame.bg:SetColorTexture(er[1] * 0.25, er[2] * 0.2, er[3] * 0.2, 0.75)
    else
        rowFrame.bg:SetColorTexture(0.06, 0.07, 0.09, 0.55)
    end

    rowFrame.meta:SetText(string.format("%s · %s", record.zone or "?", fmtDistance(record.dist)))

    if not record.online then
        -- Offline: bars carry no information — swap them for the inline
        -- "Log In" action and an explicit status subtext.
        rowFrame.hp:Hide()
        rowFrame.mp:Hide()
        rowFrame.action:Show()
        rowFrame.sub:SetText("|cff888888offline|r")
        rowFrame:Show()
        return
    end
    rowFrame.action:Hide()
    rowFrame.hp:Show()
    rowFrame.mp:Show()

    rowFrame.hp:SetValue(record.hpPct or 0)
    if record.hpPct and record.hpPct < 35 then
        rowFrame.hp:SetStatusBarColor(0.95, 0.25, 0.25)
    elseif record.hpPct and record.hpPct < 65 then
        rowFrame.hp:SetStatusBarColor(0.95, 0.80, 0.30)
    else
        rowFrame.hp:SetStatusBarColor(0.30, 0.85, 0.35)
    end
    rowFrame.hp.text:SetText(string.format("%d%%", record.hpPct or 0))

    rowFrame.mp:SetValue(record.manaPct or 0)
    rowFrame.sub:SetText(string.format("|cffaaaaaa%s|r  |cff888888%s|r",
        record.intent or "-", record.lastRule or "-"))
    rowFrame:Show()
end

------------------------------------------------------------------ sorting
local SORTERS = {
    role = function(a, b)
        local ra = ROLE_ORDER[(a.role or "UNKNOWN"):upper()] or 9
        local rb = ROLE_ORDER[(b.role or "UNKNOWN"):upper()] or 9
        if ra ~= rb then return ra < rb end
        return (a.name or "") < (b.name or "")
    end,
    name     = function(a, b) return (a.name or "") < (b.name or "") end,
    class    = function(a, b) return (a.class or "") < (b.class or "") end,
    level    = function(a, b) return (a.level or 0) > (b.level or 0) end,
    distance = function(a, b)
        local da = a.dist or 1e9; if da < 0 then da = 1e9 end
        local db = b.dist or 1e9; if db < 0 then db = 1e9 end
        return da < db
    end,
}

local function applySort()
    local s = SORTERS[(PBC.DB.roster.sortBy or "role")] or SORTERS.role
    table.sort(M.roster, s)
end

------------------------------------------------------------------ layout
local function relayout()
    if not M.frame or not M.frame:IsShown() then return end
    applySort()

    -- Apply filter.
    local visible = {}
    for _, rec in ipairs(M.roster) do
        if rec.online or PBC.DB.roster.showOffline then
            visible[#visible + 1] = rec
        end
    end

    -- Scroll clamp.
    local maxScroll = math.max(0, #visible - M.MAX_ROWS)
    if M.scroll > maxScroll then M.scroll = maxScroll end
    if M.scroll < 0 then M.scroll = 0 end

    -- Place rows.
    for i = 1, M.MAX_ROWS do
        local row = M.rows[i]
        if not row then
            row = buildRow(M.frame.list, i)
            row:SetPoint("TOPLEFT",  M.frame.list, "TOPLEFT",  0, -(i - 1) * M.ROW_H)
            row:SetPoint("TOPRIGHT", M.frame.list, "TOPRIGHT", 0, -(i - 1) * M.ROW_H)
            M.rows[i] = row
        end
        local rec = visible[i + M.scroll]
        if rec then
            populateRow(row, rec)
        else
            row.record = nil
            row:Hide()
        end
    end

    M.frame.count:SetText(string.format("%d / %d", #visible, #M.roster))
    if M.frame.scrollbar then
        M.frame.scrollbar:SetMinMaxValues(0, maxScroll)
        M.frame.scrollbar:SetValue(M.scroll)
    end
end
M.Relayout = relayout

------------------------------------------------------------------ build frame
local function styleTitlebar(f)
    f.titlebar = CreateFrame("Frame", nil, f)
    f.titlebar:SetPoint("TOPLEFT", 0, 0)
    f.titlebar:SetPoint("TOPRIGHT", 0, 0)
    f.titlebar:SetHeight(22)
    local tb = f.titlebar:CreateTexture(nil, "BACKGROUND")
    tb:SetAllPoints()
    tb:SetColorTexture(0.10, 0.12, 0.18, 0.92)

    f.title = f.titlebar:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    f.title:SetPoint("LEFT", 8, 0)
    f.title:SetText("Playerbot Roster")

    f.count = f.titlebar:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    f.count:SetPoint("RIGHT", -32, 0)

    f.closeBtn = CreateFrame("Button", nil, f.titlebar, "UIPanelCloseButton")
    f.closeBtn:SetPoint("TOPRIGHT", 0, 0)
    f.closeBtn:SetScript("OnClick", function() f:Hide() end)

    -- Drag handle on titlebar
    f.titlebar:EnableMouse(true)
    f.titlebar:RegisterForDrag("LeftButton")
    f.titlebar:SetScript("OnDragStart", function() f:StartMoving() end)
    f.titlebar:SetScript("OnDragStop",  function()
        f:StopMovingOrSizing()
        local p, _, rel, x, y = f:GetPoint()
        PBC.DB.roster.point    = p
        PBC.DB.roster.relPoint = rel
        PBC.DB.roster.x        = x
        PBC.DB.roster.y        = y
    end)
end

local function styleFooter(f)
    f.footer = CreateFrame("Frame", nil, f)
    f.footer:SetPoint("BOTTOMLEFT", 0, 0)
    f.footer:SetPoint("BOTTOMRIGHT", 0, 0)
    f.footer:SetHeight(22)
    local fb = f.footer:CreateTexture(nil, "BACKGROUND")
    fb:SetAllPoints()
    fb:SetColorTexture(0.08, 0.09, 0.13, 0.92)

    -- Sort dropdown
    f.sortBtn = CreateFrame("Button", nil, f.footer, "UIPanelButtonTemplate")
    f.sortBtn:SetSize(80, 18)
    f.sortBtn:SetPoint("LEFT", 4, 0)
    f.sortBtn:SetText("Sort: role")
    f.sortBtn:SetScript("OnClick", function()
        local order = { "role", "name", "class", "level", "distance" }
        local cur = PBC.DB.roster.sortBy or "role"
        local idx = 1
        for i, k in ipairs(order) do if k == cur then idx = i; break end end
        idx = idx % #order + 1
        PBC.DB.roster.sortBy = order[idx]
        f.sortBtn:SetText("Sort: " .. order[idx])
        relayout()
    end)

    -- Offline toggle
    f.offlineBtn = CreateFrame("CheckButton", nil, f.footer, "UICheckButtonTemplate")
    f.offlineBtn:SetSize(18, 18)
    f.offlineBtn:SetPoint("LEFT", f.sortBtn, "RIGHT", 6, 0)
    f.offlineBtn.text = f.offlineBtn:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    f.offlineBtn.text:SetPoint("LEFT", f.offlineBtn, "RIGHT", 2, 0)
    f.offlineBtn.text:SetText("Offline")
    f.offlineBtn:SetChecked(PBC.DB.roster.showOffline or false)
    f.offlineBtn:SetScript("OnClick", function(self)
        PBC.DB.roster.showOffline = self:GetChecked() and true or false
        relayout()
    end)

    -- Refresh
    f.refreshBtn = CreateFrame("Button", nil, f.footer, "UIPanelButtonTemplate")
    f.refreshBtn:SetSize(70, 18)
    f.refreshBtn:SetPoint("RIGHT", -4, 0)
    f.refreshBtn:SetText("Refresh")
    f.refreshBtn:SetScript("OnClick", function()
        PBC.Comms.RosterReq(15)
    end)

    -- Spawn Alt opens the alt-picker modal. Tucked left of Refresh so the
    -- two together form the "manage roster contents" cluster.
    f.spawnAltBtn = CreateFrame("Button", nil, f.footer, "UIPanelButtonTemplate")
    f.spawnAltBtn:SetSize(86, 18)
    f.spawnAltBtn:SetPoint("RIGHT", f.refreshBtn, "LEFT", -4, 0)
    f.spawnAltBtn:SetText("Spawn Alt…")
    f.spawnAltBtn:SetScript("OnClick", function()
        if PBC.BotAlts then PBC.BotAlts.Show() end
    end)
end

function M.Build()
    if M.frame then return M.frame end

    local f = CreateFrame("Frame", "PlayerbotControlRoster", UIParent, "BackdropTemplate")
    f:SetSize(PBC.DB.roster.w or 280, PBC.DB.roster.h or 480)
    f:SetPoint(PBC.DB.roster.point or "CENTER",
               UIParent,
               PBC.DB.roster.relPoint or "CENTER",
               PBC.DB.roster.x or -300,
               PBC.DB.roster.y or 0)
    f:SetScale(PBC.DB.roster.scale or 1.0)
    f:SetMovable(true)
    f:SetResizable(true)
    if f.SetResizeBounds then f:SetResizeBounds(220, 200, 480, 800) end
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
        f:SetBackdropColor(0.05, 0.05, 0.07, 0.95)
    end

    styleTitlebar(f)
    styleFooter(f)

    -- list area
    f.list = CreateFrame("Frame", nil, f)
    f.list:SetPoint("TOPLEFT", 4, -24)
    f.list:SetPoint("BOTTOMRIGHT", -24, 24)
    f.list:EnableMouseWheel(true)
    f.list:SetScript("OnMouseWheel", function(_, delta)
        M.scroll = M.scroll - delta
        relayout()
    end)

    -- vertical scrollbar
    f.scrollbar = CreateFrame("Slider", nil, f, "UIPanelScrollBarTemplate")
    f.scrollbar:SetPoint("TOPRIGHT", -6, -40)
    f.scrollbar:SetPoint("BOTTOMRIGHT", -6, 40)
    f.scrollbar:SetMinMaxValues(0, 0)
    f.scrollbar:SetValueStep(1)
    f.scrollbar:SetWidth(16)
    f.scrollbar:SetScript("OnValueChanged", function(_, v)
        M.scroll = math.floor(v + 0.5)
        relayout()
    end)

    -- Resize grip
    f.resize = CreateFrame("Button", nil, f)
    f.resize:SetSize(14, 14)
    f.resize:SetPoint("BOTTOMRIGHT", 0, 0)
    f.resize:SetNormalTexture("Interface\\ChatFrame\\UI-ChatIM-SizeGrabber-Up")
    f.resize:SetPushedTexture("Interface\\ChatFrame\\UI-ChatIM-SizeGrabber-Down")
    f.resize:SetHighlightTexture("Interface\\ChatFrame\\UI-ChatIM-SizeGrabber-Highlight")
    f.resize:SetScript("OnMouseDown", function() f:StartSizing("BOTTOMRIGHT") end)
    f.resize:SetScript("OnMouseUp", function()
        f:StopMovingOrSizing()
        PBC.DB.roster.w = f:GetWidth()
        PBC.DB.roster.h = f:GetHeight()
    end)

    M.frame = f
    M.RegisterHandlers()
    return f
end

------------------------------------------------------------------ protocol handlers
function M.RegisterHandlers()
    PBC.Comms.RegisterHandler("ROSTER_RESP", function(fields, sender)
        local count = tonumber(fields[1]) or 0
        local newRoster, byGuid = {}, {}
        for i = 1, count do
            local rec = fields[1 + i]
            if rec and rec ~= "" then
                local r = PBC.Comms.DecodeRosterRecord(rec)
                newRoster[#newRoster + 1] = r
                byGuid[r.guidLow] = r
                -- Remember role for command-bar autocomplete.
                if PBC.DB and PBC.DB.knownBots and r.name then
                    PBC.DB.knownBots[r.name] = r.role or "?"
                end
            end
        end
        M.roster = newRoster
        M.byGuid = byGuid
        relayout()
    end)
end

------------------------------------------------------------------ tick / show
local pollAccum = 0
function M.OnTick(elapsed)
    if not M.frame or not M.frame:IsShown() then return end
    -- One-shot refresh scheduled after a state-changing action
    -- (login/logout/spawn) — same pattern as BotAlts._pendingRefresh.
    if M._pendingRefresh and GetTime() >= M._pendingRefresh then
        M._pendingRefresh = nil
        pollAccum = 0
        PBC.Comms.RosterReq(15)
        return
    end
    pollAccum = pollAccum + elapsed
    if pollAccum >= 3.0 then
        pollAccum = 0
        PBC.Comms.RosterReq(15)
    end
end

-- Schedule a one-shot roster re-fetch `delay` seconds out. Public so other
-- panels (alt picker) can nudge the roster after they change bot state.
function M.RequestRefresh(delay)
    M._pendingRefresh = GetTime() + (delay or 1.0)
end

function M.Show()
    if not M.frame then M.Build() end
    M.frame:Show()
    PBC.DB.roster.shown = true
    PBC.Comms.RosterReq(15)
    relayout()
end

function M.Hide()
    if M.frame then M.frame:Hide(); PBC.DB.roster.shown = false end
end

function M.Toggle()
    if M.frame and M.frame:IsShown() then M.Hide() else M.Show() end
end

function M.GetRoster() return M.roster end
function M.GetByGuid(g) return M.byGuid[g] end

function M.GetNames()
    local out = {}
    for _, r in ipairs(M.roster) do out[#out + 1] = r.name end
    return out
end

-- vim: ts=4 sw=4 et
