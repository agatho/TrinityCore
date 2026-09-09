--[[============================================================================
 BotToolbar.lua  —  click-to-fire group quick-action bar.

 Layout:
   ┌─ Addr: [ all ▾ ] ────────────────────────────────────────────────┐
   │ [Follow][Stop][Come][Hold][Engage][Assist][Rez]                  │
   │ [Tight][Spread][Line][Wedge][Mount][Dismount][Hearth]            │
   │ [Repair][Sell][Loot][Buff]                                       │
   │ [Mark ▸][BG ▸][LFG ▸][Pause][Resume][Login][Logout][Dismiss]     │
   └──────────────────────────────────────────────────────────────────┘

 Click → Comms.Cmd(<currentAddr>, <verb>, args). The address dropdown
 cycles all / squad / tank / healer / dps / mage / warrior / … so a single
 click fires the same intent the textual command bar produces. The right-
 most submenu buttons (Mark / BG / LFG) open a small popup of icons/queues.

 Verbs include both "obvious" group orders and a handful that previously
 needed typed input (Hearth, Repair, Sell, Dismiss). Every verb here is
 already accepted by BotCommandParser — the toolbar is pure UI sugar.

 Two ways the user wins by clicking instead of typing:
   1. Fewer chances to fat-finger a verb name (e.g. `engage_focus`).
   2. Address picker is impossible to typo: dropdown enforces a known set.

 Position is persisted via PBC.DB.toolbar (point/relPoint/x/y/shown).
============================================================================]]

local PBC = _G.PlayerbotControl
local M = {}
PBC.BotToolbar = M

M.frame = nil
M.addressBtn = nil
M.selfBtn    = nil
M.selfAttached = nil   -- nil = unknown, true/false once SELF_RESP arrives

------------------------------------------------------------------ vocab
M.ADDRESSES = {
    "all", "squad", "tank", "healer", "dps",
    "warrior", "paladin", "hunter", "rogue", "priest",
    "deathknight", "shaman", "mage", "warlock", "monk",
    "druid", "demonhunter", "evoker",
}

M.MARK_ICONS = { "skull", "cross", "star", "circle",
                 "moon", "diamond", "square", "triangle" }

M.BG_TYPES = { "wsg", "ab", "av", "eots", "sota", "ioc", "bg",
               "tp", "kotmogu", "dg", "ss", "tk", "ashran",
               "wint", "seething" }

-- LFG dungeon shortcuts; empty arg dispatches "any random dungeon" path
-- on the server (verb without name in BotCommandParser).
M.LFG_HINTS = { "", "deadmines", "stockades", "wailing-caverns",
                "scarlet-monastery", "uldaman", "blackrock-depths",
                "scholomance", "stratholme", "hellfire-ramparts",
                "utgarde-keep", "halls-of-stone", "blackrock-caverns" }

-- "Quick" verbs that fire instantly on click — no submenu.
-- Each entry is { label, verb, optional argsBuilder(addr) -> table }.
M.QUICK = {
    -- movement / posture
    { label = "Follow",   verb = "follow",
      args  = function() return { UnitName("player") } end },
    { label = "Stop",     verb = "stop" },
    { label = "Come",     verb = "come" },
    { label = "Hold",     verb = "hold" },
    { label = "Tight",    verb = "form", args = function() return { "tight"  } end },
    { label = "Spread",   verb = "form", args = function() return { "spread" } end },
    { label = "Line",     verb = "form", args = function() return { "line"   } end },
    { label = "Wedge",    verb = "form", args = function() return { "wedge"  } end },
    -- combat
    { label = "Engage",   verb = "engage_focus",
      args  = function() return { UnitName("target") } end,
      needsTarget = true },
    { label = "Assist",   verb = "assist",
      args  = function() return { UnitName("target") } end,
      needsTarget = true },
    { label = "Pull",     verb = "pull" },
    { label = "Rez",      verb = "ghost_res" },
    -- utility
    { label = "Mount",    verb = "mount" },
    { label = "Dismount", verb = "dismount" },
    { label = "Hearth",   verb = "use_hearth" },
    { label = "Repair",   verb = "repair" },
    { label = "Sell",     verb = "sell",   args = function() return { "trash" } end },
    { label = "Loot",     verb = "loot_roll" },
    { label = "Buff",     verb = "buff" },
    -- queues
    { label = "BG Leave", verb = "bg_leave" },
    { label = "LFG Leave",verb = "lfg_leave" },
    { label = "Ready",    verb = "ready" },
    -- meta
    { label = "Pause",    verb = "pause" },
    { label = "Resume",   verb = "resume" },
    -- Login/Logout are NOT CMD verbs — they ride dedicated LOGIN_REQ /
    -- LOGOUT_REQ frames (offline bots have no session for CMD to reach).
    -- PBC.RequestLogin/Logout expand the current address against the last
    -- roster snapshot and fire one frame per matching character.
    { label = "Login",    custom = function(addr) PBC.RequestLogin(addr)  end },
    { label = "Logout",   custom = function(addr) PBC.RequestLogout(addr) end },
    { label = "Dismiss",  verb = "dismiss" },
}

