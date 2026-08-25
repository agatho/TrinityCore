#include "BotAddressResolver.h"
#include "BotRegistry.h"
#include "BotAI.h"
#include "BotSnapshot.h"
#include "../Services.h"
#include "../Fleet/OwnerRegistry.h"
#include "../Threading/SnapshotPublisher.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "WorldSession.h"
#include "SharedDefines.h"
#include "Group.h"
#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace Playerbot {

namespace {

// Lower-case copy. The address parser is fully case-insensitive — owners
// shouldn't have to know whether `mage:` or `Mage:` works.
std::string ToLower(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return out;
}

// Trim leading/trailing whitespace.
std::string_view Trim(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))  s.remove_suffix(1);
    return s;
}

// Class names ↔ Class.dbc id mapping. Lower-case keys; supports both
// canonical names and a few common aliases (DK / DH).
ResolvedAddress::Kind ClassNameToKind(std::string const& token, uint8& out_cls)
{
    // Map of canonical lower-case → CLASS_*
    static std::unordered_map<std::string, uint8> const kMap = {
        {"warrior",     CLASS_WARRIOR},
        {"paladin",     CLASS_PALADIN},
        {"hunter",      CLASS_HUNTER},
        {"rogue",       CLASS_ROGUE},
        {"priest",      CLASS_PRIEST},
        {"deathknight", CLASS_DEATH_KNIGHT},
        {"dk",          CLASS_DEATH_KNIGHT},
        {"shaman",      CLASS_SHAMAN},
        {"mage",        CLASS_MAGE},
        {"warlock",     CLASS_WARLOCK},
        {"monk",        CLASS_MONK},
        {"druid",       CLASS_DRUID},
        {"demonhunter", CLASS_DEMON_HUNTER},
        {"dh",          CLASS_DEMON_HUNTER},
        {"evoker",      CLASS_EVOKER},
    };
    auto it = kMap.find(token);
    if (it == kMap.end()) return ResolvedAddress::Kind::Single;
    out_cls = it->second;
    return ResolvedAddress::Kind::Class;
}

// Spec name → ChrSpecialization.dbc id mapping. Supports the common short
// names players actually use ("frost" → Frost Mage AND Frost DK; the
// resolver returns ALL matching specs across classes, so commands like
// `frost: nova` apply to every frost-spec bot the owner has).
//
// Spec ids cribbed from infer_role in BotSnapshotBuilder.cpp +
// well-known retail spec ids. When a token matches multiple specs we
// store all of them (the resolver filters by membership).
struct SpecAlias { char const* token; std::initializer_list<uint32> specs; };
static SpecAlias const kSpecAliases[] = {
    // Tanks (covered by role: tank too, but spec name is more specific)
    {"protection",   {73, 66}},          // War Prot, Paladin Prot
    {"prot",         {73, 66}},
    {"vengeance",    {581}},
    {"guardian",     {104}},
    {"blood",        {250}},
    {"brewmaster",   {268}},
    {"brew",         {268}},
    // Healers
    {"holy",         {65, 257}},          // Pal Holy, Priest Holy
    {"discipline",   {256}},
    {"disc",         {256}},
    {"shadow",       {258}},
    {"restoration",  {105, 264}},          // Druid, Shaman
    {"resto",        {105, 264}},
    {"mistweaver",   {270}},
    {"mw",           {270}},
    {"preservation", {1468}},
    // Mage
    {"arcane",       {62}},
    {"fire",         {63}},
    {"frost",        {64, 251}},          // Frost Mage, Frost DK
    // Warrior
    {"arms",         {71}},
    {"fury",         {72}},
    // Hunter
    {"beastmastery", {253}},
    {"bm",           {253}},
    {"marksmanship", {254}},
    {"mm",           {254}},
    {"survival",     {255}},
    // Rogue
    {"assassination",{259}},
    {"assa",         {259}},
    {"outlaw",       {260}},
    {"subtlety",     {261}},
    {"sub",          {261}},
    // Death Knight
    {"unholy",       {252}},
    // Shaman
    {"elemental",    {262}},
    {"ele",          {262}},
    {"enhancement",  {263}},
    {"enh",          {263}},
    // Demon Hunter
    {"havoc",        {577}},
    // Druid
    {"balance",      {102}},
    {"feral",        {103}},
    // Monk
    {"windwalker",   {269}},
    {"ww",           {269}},
    // Paladin
    {"retribution",  {70}},
    {"ret",          {70}},
    // Warlock
    {"affliction",   {265}},
    {"affli",        {265}},
    {"demonology",   {266}},
    {"demo",         {266}},
    {"destruction",  {267}},
    {"destro",       {267}},
    // Evoker
    {"devastation",  {1467}},
    {"augmentation", {1473}},
    {"aug",          {1473}},
};

