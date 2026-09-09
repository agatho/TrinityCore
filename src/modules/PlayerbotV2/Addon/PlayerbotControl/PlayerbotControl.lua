--[[============================================================================
 PlayerbotControl.lua  —  main entry, slash commands, event registrations.

 Responsibilities:
   * Initialize SavedVariables (PlayerbotControlDB, PlayerbotControlCharDB)
   * Register CHAT_MSG_ADDON, ADDON_LOADED, PLAYER_LOGIN events
   * Register addon prefix
   * Wire /pbc, /playerbot, /playerbotcontrol slash commands
   * Create a single OnUpdate driver that fans out to subsystems (avoids the
     classic addon mistake of N frame-driven scripts each doing GetTime work)
   * Own a global anchor frame PBC.MainFrame that holds the UI children

 Slash commands (all aliases mean the same):
   /pbc                       toggle the Roster window
   /pbc roster                show/focus roster
   /pbc commands              show command bar
   /pbc debug [botName]       open debug panel (focused on bot if given)
   /pbc stats                 show fleet dashboard
   /pbc follow [target]       address all|squad|… follow target (default me)
   /pbc stop [target]         halt
   /pbc engage                engage owner's current target
   /pbc squad <name…>         set named squad members
   /pbc role <name> <role>    promote tank/healer/dps
   /pbc reset                 wipe layout DB (positions/scales)
   /pbc debug-comms           toggle wire logging
   /pbc ping                  ping the server (latency check)
============================================================================]]

local _ADDON, NS = ...
local PBC = _G.PlayerbotControl

------------------------------------------------------------------ DB defaults
local DB_DEFAULTS = {
    debug = false,
    statsPollWhileHidden = false,
    roster = {
        point  = "CENTER", relTo = "UIParent", relPoint = "CENTER",
        x = -300, y = 0, w = 280, h = 480, scale = 1.0, shown = false,
        showOffline = false, sortBy = "role",
    },
    commands = {
        point = "BOTTOM", relTo = "UIParent", relPoint = "BOTTOM",
        x = 0, y = 200, w = 520, h = 36, scale = 1.0, shown = false,
        history = {}, maxHistory = 40,
    },
    debugPanel = {
        point = "CENTER", relTo = "UIParent", relPoint = "CENTER",
        x = 200, y = 0, w = 460, h = 520, scale = 1.0, shown = false,
        activeTab = "Snapshot",
    },
    stats = {
        point = "TOP", relTo = "UIParent", relPoint = "TOP",
        x = 0, y = -8, w = 460, h = 56, scale = 1.0, shown = false,
    },
    palette = {
        playerBlue  = { 0.30, 0.55, 1.00 },
        allyGreen   = { 0.30, 0.85, 0.35 },
        enemyRed    = { 0.90, 0.25, 0.25 },
        offlineGray = { 0.50, 0.50, 0.50 },
        warnYellow  = { 1.00, 0.85, 0.20 },
        bg1         = { 0.04, 0.04, 0.06, 0.92 },
        bg2         = { 0.10, 0.10, 0.14, 0.92 },
        border      = { 0.25, 0.25, 0.30, 1.00 },
    },
    knownBots = {}, -- name → last-known role, persisted for autocomplete
}

local CHAR_DB_DEFAULTS = {
    focusBotGuid = nil,
    pinnedBots   = {},
}

------------------------------------------------------------------ utilities
local function deepDefaults(target, defaults)
    for k, v in pairs(defaults) do
        if type(v) == "table" then
            if type(target[k]) ~= "table" then target[k] = {} end
            deepDefaults(target[k], v)
        elseif target[k] == nil then
            target[k] = v
        end
    end
end

local function trim(s) return (s:gsub("^%s+", ""):gsub("%s+$", "")) end

function PBC.Print(fmt, ...)
    local msg = (... == nil) and tostring(fmt) or string.format(fmt, ...)
    DEFAULT_CHAT_FRAME:AddMessage("|cff66ccffPBC|r " .. msg)
end

function PBC.Warn(fmt, ...)
    local msg = (... == nil) and tostring(fmt) or string.format(fmt, ...)
    DEFAULT_CHAT_FRAME:AddMessage("|cffffcc33PBC|r " .. msg)
end

function PBC.Err(fmt, ...)
    local msg = (... == nil) and tostring(fmt) or string.format(fmt, ...)
    DEFAULT_CHAT_FRAME:AddMessage("|cffff5050PBC|r " .. msg)
end

