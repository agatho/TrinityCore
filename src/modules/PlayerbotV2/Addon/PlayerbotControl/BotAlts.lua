--[[============================================================================
 BotAlts.lua  —  "Spawn one of my alts as a bot" picker.

 Look:
   ┌──────────────────────────────────────────────────┐
   │ Spawn an alt                                  [X]│
   ├──────────────────────────────────────────────────┤
   │ ⚔ Areon       L80 Warrior   Human          [Spawn]│  ← not a bot, offline
   │ ✚ Healme      L80 Priest    Dwarf        [Log In] │  ← marked bot, offline
   │ ☠ Botty       L70 Rogue     Gnome        [Logout] │  ← in world as a bot
   │ ☣ Lockyou     L74 Warlock   Gnome      [In World] │  ← real client session
   │ - Yourself    L80 Mage      Human        [/self]  │  ← caller's char
   │ …                                                │
   └──────────────────────────────────────────────────┘

 Row states (decided from online/isBot/isHeadless/isSelf in the record):
   * Spawn   — offline + not marked: SUMMON marks + binds + headless-login.
   * Log In  — offline + marked bot: LOGIN_REQ re-enters world headless.
   * Logout  — online as a HEADLESS bot session: LOGOUT_REQ kicks it.
   * In World— online as a REAL client (locked; never logout-able here).
   * /self   — the caller's own character (locked; use /pbc self on).

 Data flow:
   * Show() → Comms.AltsReq().
   * ALTS_RESP handler parses records, sorts (actionable rows first:
     spawnable, then log-in-able, then logout-able; locked rows last),
     repopulates rows.
   * Action click → Comms.Summon / LoginReq / LogoutReq. The server
     replies with EVENT_PUSH (info/warn) which the core handler already
     pipes to chat — no per-modal feedback wiring needed.

 Design notes:
   * Picker is INTENTIONALLY modal-ish (single instance, top strata) so a
     misclick on a row doesn't accidentally race with a roster click.
   * We refresh the alts list 1s after every action click so the row
     flips state (Spawn→Logout, Log In→Logout, …) without manual /reload.
============================================================================]]

local PBC = _G.PlayerbotControl
local M = {}
PBC.BotAlts = M

M.frame    = nil
M.rows     = {}
M.MAX_ROWS = 14
M.ROW_H    = 24
M.alts     = {}
M.scroll   = 0
M._pendingRefresh = nil

------------------------------------------------------------------ helpers
local CLASS_COLORS = RAID_CLASS_COLORS or {}

local function classColor(class)
    local c = CLASS_COLORS[class and class:upper() or ""]
    if c then return c.r, c.g, c.b end
    return 0.85, 0.85, 0.85
end

-- Row state machine. Returns label, clickable, action where action is one
-- of "spawn" / "login" / "logout" / nil (locked row). Colors follow the
-- shared palette semantics: green = safe create/spawn, blue = bot session
-- control, red = locked by a real player session, gray = self.
local function statusText(rec)
    if rec.isSelf then
        return "|cffaaaaaa/self|r", false, nil
    elseif rec.online and rec.isHeadless then
        return "|cff66ccffLogout|r", true, "logout"
    elseif rec.online then
        return "|cffff6655In World|r", false, nil
    elseif rec.isBot then
        return "|cff66ccffLog In|r", true, "login"
    else
        return "|cff66ff66Spawn|r", true, "spawn"
    end
end

local function raceLabel(r)
    if not r then return "?" end
    -- Lower-case for readability; first letter capitalized.
    local s = r:lower():gsub("_", " ")
    return s:sub(1, 1):upper() .. s:sub(2)
end

local function classLabel(c)
    if not c then return "?" end
    local s = c:lower():gsub("_", " ")
    return s:sub(1, 1):upper() .. s:sub(2)
end

------------------------------------------------------------------ sort
-- Actionable rows come first so the user's eye lands on choices that do
-- something: spawnable, then log-in-able bots, then logout-able live bots;
-- locked rows (real client sessions, self) sink to the bottom. Inside each
-- bucket, sort by descending level then name — matches the server ORDER BY.
local function sortAlts(alts)
    local function bucket(rec)
        if rec.isSelf then return 5 end
        if rec.online and rec.isHeadless then return 3 end
        if rec.online then return 4 end
        if rec.isBot  then return 2 end
        return 1
    end
    table.sort(alts, function(a, b)
        local ba, bb = bucket(a), bucket(b)
        if ba ~= bb then return ba < bb end
        if (a.level or 0) ~= (b.level or 0) then
            return (a.level or 0) > (b.level or 0)
        end
        return (a.name or "") < (b.name or "")
    end)
