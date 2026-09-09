--[[============================================================================
 BotStats.lua  —  fleet dashboard, top of screen.

 Layout (560 x 56):
   ┌──────────────────────────────────────────────────────────────────────────┐
   │  Bots 412/2000  T 38  H 52  D 322  Wedged 2   intents/s 184 ▂▄▅▆▇▆▅▄▂▁ │
   │  Tick 24.7/33.3 ms  [████████░░░░░░░░░░] 74% budget                       │
   └──────────────────────────────────────────────────────────────────────────┘

 Sparkline samples last 30 STATS_RESP frames (over ~150s at 5s cadence).
 Colors:
   tick used  green <70%  yellow <90%  red ≥90%
   wedged > 0 always shown in red badge.
============================================================================]]

local PBC = _G.PlayerbotControl
local M = {}
PBC.BotStats = M

M.frame   = nil
M.spark   = { ips = {}, max = 1, len = 30 }   -- intents-per-second history

local function pushSample(v)
    table.insert(M.spark.ips, v)
    while #M.spark.ips > M.spark.len do table.remove(M.spark.ips, 1) end
    local mx = 1
    for _, s in ipairs(M.spark.ips) do if s > mx then mx = s end end
    M.spark.max = mx
end

------------------------------------------------------------------ sparkline rendering
local BARS = { "▁","▂","▃","▄","▅","▆","▇","█" }
local function sparkString()
    if #M.spark.ips == 0 then return "" end
    local out, mx = {}, M.spark.max
    for _, s in ipairs(M.spark.ips) do
        local frac = (mx > 0) and (s / mx) or 0
        local idx = math.max(1, math.min(#BARS, math.floor(frac * (#BARS - 1) + 1)))
        out[#out + 1] = BARS[idx]
    end
    return table.concat(out)
end

------------------------------------------------------------------ render
local function colorForTickPct(pct)
    if pct >= 0.90 then return "|cffff5050" end
    if pct >= 0.70 then return "|cffffcc33" end
    return "|cff66cc66"
end

local function render(stats)
    local f = M.frame; if not f then return end
    f.line1:SetText(string.format(
        "|cffffffffBots|r %d/%d  |cff66ccffT|r %d  |cffaa66ffH|r %d  |cffff7766D|r %d  " ..
        "%s   intents/s %d  %s",
        stats.online or 0, stats.total or 0,
        stats.tanks or 0, stats.healers or 0, stats.dps or 0,
        ((stats.wedged or 0) > 0)
            and string.format("|cffff5050Wedged %d|r", stats.wedged)
            or  "|cff66cc66Wedged 0|r",
        stats.intents_per_sec or 0,
        sparkString()))

    local used = stats.tick_used_ms or 0
    local budget = stats.tick_budget_ms or 33.3
    local pct = (budget > 0) and (used / budget) or 0
    local color = colorForTickPct(pct)
    local barW = math.floor(math.max(0, math.min(1, pct)) * 20)
    local bar = string.rep("█", barW) .. string.rep("░", 20 - barW)
    f.line2:SetText(string.format(
        "|cffffffffTick|r %s%.1f|r/%.1f ms  %s[%s]|r %d%% budget   %s",
        color, used, budget, color, bar, math.floor(pct * 100 + 0.5),
        stats.extra or ""))
end

------------------------------------------------------------------ build
function M.Build()
    if M.frame then return M.frame end

    local f = CreateFrame("Frame", "PlayerbotControlStats", UIParent, "BackdropTemplate")
    f:SetSize(PBC.DB.stats.w or 460, PBC.DB.stats.h or 56)
    f:SetPoint(PBC.DB.stats.point or "TOP",
               UIParent,
               PBC.DB.stats.relPoint or "TOP",
               PBC.DB.stats.x or 0,
               PBC.DB.stats.y or -8)
    f:SetMovable(true)
    f:SetClampedToScreen(true)
    f:EnableMouse(true)
    f:RegisterForDrag("LeftButton")
    f:SetScript("OnDragStart", function() f:StartMoving() end)
    f:SetScript("OnDragStop", function()
        f:StopMovingOrSizing()
        local p, _, rel, x, y = f:GetPoint()
        PBC.DB.stats.point    = p
        PBC.DB.stats.relPoint = rel
        PBC.DB.stats.x        = x
        PBC.DB.stats.y        = y
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

    f.line1 = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    f.line1:SetPoint("TOPLEFT", 8, -6)
    f.line1:SetPoint("RIGHT", -8, 0)
    f.line1:SetJustifyH("LEFT")
    f.line1:SetText("Bots …")

    f.line2 = f:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    f.line2:SetPoint("TOPLEFT", 8, -28)
    f.line2:SetPoint("RIGHT", -8, 0)
    f.line2:SetJustifyH("LEFT")
    f.line2:SetText("Tick …")

    -- Mini close button on hover
    f.closeBtn = CreateFrame("Button", nil, f, "UIPanelCloseButton")
    f.closeBtn:SetSize(20, 20)
    f.closeBtn:SetPoint("TOPRIGHT", 0, 0)
    f.closeBtn:SetScript("OnClick", function() M.Hide() end)
    f.closeBtn:SetAlpha(0.2)
    f:SetScript("OnEnter", function() f.closeBtn:SetAlpha(1) end)
    f:SetScript("OnLeave", function() f.closeBtn:SetAlpha(0.2) end)

    M.frame = f
    M.RegisterHandlers()
    return f
end

------------------------------------------------------------------ protocol handlers
function M.RegisterHandlers()
    PBC.Comms.RegisterHandler("STATS_RESP", function(fields, sender)
        local s = PBC.Comms.DecodeStats(fields)
        pushSample(s.intents_per_sec or 0)
        render(s)
        M.lastStats = s
    end)
end

------------------------------------------------------------------ tick (idle render)
function M.OnTick(elapsed)
    -- If we have no recent sample, re-render with stale to keep sparkline alive.
    if M.lastStats and M.frame and M.frame:IsShown() then
        -- No-op: STATS_RESP drives the redraw. We could decay here in future.
    end
end

------------------------------------------------------------------ show/hide
function M.Show()
    if not M.frame then M.Build() end
    M.frame:Show()
    PBC.DB.stats.shown = true
    PBC.Comms.StatsReq()
end

function M.Hide()
    if M.frame then M.frame:Hide() end
    PBC.DB.stats.shown = false
end

function M.Toggle()
    if M.frame and M.frame:IsShown() then M.Hide() else M.Show() end
end

-- vim: ts=4 sw=4 et
