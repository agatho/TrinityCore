--[[============================================================================
 BotCommands.lua  —  bottom-screen command bar with autocomplete + history.

 Look:
   ┌──────────────────────────────────────────────────────────────┐
   │ > all follow Areon                                          │
   ├──────────────────────────────────────────────────────────────┤
   │ all follow Areon   ·   all stop   ·   tank engage_focus     │  ← suggestions
   └──────────────────────────────────────────────────────────────┘

 Keybindings (while editbox focused):
   TAB / Shift-TAB    cycle suggestions
   ↑ / ↓              walk history (most recent first)
   ENTER              dispatch
   ESC                hide

 Command grammar:
   <addr>            <verb>           <args…>
   all|squad|tank    follow|stop|     bot names / coordinates / role
   |healer|dps       engage|engage_   (depending on verb)
   |<botName>        focus|hold|squad
                     |role|mark|login
                     |logout|promote
                     |whisper|pause|resume

 Server is the source of truth on addressing rules. We *don't* attempt to
 dispatch verbs locally — every command goes out as a CMD frame.
============================================================================]]

local PBC = _G.PlayerbotControl
local M = {}
PBC.BotCommands = M

M.frame   = nil
M.editbox = nil

------------------------------------------------------------------ vocab
M.ADDRESSES = { "all", "squad", "tank", "healer", "dps",
                "warrior", "paladin", "hunter", "rogue", "priest",
                "death-knight", "shaman", "mage", "warlock", "monk",
                "druid", "demon-hunter", "evoker" }

M.VERBS = {
    "follow", "stop", "engage", "engage_focus", "hold", "squad",
    "role", "mark", "login", "logout", "promote", "whisper",
    "pause", "resume", "form", "spread", "tight", "ghost_res",
    "use_hearth", "mount", "dismount", "loot_roll",
    "bg_queue", "bg_leave", "lfg_queue", "lfg_leave",
}

-- Verb-specific arg hints to surface in placeholder text.
M.VERB_HELP = {
    follow       = "<target?>",
    engage_focus = "<target>",
    squad        = "<botName…>",
    role         = "tank|healer|dps",
    mark         = "skull|cross|star|circle|moon|diamond|square|triangle",
    form         = "tight|spread|line|wedge",
    whisper      = "<botName> <text…>",
    login        = "(uses addr: all|role|class|name — offline bots)",
    logout       = "(uses addr: all|role|class|name — headless bots)",
    bg_queue     = "wsg|ab|av|eots|sota|ioc|bg|tp|kotmogu|dg|ss|tk|ashran|wint|seething",
    lfg_queue    = "<dungeonName?>",
}