------------------------------------------------------------------ session control fan-out
-- /pbc login|logout, the toolbar Login/Logout buttons and the command bar
-- all funnel through these. LOGIN_REQ / LOGOUT_REQ are per-character
-- frames, so multi-bot addresses (all / role / class) are expanded
-- client-side against the last ROSTER_RESP. Anything that isn't a known
-- filter token is treated as a single character name and sent verbatim —
-- the server validates ownership either way. "squad" can't be resolved
-- locally (the squad set lives server-side), so it isn't offered here.
local SESSION_FILTERS = {
    all = true, tank = true, healer = true, dps = true,
    warrior = true, paladin = true, hunter = true, rogue = true,
    priest = true, deathknight = true, shaman = true, mage = true,
    warlock = true, monk = true, druid = true, demonhunter = true,
    evoker = true,
}

local function matchSessionFilter(rec, token)
    if token == "all" then return true end
    if token == "tank" or token == "healer" or token == "dps" then
        return (rec.role or ""):lower() == token
    end
    -- Class token: server sends e.g. "DEATHKNIGHT"; user may type
    -- "death-knight" (command-bar vocab) — both normalize the same way.
    return (rec.class or ""):lower():gsub("[%-_]", "") == token
end

-- who: "all" | role | class token | single character name.
function PBC.RequestLogin(who)
    who = who or "all"
    local token = who:lower():gsub("[%-_]", "")
    if not SESSION_FILTERS[token] then
        PBC.Comms.LoginReq(who)               -- single name, original case
        PBC.Print("→ login %s", who)
        if PBC.BotRoster then PBC.BotRoster.RequestRefresh(1.5) end
        return
    end
    local roster = (PBC.BotRoster and PBC.BotRoster.GetRoster()) or {}
    local sent = 0
    for _, rec in ipairs(roster) do
        if rec.name and not rec.online and matchSessionFilter(rec, token) then
            PBC.Comms.LoginReq(rec.name)
            sent = sent + 1
        end
    end
    if sent > 0 then
        PBC.Print("→ login requested for %d offline bot%s",
                  sent, sent == 1 and "" or "s")
        if PBC.BotRoster then PBC.BotRoster.RequestRefresh(2.0) end
    else
        PBC.Warn("no offline bots match '%s' — open /pbc roster so the " ..
                 "list is fetched, or give a character name", who)
    end
end