------------------------------------------------------------------ DB defaults
local function db()
    PBC.DB.toolbar = PBC.DB.toolbar or {
        point = "BOTTOM", relPoint = "BOTTOM", x = 0, y = 240,
        shown = false, address = "all",
    }
    return PBC.DB.toolbar
end

------------------------------------------------------------------ address selector
local function setAddress(addr)
    db().address = addr
    if M.addressBtn then M.addressBtn:SetText("Addr: " .. addr) end
end

local function cycleAddress(dir)
    dir = dir or 1
    local cur = db().address or "all"
    local idx = 1
    for i, a in ipairs(M.ADDRESSES) do
        if a == cur then idx = i; break end
    end
    idx = ((idx - 1 + dir) % #M.ADDRESSES) + 1
    setAddress(M.ADDRESSES[idx])
end

local function showAddressMenu(anchor)
    local menu = M._addressMenu
    if not menu then
        menu = CreateFrame("Frame", "PBCToolbarAddrMenu", UIParent,
                           "UIDropDownMenuTemplate")
        M._addressMenu = menu
    end
    local items = {}
    for _, addr in ipairs(M.ADDRESSES) do
        items[#items + 1] = {
            text = addr, notCheckable = true,
            func = function() setAddress(addr) end,
        }
    end
    PBC.OpenMenu(items, anchor, "MENU")
end

------------------------------------------------------------------ click → CMD
local function fire(verb, args)
    local addr = db().address or "all"
    PBC.Comms.Cmd(addr, verb, args or {})
    if args and #args > 0 then
        PBC.Print("→ %s %s %s", addr, verb, table.concat(args, " "))
    else
        PBC.Print("→ %s %s", addr, verb)
    end
end

------------------------------------------------------------------ submenus
local function showMarkMenu(anchor)
    local menu = M._markMenu
    if not menu then
        menu = CreateFrame("Frame", "PBCToolbarMarkMenu", UIParent,
                           "UIDropDownMenuTemplate")
        M._markMenu = menu
    end
    local items = { { text = "Mark target", isTitle = true,
                      notCheckable = true } }
    for _, icon in ipairs(M.MARK_ICONS) do
        items[#items + 1] = {
            text = icon, notCheckable = true,
            func = function()
                local tgt = UnitName("target")
                if not tgt then PBC.Warn("no target to mark"); return end
                fire("mark", { icon, tgt })
            end,
        }
    end
    PBC.OpenMenu(items, anchor, "MENU")
end

local function showBgMenu(anchor)
    local menu = M._bgMenu
    if not menu then
        menu = CreateFrame("Frame", "PBCToolbarBgMenu", UIParent,
                           "UIDropDownMenuTemplate")
        M._bgMenu = menu
    end
    local items = { { text = "Queue BG", isTitle = true, notCheckable = true } }
    for _, b in ipairs(M.BG_TYPES) do
        items[#items + 1] = {
            text = b, notCheckable = true,
            func = function() fire("bg_queue", { b }) end,
        }
    end
    items[#items + 1] = {
        text = "Leave BG queue", notCheckable = true,
        func = function() fire("bg_leave") end,
    }
    PBC.OpenMenu(items, anchor, "MENU")
end

local function showLfgMenu(anchor)
    local menu = M._lfgMenu
    if not menu then
        menu = CreateFrame("Frame", "PBCToolbarLfgMenu", UIParent,
                           "UIDropDownMenuTemplate")
        M._lfgMenu = menu
    end
    local items = { { text = "Queue LFG", isTitle = true, notCheckable = true } }
    for _, d in ipairs(M.LFG_HINTS) do
        local lbl = (d == "") and "(random)" or d
        items[#items + 1] = {
            text = lbl, notCheckable = true,
            func = function()
                if d == "" then fire("lfg_queue")
                else fire("lfg_queue", { d }) end
            end,
        }
    end
    items[#items + 1] = {
        text = "Leave LFG queue", notCheckable = true,
        func = function() fire("lfg_leave") end,
    }
    PBC.OpenMenu(items, anchor, "MENU")
end

------------------------------------------------------------------ button factory
local BTN_W = 64
local BTN_H = 20
local BTN_GAP = 2
local PAD_X = 6
local PAD_TOP = 22
local ROW_GAP = 2

local function makeButton(parent, label, onClick)
    local b = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
    b:SetSize(BTN_W, BTN_H)
    b:SetText(label)
    b:SetScript("OnClick", onClick)
    return b
end

------------------------------------------------------------------ layout (wrap)
local function flow(f)
    -- Adjust frame width to whatever's available, then flow buttons left→
    -- right top→bottom. Wrap when next button would exceed `usableW`.
    local usableW = f:GetWidth() - 2 * PAD_X
    local x, y = PAD_X, -PAD_TOP
    for _, b in ipairs(f._buttons) do
        if x + BTN_W > PAD_X + usableW then
            x = PAD_X
            y = y - (BTN_H + ROW_GAP)
        end
        b:ClearAllPoints()
        b:SetPoint("TOPLEFT", f, "TOPLEFT", x, y)
        x = x + BTN_W + BTN_GAP
    end
    -- Resize height to hold all rows.
    f:SetHeight(-y + BTN_H + 6)
end

------------------------------------------------------------------ build
function M.Build()
    if M.frame then return M.frame end

    local f = CreateFrame("Frame", "PlayerbotControlToolbar", UIParent,
                          "BackdropTemplate")
    f:SetSize(720, 96)
    local d = db()
    f:SetPoint(d.point or "BOTTOM", UIParent,
               d.relPoint or "BOTTOM", d.x or 0, d.y or 240)
    f:SetMovable(true)
    f:EnableMouse(true)
    f:SetClampedToScreen(true)
    f:RegisterForDrag("LeftButton")
    f:SetScript("OnDragStart", function() f:StartMoving() end)
    f:SetScript("OnDragStop",  function()
        f:StopMovingOrSizing()
        local p, _, rel, x, y = f:GetPoint()
        d.point = p; d.relPoint = rel; d.x = x; d.y = y
    end)
    f:Hide()

    if f.SetBackdrop then
        f:SetBackdrop({
            bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
            edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
            tile = true, tileSize = 16, edgeSize = 12,
            insets = { left = 3, right = 3, top = 3, bottom = 3 },
        })
        f:SetBackdropColor(0.04, 0.04, 0.06, 0.92)
    end

    -- Titlebar with drag handle + address picker + close
    f.titlebar = CreateFrame("Frame", nil, f)
    f.titlebar:SetPoint("TOPLEFT", 0, 0)
    f.titlebar:SetPoint("TOPRIGHT", 0, 0)
    f.titlebar:SetHeight(20)
    local tb = f.titlebar:CreateTexture(nil, "BACKGROUND")
    tb:SetAllPoints()
    tb:SetColorTexture(0.10, 0.12, 0.18, 0.92)

    f.title = f.titlebar:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    f.title:SetPoint("LEFT", 6, 0)
    f.title:SetText("Quick Orders")

    M.addressBtn = CreateFrame("Button", nil, f.titlebar,
                               "UIPanelButtonTemplate")
    M.addressBtn:SetSize(110, 18)
    M.addressBtn:SetPoint("LEFT", f.title, "RIGHT", 12, 0)
    M.addressBtn:SetText("Addr: " .. (d.address or "all"))
    M.addressBtn:RegisterForClicks("LeftButtonUp", "RightButtonUp")
    M.addressBtn:SetScript("OnClick", function(self, button)
        if button == "RightButton" then cycleAddress(-1)
        elseif IsShiftKeyDown() then cycleAddress(1)
        else showAddressMenu(self) end
    end)

    f.closeBtn = CreateFrame("Button", nil, f.titlebar,
                             "UIPanelCloseButton")
    f.closeBtn:SetPoint("TOPRIGHT", 0, 0)
    f.closeBtn:SetScript("OnClick", function() M.Hide() end)

    -- Self-AI toggle (right of the address picker). Three visible states:
    --   "Self: ?"   — unknown, waiting on first SELF_RESP from server
    --   "Self: OFF" — AI not attached; click flips to on
    --   "Self: ON"  — AI driving the caller's character; click flips off
    -- The button is the only widget here that touches the caller's OWN
    -- char rather than the bot fleet, so it sits next to the address
    -- picker for visual continuity ("scope of effect" cluster).
    M.selfBtn = CreateFrame("Button", nil, f.titlebar, "UIPanelButtonTemplate")
    M.selfBtn:SetSize(96, 18)
    M.selfBtn:SetPoint("LEFT", M.addressBtn, "RIGHT", 8, 0)
    M.selfBtn:SetText("Self: ?")
    M.selfBtn:SetScript("OnClick", function()
        if M.selfAttached == nil then
            -- Don't toggle blind; query state first then let user re-click.
            PBC.Comms.Self("status")
            PBC.Print("querying self-AI state…")
            return
        end
        if M.selfAttached then PBC.Comms.Self("off")
        else                   PBC.Comms.Self("on")  end
    end)
    M.selfBtn:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:AddLine("Self-AI")
        GameTooltip:AddLine(
            "Toggle the PlayerbotV2 AI on your own character.\n" ..
            "Same as `.playerbot self on` / `off`.\n" ..
            "Your client input keeps working — pressing a movement key " ..
            "interrupts the AI's move intents.", 1, 1, 1, true)
        GameTooltip:Show()
    end)
    M.selfBtn:SetScript("OnLeave", function() GameTooltip:Hide() end)

    -- Buttons
    f._buttons = {}
    for _, q in ipairs(M.QUICK) do
        local b = makeButton(f, q.label, function()
            if q.custom then
                q.custom(db().address or "all")
                return
            end
            local args
            if q.args then
                args = q.args()
                if q.needsTarget and (not args[1] or args[1] == "") then
                    PBC.Warn("%s needs a target", q.label); return
                end
            end
            fire(q.verb, args)
        end)
        f._buttons[#f._buttons + 1] = b
    end
    -- Submenu buttons
    local markBtn = makeButton(f, "Mark ▸", function(self) showMarkMenu(self) end)
    local bgBtn   = makeButton(f, "BG ▸",   function(self) showBgMenu(self) end)
    local lfgBtn  = makeButton(f, "LFG ▸",  function(self) showLfgMenu(self) end)
    f._buttons[#f._buttons + 1] = markBtn
    f._buttons[#f._buttons + 1] = bgBtn
    f._buttons[#f._buttons + 1] = lfgBtn

    flow(f)
    M.frame = f
    M.RegisterHandlers()
    return f
end

------------------------------------------------------------------ protocol
function M.RegisterHandlers()
    PBC.Comms.RegisterHandler("SELF_RESP", function(fields, _sender)
        -- Fields are key=value strings; ALTS_RESP-style decode.
        local kv = {}
        for _, f in ipairs(fields) do
            local k, v = f:match("^([^=]+)=(.*)$")
            if k then kv[k] = v end
        end
        local attached = (kv.attached == "1")
        M.selfAttached = attached
        if M.selfBtn then
            M.selfBtn:SetText(attached and "Self: ON" or "Self: OFF")
        end
    end)
end

------------------------------------------------------------------ public
function M.Show()
    if not M.frame then M.Build() end
    M.frame:Show()
    db().shown = true
    -- Refresh the Self toggle state so the button shows server truth.
    PBC.Comms.Self("status")
end

function M.Hide()
    if M.frame then M.frame:Hide() end
    db().shown = false
end

function M.Toggle()
    if M.frame and M.frame:IsShown() then M.Hide() else M.Show() end
end

function M.SetAddress(addr) setAddress(addr) end

-- vim: ts=4 sw=4 et