end

------------------------------------------------------------------ row factory
local function buildRow(parent, index)
    local f = CreateFrame("Frame", "PBCAltsRow" .. index, parent)
    f:SetHeight(M.ROW_H)

    f.bg = f:CreateTexture(nil, "BACKGROUND")
    f.bg:SetAllPoints()
    f.bg:SetColorTexture(0.06, 0.07, 0.09, 0.55)

    f.name = f:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    f.name:SetPoint("LEFT", 6, 0)
    f.name:SetJustifyH("LEFT")

    f.meta = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    f.meta:SetPoint("LEFT", 170, 0)
    f.meta:SetJustifyH("LEFT")

    f.action = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    f.action:SetSize(78, 20)
    f.action:SetPoint("RIGHT", -6, 0)
    f.action:SetScript("OnClick", function(self)
        local rec = self:GetParent().record
        if not rec then return end
        local _, _, action = statusText(rec)
        if action == "spawn" then
            PBC.Comms.Summon(rec.name)
            PBC.Print("→ summon %s", rec.name)
        elseif action == "login" then
            PBC.Comms.LoginReq(rec.name)
            PBC.Print("→ login %s", rec.name)
        elseif action == "logout" then
            PBC.Comms.LogoutReq(rec.name)
            PBC.Print("→ logout %s", rec.name)
        elseif rec.isSelf then
            PBC.Print("That's your current character. Try /pbc self on.")
            return
        else
            PBC.Warn("'%s' is online as a real player — locked.", rec.name)
            return
        end
        -- Refresh shortly so the row flips state. The roster shows the
        -- same character set, so nudge it too if it's open.
        M._pendingRefresh = GetTime() + 1.0
        if PBC.BotRoster and PBC.BotRoster.RequestRefresh then
            PBC.BotRoster.RequestRefresh(1.5)
        end
    end)

    return f
end

local function populateRow(rowFrame, rec)
    rowFrame.record = rec
    local r, g, b = classColor(rec.class)
    rowFrame.name:SetText(string.format(
        "|cff%02x%02x%02x%s|r  L%d",
        r * 255, g * 255, b * 255, rec.name or "?", rec.level or 0))
    rowFrame.meta:SetText(string.format("%s · %s",
        classLabel(rec.class), raceLabel(rec.race)))
    local txt, clickable = statusText(rec)
    rowFrame.action:SetText(txt)
    rowFrame.action:SetEnabled(clickable)
    rowFrame:Show()
end

