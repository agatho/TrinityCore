# SMSG_XP_GAIN_ABORTED — aborted XP notification (WoW 12.0.7.68275)

Branch: `feature/xp-gain-aborted` (base `560165c0a6`)

Sent when a kill/credit was eligible for XP but the award was aborted (the killer is at max level).
The opcode (`0x42006C`) was `STATUS_UNHANDLED` with no packet.

## Wire (recovered no-guess from the client reader `sub_7FF72908C710`)

The 0x42-group deserializer reads, in order:

```
PackedGuid Victim          // WorldPacket_ReadPackedObjectGuid -> obj+32
uint32     Amount          // Stream_ReadUInt32               -> obj+48
uint32     Unused1         // Stream_ReadUInt32               -> obj+52
uint32     Unused2         // Stream_ReadUInt32               -> obj+56
```

(The message class's vtable is `off_7FF72C4B3100`; its GetOpcode thunk at RVA 0x5EC700 returns
`0x0042006C`.)

### Field evidence (rated-BG capture, 116 packets)

Every packet was `PackedGuid + uint32 + uint32(0) + uint32(0)`, e.g. sizes 28/31 (PackedGuid is
variable-length). The first `uint32` varied with plausible XP magnitudes (0x20 = 32, 0xD18 = 3352); the
trailing two `uint32` were **0 in every packet**. All captured senders were max-level players, matching
"XP that would have been gained, but you are max level." The two trailing fields' exact roles are
unconfirmed, so they are sent honestly as `0` (named `Unused1`/`Unused2`) rather than guessed.

## Server implementation

Emitted from `Player::GiveXP` at the max-level abort — the point where XP was computed for an eligible,
loot-tagged kill but the award returns early because the player is at max level:

```cpp
if (IsMaxLevel())
{
    WorldPackets::Character::XPGainAborted xpGainAborted;
    xpGainAborted.Victim = victim ? victim->GetGUID() : ObjectGuid::Empty;
    xpGainAborted.Amount = xp;          // the XP that would have been granted
    SendDirectMessage(xpGainAborted.Write());
    return;
}
```

This sits after the existing loot-recipient guard, so only kills the player actually tapped trigger it —
matching the capture. (`PLAYER_FLAGS_NO_XP_GAIN` is a second plausible abort trigger but was not present
in the capture, so it is left alone to avoid unobserved sends.)

## Files

- `src/server/game/Server/Protocol/Opcodes.cpp` — `SMSG_XP_GAIN_ABORTED` → `STATUS_NEVER`
- `src/server/game/Server/Packets/CharacterPackets.{h,cpp}` — `XPGainAborted` packet + `Write()`
- `src/server/game/Entities/Player/Player.cpp` — emit in `GiveXP` at the max-level abort