function PBC.RequestLogout(who)
    who = who or "all"
    local token = who:lower():gsub("[%-_]", "")
    if not SESSION_FILTERS[token] then
        PBC.Comms.LogoutReq(who)              -- single name, original case
        PBC.Print("→ logout %s", who)
        if PBC.BotRoster then PBC.BotRoster.RequestRefresh(1.5) end
        return
    end
    local roster = (PBC.BotRoster and PBC.BotRoster.GetRoster()) or {}
    local sent = 0
    for _, rec in ipairs(roster) do
        -- Only headless sessions — the server refuses real clients anyway,
        -- but filtering here keeps the chat free of pointless warn toasts
        -- (e.g. the owner's own self-AI character).
        if rec.name and rec.online and rec.headless
            and matchSessionFilter(rec, token) then
            PBC.Comms.LogoutReq(rec.name)
            sent = sent + 1
        end
    end
    if sent > 0 then
        PBC.Print("→ logout requested for %d bot%s",
                  sent, sent == 1 and "" or "s")
        if PBC.BotRoster then PBC.BotRoster.RequestRefresh(2.0) end
    else
        PBC.Warn("no online bots match '%s' — open /pbc roster so the " ..
                 "list is fetched, or give a character name", who)
    end
end

------------------------------------------------------------------ main anchor frame
local MainFrame = CreateFrame("Frame", "PlayerbotControlMain", UIParent)
MainFrame:Hide()
PBC.MainFrame = MainFrame

------------------------------------------------------------------ OnUpdate fan-out
local accum = 0
local function OnUpdate(_, elapsed)
    accum = accum + elapsed
    if PBC.Comms then PBC.Comms.OnUpdate(elapsed) end
    -- 5Hz panel refresh for things that aren't event-driven (bar interp etc).
    if accum >= 0.2 then
        if PBC.BotRoster and PBC.BotRoster.OnTick then
            PBC.BotRoster.OnTick(accum)
        end
        if PBC.BotStats and PBC.BotStats.OnTick then
            PBC.BotStats.OnTick(accum)
        end
        if PBC.BotDebug and PBC.BotDebug.OnTick then
            PBC.BotDebug.OnTick(accum)
        end
        if PBC.BotAlts and PBC.BotAlts.OnTick then
            PBC.BotAlts.OnTick(accum)
        end
        accum = 0
    end
end

------------------------------------------------------------------ stats poll
local statsTimer = 0
local function PollStatsIfVisible(elapsed)
    statsTimer = statsTimer + elapsed
    if statsTimer < PBC.STATS_INTERVAL then return end
    statsTimer = 0
    if not PBC.Comms then return end
    local anyVisible =
        (PBC.BotStats and PBC.BotStats.frame and PBC.BotStats.frame:IsShown()) or
        (PBC.BotRoster and PBC.BotRoster.frame and PBC.BotRoster.frame:IsShown()) or
        (PBC.BotDebug and PBC.BotDebug.frame and PBC.BotDebug.frame:IsShown())
    if anyVisible or (PBC.DB and PBC.DB.statsPollWhileHidden) then
        PBC.Comms.StatsReq()
    end
end

local pollFrame = CreateFrame("Frame")
pollFrame:SetScript("OnUpdate", function(_, e) PollStatsIfVisible(e) end)

MainFrame:SetScript("OnUpdate", OnUpdate)

------------------------------------------------------------------ event glue
local eventFrame = CreateFrame("Frame", "PlayerbotControlEvents")
eventFrame:RegisterEvent("ADDON_LOADED")
eventFrame:RegisterEvent("PLAYER_LOGIN")
eventFrame:RegisterEvent("CHAT_MSG_ADDON")
eventFrame:RegisterEvent("PLAYER_LOGOUT")
eventFrame:SetScript("OnEvent", function(_, event, ...)
    if event == "ADDON_LOADED" then
        local name = ...
        if name == "PlayerbotControl" or name == _ADDON then
            PlayerbotControlDB     = PlayerbotControlDB     or {}
            PlayerbotControlCharDB = PlayerbotControlCharDB or {}
            deepDefaults(PlayerbotControlDB,     DB_DEFAULTS)
            deepDefaults(PlayerbotControlCharDB, CHAR_DB_DEFAULTS)
            PBC.DB     = PlayerbotControlDB
            PBC.CharDB = PlayerbotControlCharDB
            if C_ChatInfo and C_ChatInfo.RegisterAddonMessagePrefix then
                C_ChatInfo.RegisterAddonMessagePrefix(PBC.PREFIX)
            elseif RegisterAddonMessagePrefix then
                RegisterAddonMessagePrefix(PBC.PREFIX)
            end
        end
    elseif event == "PLAYER_LOGIN" then
        -- Server-side hook fires inside Player::WhisperAddon, which is only
        -- invoked when the whisper target resolves to an actual connected
        -- player. The fake "PBCFLEET" name never resolves and the hook
        -- never fires, so we re-point the fleet target at the player's own
        -- name (self-whispers DO route through WhisperAddon and trigger
        -- the PlayerScript::OnChat hook the server uses to intercept PBC
        -- frames). Override happens here, post-login, because UnitName
        -- isn't valid until PLAYER_LOGIN fires.
        local me = UnitName("player")
        if me and me ~= "" then PBC.FLEET_TARGET = me end
        PBC.Print("Wave G online — /pbc to begin. Server prefix: %s", PBC.PREFIX)
        if PBC.BotRoster   and PBC.BotRoster.Build   then PBC.BotRoster.Build()   end
        if PBC.BotCommands and PBC.BotCommands.Build then PBC.BotCommands.Build() end
        if PBC.BotDebug    and PBC.BotDebug.Build    then PBC.BotDebug.Build()    end
        if PBC.BotStats    and PBC.BotStats.Build    then PBC.BotStats.Build()    end
        if PBC.BotToolbar  and PBC.BotToolbar.Build  then PBC.BotToolbar.Build()  end
        if PBC.BotAlts     and PBC.BotAlts.Build     then PBC.BotAlts.Build()     end
        -- Restore previously shown frames.
        if PBC.DB.roster.shown   and PBC.BotRoster   then PBC.BotRoster.Show()   end
        if PBC.DB.commands.shown and PBC.BotCommands then PBC.BotCommands.Show() end
        if PBC.DB.stats.shown    and PBC.BotStats    then PBC.BotStats.Show()    end
        if PBC.DB.debugPanel.shown and PBC.BotDebug  then PBC.BotDebug.Show()    end
        if PBC.DB.toolbar and PBC.DB.toolbar.shown and PBC.BotToolbar then
            PBC.BotToolbar.Show()
        end
        MainFrame:Show()
        -- Kick the first stats fetch right away.
        if PBC.Comms then PBC.Comms.StatsReq() end
    elseif event == "CHAT_MSG_ADDON" then
        if PBC.Comms then PBC.Comms.OnAddonMsg(...) end
    elseif event == "PLAYER_LOGOUT" then
        -- Persist visibility flags so layouts come back on next session.
        if PBC.DB then
            if PBC.BotRoster   and PBC.BotRoster.frame   then PBC.DB.roster.shown    = PBC.BotRoster.frame:IsShown()   end
            if PBC.BotCommands and PBC.BotCommands.frame then PBC.DB.commands.shown  = PBC.BotCommands.frame:IsShown() end
            if PBC.BotDebug    and PBC.BotDebug.frame    then PBC.DB.debugPanel.shown= PBC.BotDebug.frame:IsShown()    end
            if PBC.BotStats    and PBC.BotStats.frame    then PBC.DB.stats.shown     = PBC.BotStats.frame:IsShown()    end
        end
    end
end)

------------------------------------------------------------------ slash commands
local function usage()
    PBC.Print("commands:")
    PBC.Print("  /pbc | /pbc roster      — toggle bot roster")
    PBC.Print("  /pbc commands           — toggle command bar")
    PBC.Print("  /pbc toolbar            — toggle group quick-action bar")
    PBC.Print("  /pbc alts               — open Spawn-an-Alt picker")
    PBC.Print("  /pbc summon <alt>       — spawn one alt directly")
    PBC.Print("  /pbc self on|off|status — drive your OWN char with the AI")
    PBC.Print("  /pbc debug [name]       — open debug panel")
    PBC.Print("  /pbc stats              — toggle fleet dashboard")
    PBC.Print("  /pbc follow [target]    — squad follow")
    PBC.Print("  /pbc stop               — halt")
    PBC.Print("  /pbc engage             — focus engage")
    PBC.Print("  /pbc squad name1 name2…")
    PBC.Print("  /pbc role <bot> <role>")
    PBC.Print("  /pbc login [who]        — headless-login offline bots (all|role|class|name)")
    PBC.Print("  /pbc logout [who]       — log out headless bot sessions")
    PBC.Print("  /pbc reset              — wipe layout")
    PBC.Print("  /pbc debug-comms        — wire logging")
    PBC.Print("  /pbc ping               — latency check")
end

local function tokens(s)
    local out = {}
    for tok in s:gmatch("%S+") do out[#out + 1] = tok end
    return out
end

local function slashHandler(msg)
    msg = trim(msg or "")
    if msg == "" then
        if PBC.BotRoster then PBC.BotRoster.Toggle() end
        return
    end
    local toks = tokens(msg)
    local cmd  = (toks[1] or ""):lower()

    if cmd == "roster" then
        PBC.BotRoster.Toggle()
    elseif cmd == "commands" or cmd == "cmd" then
        PBC.BotCommands.Toggle()
    elseif cmd == "toolbar" or cmd == "orders" or cmd == "bar" then
        if PBC.BotToolbar then PBC.BotToolbar.Toggle() end
    elseif cmd == "alts" or cmd == "spawn" then
        if PBC.BotAlts then PBC.BotAlts.Show() end
    elseif cmd == "summon" then
        local who = toks[2]
        if not who then PBC.Warn("usage: /pbc summon <altname>"); return end
        PBC.Comms.Summon(who)
        PBC.Print("→ summon %s", who)
    elseif cmd == "self" then
        local mode = (toks[2] or "status"):lower()
        if mode ~= "on" and mode ~= "off" and mode ~= "status" then
            PBC.Warn("usage: /pbc self on|off|status"); return
        end
        PBC.Comms.Self(mode)
        PBC.Print("→ self %s", mode)
    elseif cmd == "debug" then
        PBC.BotDebug.Show()
        if toks[2] then PBC.BotDebug.FocusByName(toks[2]) end
    elseif cmd == "stats" then
        PBC.BotStats.Toggle()
    elseif cmd == "follow" then
        local target = toks[2] or UnitName("player")
        PBC.Comms.Cmd("all", "follow", { target })
        PBC.Print("→ all follow %s", target)
    elseif cmd == "stop" or cmd == "halt" then
        PBC.Comms.Cmd("all", "stop", {})
        PBC.Print("→ all stop")
    elseif cmd == "engage" then
        local t = UnitName("target")
        if not t then PBC.Warn("no target"); return end
        PBC.Comms.Cmd("all", "engage_focus", { t })
        PBC.Print("→ all engage %s", t)
    elseif cmd == "squad" then
        local args = {}
        for i = 2, #toks do args[#args + 1] = toks[i] end
        if #args == 0 then PBC.Warn("squad needs at least one bot name"); return end
        PBC.Comms.Cmd("squad", "set", args)
        PBC.Print("→ squad set: %s", table.concat(args, ", "))
    elseif cmd == "role" then
        local who, role = toks[2], toks[3]
        if not who or not role then PBC.Warn("usage: /pbc role <bot> tank|healer|dps"); return end
        PBC.Comms.Cmd(who, "role", { role:lower() })
        PBC.Print("→ %s role=%s", who, role)
    elseif cmd == "promote" then
        local who = toks[2]
        if not who then PBC.Warn("usage: /pbc promote <bot>"); return end
        PBC.Comms.Cmd(who, "promote", {})
    elseif cmd == "reset" then
        PlayerbotControlDB = {}
        deepDefaults(PlayerbotControlDB, DB_DEFAULTS)
        PBC.DB = PlayerbotControlDB
        PBC.Print("layout reset — /reload to fully reapply")
    elseif cmd == "debug-comms" or cmd == "debug_comms" then
        PBC.DB.debug = not PBC.DB.debug
        PBC.Print("wire logging: %s", PBC.DB.debug and "ON" or "OFF")
    elseif cmd == "ping" then
        local t0 = GetTime()
        PBC.Comms.Send(PBC.FLEET_TARGET, "WHISPER", "PING",
            { tostring(t0 * 1000) },
            { expectReply = true,
              onReply  = function() PBC.Print("pong (%.0f ms)", (GetTime() - t0) * 1000) end,
              onTimeout= function() PBC.Warn("ping timeout") end })
    elseif cmd == "pause" then
        local who = toks[2] or "all"
        PBC.Comms.Cmd(who, "pause", {})
        PBC.Print("→ %s paused", who)
    elseif cmd == "resume" then
        local who = toks[2] or "all"
        PBC.Comms.Cmd(who, "resume", {})
        PBC.Print("→ %s resumed", who)
    elseif cmd == "logout" then
        -- Dedicated LOGOUT_REQ frames (the CMD "logout" verb never existed
        -- server-side, and CMD can't reach a bot without a live session).
        PBC.RequestLogout(toks[2] or "all")
    elseif cmd == "login" then
        PBC.RequestLogin(toks[2] or "all")
    elseif cmd == "help" or cmd == "?" then
        usage()
    else
        -- Fall through: treat unknown verb as a direct CMD with "all" address.
        local args = {}
        for i = 2, #toks do args[#args + 1] = toks[i] end
        PBC.Comms.Cmd("all", cmd, args)
        PBC.Print("→ all %s %s", cmd, table.concat(args, " "))
    end
end

SLASH_PLAYERBOTCONTROL1 = "/pbc"
SLASH_PLAYERBOTCONTROL2 = "/playerbotcontrol"
SLASH_PLAYERBOTCONTROL3 = "/playerbot"
SlashCmdList["PLAYERBOTCONTROL"] = slashHandler

------------------------------------------------------------------ generic protocol handlers
-- These are installed before the UI modules so even un-built panels see data.
local function installCoreHandlers()
    PBC.Comms.RegisterHandler("PONG", function(fields, sender)
        local t0 = tonumber(fields[1]) or 0
        local lat = GetTime() * 1000 - t0
        if PBC.DB and PBC.DB.debug then
            PBC.Print("PONG %.0fms from %s", lat, sender or "?")
        end
    end)

    PBC.Comms.RegisterHandler("EVENT_PUSH", function(fields, sender, seq)
        local severity = fields[1] or "info"
        local guid     = fields[2] or "?"
        local evt      = fields[3] or "?"
        local detail   = fields[4] or ""
        -- Always ACK so the server can drop from its retry spool.
        PBC.Comms.Ack(seq)
        local fn
        if severity == "error" then fn = PBC.Err
        elseif severity == "warn" then fn = PBC.Warn
        else fn = PBC.Print end
        fn("event %s [%s] %s", evt, guid, detail)
        if PBC.BotDebug and PBC.BotDebug.PushEvent then
            PBC.BotDebug.PushEvent(guid, evt, detail, severity)
        end
    end)
end

-- Wait for ADDON_LOADED to populate DB before installing.
local installFrame = CreateFrame("Frame")
installFrame:RegisterEvent("PLAYER_LOGIN")
installFrame:SetScript("OnEvent", function() installCoreHandlers(); installFrame:UnregisterAllEvents() end)

------------------------------------------------------------------ public reset
function PBC.ResetLayout()
    PlayerbotControlDB = {}
    deepDefaults(PlayerbotControlDB, DB_DEFAULTS)
    PBC.DB = PlayerbotControlDB
end

-- Expose a generic registration so other addons can listen in.
function PBC.OnEvent(fn, owner)
    PBC.Comms.AddEventListener(fn, owner or "external")
end

-- vim: ts=4 sw=4 et