------------------------------------------------------------------ layout
local function relayout()
    if not M.frame or not M.frame:IsShown() then return end

    -- Clamp scroll.
    local maxScroll = math.max(0, #M.alts - M.MAX_ROWS)
    if M.scroll > maxScroll then M.scroll = maxScroll end
    if M.scroll < 0 then M.scroll = 0 end

    for i = 1, M.MAX_ROWS do
        local row = M.rows[i]
        if not row then
            row = buildRow(M.frame.list, i)
            row:SetPoint("TOPLEFT",  M.frame.list, "TOPLEFT",  0,
                         -(i - 1) * M.ROW_H)
            row:SetPoint("TOPRIGHT", M.frame.list, "TOPRIGHT", 0,
                         -(i - 1) * M.ROW_H)
            M.rows[i] = row
        end
        local rec = M.alts[i + M.scroll]
        if rec then populateRow(row, rec)
        else row.record = nil; row:Hide() end
    end

    M.frame.count:SetText(string.format("%d alt%s",
        #M.alts, (#M.alts == 1) and "" or "s"))
end
M.Relayout = relayout

------------------------------------------------------------------ build
function M.Build()
    if M.frame then return M.frame end

    local f = CreateFrame("Frame", "PlayerbotControlAlts", UIParent,
                          "BackdropTemplate")
    f:SetSize(420, 24 + M.MAX_ROWS * M.ROW_H + 8)
    f:SetPoint("CENTER")
    f:SetMovable(true)
    f:EnableMouse(true)
    f:SetClampedToScreen(true)
    f:SetFrameStrata("DIALOG")
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

    -- Titlebar
    f.titlebar = CreateFrame("Frame", nil, f)
    f.titlebar:SetPoint("TOPLEFT", 0, 0)
    f.titlebar:SetPoint("TOPRIGHT", 0, 0)
    f.titlebar:SetHeight(22)
    local tb = f.titlebar:CreateTexture(nil, "BACKGROUND")
    tb:SetAllPoints()
    tb:SetColorTexture(0.10, 0.12, 0.18, 0.92)

    f.title = f.titlebar:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    f.title:SetPoint("LEFT", 8, 0)
    f.title:SetText("Spawn an alt")

    f.count = f.titlebar:CreateFontString(nil, "OVERLAY",
                                          "GameFontDisableSmall")
    f.count:SetPoint("RIGHT", -32, 0)

    f.closeBtn = CreateFrame("Button", nil, f.titlebar,
                             "UIPanelCloseButton")
    f.closeBtn:SetPoint("TOPRIGHT", 0, 0)
    f.closeBtn:SetScript("OnClick", function() M.Hide() end)

    f.titlebar:EnableMouse(true)
    f.titlebar:RegisterForDrag("LeftButton")
    f.titlebar:SetScript("OnDragStart", function() f:StartMoving() end)
    f.titlebar:SetScript("OnDragStop",  function() f:StopMovingOrSizing() end)

    -- List body
    f.list = CreateFrame("Frame", nil, f)
    f.list:SetPoint("TOPLEFT", 4, -24)
    f.list:SetPoint("BOTTOMRIGHT", -4, 22)
    f.list:EnableMouseWheel(true)
    f.list:SetScript("OnMouseWheel", function(_, delta)
        M.scroll = M.scroll - delta; relayout()
    end)

    -- Footer (refresh + helper text)
    f.footer = CreateFrame("Frame", nil, f)
    f.footer:SetPoint("BOTTOMLEFT", 0, 0)
    f.footer:SetPoint("BOTTOMRIGHT", 0, 0)
    f.footer:SetHeight(22)
    local fb = f.footer:CreateTexture(nil, "BACKGROUND")
    fb:SetAllPoints()
    fb:SetColorTexture(0.08, 0.09, 0.13, 0.92)

    f.hint = f.footer:CreateFontString(nil, "OVERLAY",
                                       "GameFontDisableSmall")
    f.hint:SetPoint("LEFT", 6, 0)
    f.hint:SetText("Bots: Log In / Logout.  Real-client sessions are locked.")

    f.refreshBtn = CreateFrame("Button", nil, f.footer,
                               "UIPanelButtonTemplate")
    f.refreshBtn:SetSize(70, 18)
    f.refreshBtn:SetPoint("RIGHT", -4, 0)
    f.refreshBtn:SetText("Refresh")
    f.refreshBtn:SetScript("OnClick", function() PBC.Comms.AltsReq() end)

    M.frame = f
    M.RegisterHandlers()
    return f
end

------------------------------------------------------------------ protocol
function M.RegisterHandlers()
    PBC.Comms.RegisterHandler("ALTS_RESP", function(fields, _sender)
        local count = tonumber(fields[1]) or 0
        local newAlts = {}
        for i = 1, count do
            local rec = fields[1 + i]
            if rec and rec ~= "" then
                newAlts[#newAlts + 1] = PBC.Comms.DecodeAltRecord(rec)
            end
        end
        sortAlts(newAlts)
        M.alts = newAlts
        relayout()
    end)
end

------------------------------------------------------------------ tick
function M.OnTick(_elapsed)
    if M._pendingRefresh and GetTime() >= M._pendingRefresh then
        M._pendingRefresh = nil
        if M.frame and M.frame:IsShown() then PBC.Comms.AltsReq() end
    end
end

------------------------------------------------------------------ public
function M.Show()
    if not M.frame then M.Build() end
    M.frame:Show()
    PBC.Comms.AltsReq()
end

function M.Hide()
    if M.frame then M.frame:Hide() end
end

function M.Toggle()
    if M.frame and M.frame:IsShown() then M.Hide() else M.Show() end
end

-- vim: ts=4 sw=4 et