// For Class/Spec/Role kinds, returns a predicate over snapshots that
// decides whether a bot matches. Snapshots are pulled from the
// SnapshotPublisher to read the latest published cls/spec/my_role.
template <class Pred>
void CollectOwnedMatching(uint32 owner_account, Pred pred,
                          std::vector<Player*>& out)
{
    auto const owned = Services::Owners().BotsOwnedBy(owner_account);
    out.reserve(owned.size());
    for (BotId id : owned)
    {
        ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(id);
        Player* p = ObjectAccessor::FindConnectedPlayer(guid);
        if (!p) continue;
        // Pull the latest snapshot — snapshot fields are the source of
        // truth for cls/spec/role (the live Player can be mid-spec-swap).
        std::shared_ptr<BotSnapshot const> snap = Services::Snapshots().latest(id);
        if (!snap) continue;
        if (pred(*snap)) out.push_back(p);
    }
}

// Fallback collector: walks the sender's CURRENT GROUP and returns
// every V2-registered bot matching the predicate, regardless of
// ownership. Needed for the LFG case — the player queued solo via
// the dungeon-finder UI, TC matched them with auto-spawned (unowned)
// bots, and `;all run` would otherwise resolve to zero because
// CollectOwnedMatching only walks bots explicitly owned by the player.
// The downstream IsAuthorized() in BotCommandParser already permits
// group-member commands for unowned bots, so this matches existing
// authority semantics — players can command bots that share their
// group whether they own them or not.
template <class Pred>
void CollectGroupMatching(Player const* sender, Pred pred,
                          std::vector<Player*>& out)
{
    if (!sender) return;
    Group const* g = sender->GetGroup();
    if (!g) return;
    for (GroupReference const& itr : g->GetMembers())
    {
        Player* member = itr.GetSource();
        if (!member || member == sender) continue;
        const BotId mid = member->GetGUID().GetCounter();
        if (!Services::Registry().has(mid)) continue;
        std::shared_ptr<BotSnapshot const> snap = Services::Snapshots().latest(mid);
        if (!snap) continue;
        if (pred(*snap)) out.push_back(member);
    }
}

} // anonymous