------------------------------------------------------------------ suggestion logic
local function inferRoleNames(role)
    local out = {}
    if PBC.DB and PBC.DB.knownBots then
        for name, r in pairs(PBC.DB.knownBots) do
            if (r or ""):upper() == role:upper() then out[#out + 1] = name end
        end
    end
    return out
end

local function botNames()
    if PBC.BotRoster and PBC.BotRoster.GetNames then
        return PBC.BotRoster.GetNames()
    end
    return {}
end

local function prefixMatch(list, pfx)
    pfx = (pfx or ""):lower()
    local out = {}
    for _, item in ipairs(list) do
        if item:lower():sub(1, #pfx) == pfx then out[#out + 1] = item end
    end
    return out
end

local function buildCandidates(text, cursorPos)
    -- Tokenize the text up to cursor; the last partial token is what we
    -- autocomplete on.
    local pre = text:sub(1, cursorPos)
    local toks = {}
    for t in pre:gmatch("%S+") do toks[#toks + 1] = t end
    local partial = ""
    if pre:sub(-1) ~= " " and #toks > 0 then
        partial = toks[#toks]
        toks[#toks] = nil
    end

    local pos = #toks + 1
    if pos == 1 then
        return prefixMatch(M.ADDRESSES, partial), 1
    elseif pos == 2 then
        return prefixMatch(M.VERBS, partial), 2
    else
        -- For arg positions, suggest bot names primarily.
        local addr = toks[1]
        local verb = toks[2]
        if verb == "role" then
            return prefixMatch({ "tank", "healer", "dps" }, partial), pos
        elseif verb == "mark" then
            return prefixMatch({ "skull","cross","star","circle","moon","diamond","square","triangle" }, partial), pos
        elseif verb == "form" then
            return prefixMatch({ "tight","spread","line","wedge" }, partial), pos
        elseif verb == "bg_queue" then
            return prefixMatch({ "wsg","ab","av","eots","sota","ioc","bg","tp","kotmogu","dg","ss","tk","ashran","wint","seething" }, partial), pos
        else
            local pool = botNames()
            if #pool == 0 and (addr == "tank" or addr == "healer" or addr == "dps") then
                pool = inferRoleNames(addr)
            end
            return prefixMatch(pool, partial), pos
        end
    end
end

------------------------------------------------------------------ history walk
local histIdx = 0

local function pushHistory(s)
    local h = PBC.DB.commands.history
    -- de-dupe: don't add same as most-recent
    if h[#h] == s then histIdx = 0; return end
    h[#h + 1] = s
    while #h > (PBC.DB.commands.maxHistory or 40) do
        table.remove(h, 1)
    end
    histIdx = 0
end

local function historyAt(idx)
    local h = PBC.DB.commands.history
    if idx <= 0 or idx > #h then return nil end
    return h[#h - idx + 1] -- 1 = most recent
end

------------------------------------------------------------------ dispatch
local function dispatch(text)
    local toks = {}
    for t in text:gmatch("%S+") do toks[#toks + 1] = t end
    if #toks < 2 then
        PBC.Warn("usage: <addr> <verb> [args…]")
        return
    end
    local addr = toks[1]
    local verb = toks[2]
    -- login/logout aren't CMD verbs (an offline bot has no session for a
    -- CMD to reach) — they ride dedicated LOGIN_REQ / LOGOUT_REQ frames.
    if verb == "login" or verb == "logout" then
        if verb == "login" then PBC.RequestLogin(addr)
        else PBC.RequestLogout(addr) end
        pushHistory(text)
        return
    end
    local args = {}
    for i = 3, #toks do args[#args + 1] = toks[i] end
    PBC.Comms.Cmd(addr, verb, args)
    PBC.Print("→ %s %s%s%s", addr, verb,
        (#args > 0) and " " or "",
        table.concat(args, " "))
    pushHistory(text)
end

------------------------------------------------------------------ frame
local SUG_MAX = 6

local function rebuildSuggestions()
    local f   = M.frame
    if not f then return end
    local eb  = M.editbox
    local txt = eb:GetText() or ""
    local cur = eb.GetUTF8CursorPosition and eb:GetUTF8CursorPosition() or eb:GetCursorPosition() or #txt
    local cands, _pos = buildCandidates(txt, cur)
    f.suggestionList = cands
    f.suggestionIdx  = 0

    for i = 1, SUG_MAX do
        local btn = f.sugBtns[i]
        local label = cands[i]
        if label then
            btn:SetText(label)
            btn:Show()
        else
            btn:Hide()
        end
    end

    -- Update placeholder hint for verbs.
    local toks = {}
    for t in txt:gmatch("%S+") do toks[#toks + 1] = t end
    if #toks >= 2 then
        local hint = M.VERB_HELP[toks[2]] or ""
        f.hint:SetText(hint)
    else
        f.hint:SetText("addr verb args…")
    end
end

local function applySuggestion(label)
    local eb = M.editbox
    if not eb or not label then return end
    local txt = eb:GetText() or ""
    -- Replace the trailing partial token with the suggestion + trailing space.
    local pre, _ = txt:match("^(.-)(%S*)$")
    eb:SetText(pre .. label .. " ")
    eb:SetCursorPosition(#eb:GetText())
    rebuildSuggestions()
end

local function cycleSuggestion(dir)
    local f = M.frame
    if not f or not f.suggestionList or #f.suggestionList == 0 then return end
    f.suggestionIdx = ((f.suggestionIdx or 0) + dir - 1) % #f.suggestionList + 1
    applySuggestion(f.suggestionList[f.suggestionIdx])
end

local function styleEditbox(eb)
    eb:SetAutoFocus(false)
    eb:SetFontObject("ChatFontNormal")
    eb:SetTextInsets(8, 8, 4, 4)
    eb:SetMaxLetters(220)
    eb:SetScript("OnTextChanged", function() rebuildSuggestions() end)
    eb:SetScript("OnTabPressed", function()
        if IsShiftKeyDown() then cycleSuggestion(-1) else cycleSuggestion(1) end
    end)
    eb:SetScript("OnEnterPressed", function(self)
        local txt = self:GetText() or ""
        if #txt > 0 then dispatch(txt); self:SetText("") end
        self:ClearFocus()
    end)
    eb:SetScript("OnEscapePressed", function(self) self:ClearFocus(); M.Hide() end)
    -- Retail WoW EditBox doesn't expose OnUpPressed/OnDownPressed (those
    -- are Classic-era scripts and throw "Doesn't have a script" on retail).
    -- We observe UP/DOWN via OnKeyDown, which fires for every key while
    -- the box has focus. Printable characters still flow into the text
    -- normally — OnKeyDown is observation-only — so typing isn't broken.
    eb:SetScript("OnKeyDown", function(self, key)
        if key == "UP" then
            histIdx = histIdx + 1
            local h = historyAt(histIdx)
            if h then self:SetText(h); self:SetCursorPosition(#h)
            else histIdx = histIdx - 1 end
        elseif key == "DOWN" then
            if histIdx <= 1 then histIdx = 0; self:SetText(""); return end
            histIdx = histIdx - 1
            local h = historyAt(histIdx)
            if h then self:SetText(h); self:SetCursorPosition(#h) end
        end
    end)
end

function M.Build()
    if M.frame then return M.frame end

    local f = CreateFrame("Frame", "PlayerbotControlCommands", UIParent, "BackdropTemplate")
    f:SetSize(PBC.DB.commands.w or 520, PBC.DB.commands.h or 36)
    f:SetPoint(PBC.DB.commands.point or "BOTTOM",
               UIParent,
               PBC.DB.commands.relPoint or "BOTTOM",
               PBC.DB.commands.x or 0,
               PBC.DB.commands.y or 200)
    f:SetMovable(true)
    f:SetClampedToScreen(true)
    f:EnableMouse(true)
    f:RegisterForDrag("LeftButton")
    f:SetScript("OnDragStart", function() f:StartMoving() end)
    f:SetScript("OnDragStop", function()
        f:StopMovingOrSizing()
        local p, _, rel, x, y = f:GetPoint()
        PBC.DB.commands.point    = p
        PBC.DB.commands.relPoint = rel
        PBC.DB.commands.x        = x
        PBC.DB.commands.y        = y
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

    -- Prompt > label
    f.prompt = f:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    f.prompt:SetPoint("LEFT", 8, 0)
    f.prompt:SetText("|cff66ccff>|r")

    -- Editbox
    local eb = CreateFrame("EditBox", nil, f)
    eb:SetPoint("LEFT", f.prompt, "RIGHT", 4, 0)
    eb:SetPoint("RIGHT", -8, 0)
    eb:SetHeight(20)
    styleEditbox(eb)
    M.editbox = eb

    -- Hint text overlay (right-aligned, faded)
    f.hint = f:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    f.hint:SetPoint("RIGHT", -10, 0)
    f.hint:SetText("addr verb args…")

    -- Suggestion strip below the bar.
    f.sug = CreateFrame("Frame", nil, f)
    f.sug:SetPoint("TOPLEFT", f, "BOTTOMLEFT", 0, -2)
    f.sug:SetPoint("TOPRIGHT", f, "BOTTOMRIGHT", 0, -2)
    f.sug:SetHeight(22)
    local sb = f.sug:CreateTexture(nil, "BACKGROUND")
    sb:SetAllPoints()
    sb:SetColorTexture(0.08, 0.09, 0.13, 0.85)

    f.sugBtns = {}
    for i = 1, SUG_MAX do
        local b = CreateFrame("Button", nil, f.sug)
        b:SetSize(80, 18)
        b:SetPoint("LEFT", 4 + (i - 1) * 82, 0)
        b:SetNormalFontObject("GameFontHighlightSmall")
        b:SetText("…")
        b:SetScript("OnClick", function(self) applySuggestion(self:GetText()); eb:SetFocus() end)
        local bg = b:CreateTexture(nil, "BACKGROUND")
        bg:SetAllPoints()
        bg:SetColorTexture(0.15, 0.18, 0.25, 0.7)
        b:Hide()
        f.sugBtns[i] = b
    end

    M.frame = f
    return f
end

------------------------------------------------------------------ public API
function M.Show()
    if not M.frame then M.Build() end
    M.frame:Show()
    M.editbox:SetFocus()
    PBC.DB.commands.shown = true
    rebuildSuggestions()
end

function M.Hide()
    if M.frame then M.frame:Hide() end
    if M.editbox then M.editbox:ClearFocus() end
    PBC.DB.commands.shown = false
end

function M.Toggle()
    if M.frame and M.frame:IsShown() then M.Hide() else M.Show() end
end

function M.SetText(s)
    if not M.editbox then return end
    M.editbox:SetText(s or "")
    if s then M.editbox:SetCursorPosition(#s) end
end

-- vim: ts=4 sw=4 et