ResolvedAddress BotAddressResolver::ParsePrefix(std::string_view whisper_text)
{
    ResolvedAddress out;
    auto trimmed = Trim(whisper_text);
    // Find the separator. Accept either ':' (the original "all:run" form)
    // OR the first whitespace ("all run") — the latter is what users
    // intuitively type from chat, and the previous colon-only parser
    // dropped `;all run` style commands onto the Single path. Whichever
    // separator comes first wins. No separator at all → Single.
    auto colon = trimmed.find(':');
    auto space = trimmed.find_first_of(" \t");
    std::string_view::size_type sep_pos = std::string_view::npos;
    if (colon != std::string_view::npos && space != std::string_view::npos)
        sep_pos = std::min(colon, space);
    else if (colon != std::string_view::npos)
        sep_pos = colon;
    else if (space != std::string_view::npos)
        sep_pos = space;

    if (sep_pos == std::string_view::npos)
    {
        out.kind = ResolvedAddress::Kind::Single;
        out.command = std::string(trimmed);
        return out;
    }
    auto prefix = std::string(Trim(trimmed.substr(0, sep_pos)));
    auto rest   = std::string(Trim(trimmed.substr(sep_pos + 1)));

    // The space-separated parse must only kick in for KNOWN prefix
    // tokens — otherwise an innocuous command like "fly 5" would have
    // "fly" mis-parsed as a prefix. Defer the prefix-validity check to
    // the switch below: if no token matches, fall back to Single with
    // the original full text as command.
    auto pl_check = ToLower(prefix);
    bool const is_known_prefix =
        pl_check == "all" || pl_check == "squad" || pl_check == "here" ||
        pl_check == "tank" || pl_check == "tanks" ||
        pl_check == "heal" || pl_check == "heals" ||
        pl_check == "healer" || pl_check == "healers" ||
        pl_check == "dps" ||
        pl_check == "marked" || pl_check == "mark";
    // For space-separated, also accept class / spec / bot-name as the
    // prefix only if it matches one of those tables — check after the
    // switch by capturing whether the original separator was a space.
    bool const sep_was_space = (sep_pos == space);
    if (sep_was_space && !is_known_prefix)
    {
        // Try class / spec alias / name. If any matches, accept as a
        // prefix. Otherwise treat the whole text as a Single command
        // (don't eat "fly 5" by accident).
        uint8 dummy_cls = 0;
        bool const is_class = ClassNameToKind(pl_check, dummy_cls) ==
                              ResolvedAddress::Kind::Class;
        bool is_spec = false;
        for (SpecAlias const& a : kSpecAliases)
            if (pl_check == a.token) { is_spec = true; break; }
        // Name-prefix path is ambiguous in space-form (any first word
        // would match), so require the colon for name addressing.
        if (!is_class && !is_spec)
        {
            out.kind = ResolvedAddress::Kind::Single;
            out.command = std::string(trimmed);
            return out;
        }
    }
    out.command = std::move(rest);

    auto pl = ToLower(prefix);
    if (pl == "all")    { out.kind = ResolvedAddress::Kind::All;    return out; }
    if (pl == "squad")  { out.kind = ResolvedAddress::Kind::Squad;  return out; }
    if (pl == "here")   { out.kind = ResolvedAddress::Kind::Here;   return out; }
    if (pl == "tank" || pl == "tanks")
                        { out.kind = ResolvedAddress::Kind::Role_Tank; return out; }
    if (pl == "heal" || pl == "heals" || pl == "healer" || pl == "healers")
                        { out.kind = ResolvedAddress::Kind::Role_Healer; return out; }
    if (pl == "dps")    { out.kind = ResolvedAddress::Kind::Role_Dps; return out; }
    if (pl == "marked" || pl == "mark")
                        { out.kind = ResolvedAddress::Kind::Marked; return out; }

    // Class name?
    {
        uint8 cls = 0;
        if (ClassNameToKind(pl, cls) == ResolvedAddress::Kind::Class)
        {
            out.kind = ResolvedAddress::Kind::Class;
            out.filter = pl;
            return out;
        }
    }

    // Spec alias?
    for (SpecAlias const& a : kSpecAliases)
    {
        if (pl == a.token)
        {
            out.kind = ResolvedAddress::Kind::Spec;
            out.filter = pl;
            return out;
        }
    }

    // Otherwise — treat as a name. Restore original-case prefix as
    // the filter so the name match is case-insensitive but the
    // diagnostic display preserves the player's typing.
    out.kind = ResolvedAddress::Kind::Name;
    out.filter = std::move(prefix);
    return out;
}

ResolvedAddress BotAddressResolver::Resolve(
    Player const* sender,
    Player*       primary,
    std::string_view whisper_text)
{
    ResolvedAddress out = ParsePrefix(whisper_text);
    if (!sender)
    {
        if (primary) out.bots.push_back(primary);
        return out;
    }
    const uint32 sender_account =
        sender->GetSession() ? sender->GetSession()->GetAccountId() : 0;

    if (out.kind == ResolvedAddress::Kind::Single)
    {
        if (primary) out.bots.push_back(primary);
        return out;
    }

    if (out.kind == ResolvedAddress::Kind::All)
    {
        CollectOwnedMatching(sender_account,
            [](BotSnapshot const&) { return true; },
            out.bots);
        // LFG / shared-group fallback when the sender owns no bots.
        if (out.bots.empty())
            CollectGroupMatching(sender,
                [](BotSnapshot const&) { return true; },
                out.bots);
        return out;
    }

    if (out.kind == ResolvedAddress::Kind::Squad)
    {
        // Owned + currently in sender's group.
        Group const* g = sender->GetGroup();
        CollectOwnedMatching(sender_account,
            [g](BotSnapshot const& s)
            {
                if (!g) return false;
                return g->IsMember(s.guid);
            },
            out.bots);
        // Same fallback as 'all' but already group-scoped — any V2 bot
        // in the group counts as the player's squad in the unowned case.
        if (out.bots.empty())
            CollectGroupMatching(sender,
                [](BotSnapshot const&) { return true; },
                out.bots);
        return out;
    }

    if (out.kind == ResolvedAddress::Kind::Here)
    {
        // Owned + on sender's map within 60y.
        const uint32 sender_map = sender->GetMapId();
        const float  sx = sender->GetPositionX();
        const float  sy = sender->GetPositionY();
        constexpr float kRadiusSq = 60.f * 60.f;
        CollectOwnedMatching(sender_account,
            [sender_map, sx, sy, kRadiusSq](BotSnapshot const& s)
            {
                if (s.position.map_id != sender_map) return false;
                const float dx = s.position.x - sx, dy = s.position.y - sy;
                return dx*dx + dy*dy <= kRadiusSq;
            },
            out.bots);
        return out;
    }

    if (out.kind == ResolvedAddress::Kind::Role_Tank ||
        out.kind == ResolvedAddress::Kind::Role_Healer ||
        out.kind == ResolvedAddress::Kind::Role_Dps)
    {
        const Role want =
            out.kind == ResolvedAddress::Kind::Role_Tank   ? Role::Tank :
            out.kind == ResolvedAddress::Kind::Role_Healer ? Role::Healer :
                                                              Role::Dps;
        CollectOwnedMatching(sender_account,
            [want](BotSnapshot const& s) { return s.group.my_role == want; },
            out.bots);
        if (out.bots.empty())
            CollectGroupMatching(sender,
                [want](BotSnapshot const& s) { return s.group.my_role == want; },
                out.bots);
        return out;
    }

    if (out.kind == ResolvedAddress::Kind::Class)
    {
        uint8 want_cls = 0;
        ClassNameToKind(out.filter, want_cls);
        CollectOwnedMatching(sender_account,
            [want_cls](BotSnapshot const& s) { return s.identity.cls == want_cls; },
            out.bots);
        return out;
    }

    if (out.kind == ResolvedAddress::Kind::Spec)
    {
        // Look up the spec id set for this alias and match against
        // bot's current spec.
        std::vector<uint32> wanted;
        for (SpecAlias const& a : kSpecAliases)
            if (out.filter == a.token)
                for (uint32 sid : a.specs)
                    wanted.push_back(sid);
        CollectOwnedMatching(sender_account,
            [&wanted](BotSnapshot const& s)
            {
                for (uint32 sid : wanted) if (s.identity.spec == sid) return true;
                return false;
            },
            out.bots);
        return out;
    }

    if (out.kind == ResolvedAddress::Kind::Marked)
    {
        // Owned + carries any active raid marker. Snapshot doesn't yet
        // expose markers, so this is a future-fillable filter — for now
        // resolve to empty and let callers know via empty bots list.
        // (Marker wiring deferred to Phase E /mark.)
        return out;
    }

    if (out.kind == ResolvedAddress::Kind::Name)
    {
        const std::string filter_lower = ToLower(out.filter);
        CollectOwnedMatching(sender_account,
            [&filter_lower](BotSnapshot const& s)
            {
                return ToLower(s.identity.name) == filter_lower;
            },
            out.bots);
        return out;
    }

    return out;
}

} // namespace Playerbot
