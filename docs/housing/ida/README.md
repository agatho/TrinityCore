# Housing IDA Reference Material

Generated 2026-05-12 from WoW retail client build **12.0.5.67186**.

## Files

### `CMSG_SENDERS_67186.md`

Definitive list of all client→server CMSG opcodes the build-67186 client sends
in housing-related ranges (0x2E–0x39). Each entry maps:
- Opcode value → sender function address → identifying name (if recovered)

This is the **ground truth** for verifying our `Opcodes.h` mappings. Any opcode
in our `Opcodes.h` marked `TC-CUSTOM` that does NOT appear in this list is
provably dead code — the retail client never sends it in build 67186.

## How it was generated

Dynamic analysis via IDA Pro MCP using `py_eval`:

1. Found 10 dispatcher class anchors via function-name search (`CliHousing*System`,
   `CliNeighborhood*System`, `CliHouseExterior*System`, `CliHouseInterior*System`).
2. For each anchor, scanned a ±0x1500 byte window for functions calling
   `sub_7FF75EE9FF10` (`WritePacketHeader(packet, opcode)`).
3. For each call site, walked backward up to 0x80 bytes looking for the
   preceding `mov edx, imm32` instruction — that's the opcode argument.
4. Filtered to housing range (0x2A0000 ≤ opcode ≤ 0x3FFFFF).

Total: 80 unique CMSG opcodes mapped across 10 dispatcher namespaces.

## Build-drift notes

Opcode values shift between WoW client builds. Examples observed in sniff
captures spanning 12.0.1.65940 → 12.0.5.67186:

| Concept | Older builds | Build 67186 |
|---|---|---|
| `CMSG_HOUSING_SVCS_NEIGHBORHOOD_RESERVE_PLOT` | `0x330006` | `0x330007` |
| `CMSG_HOUSING_SVCS_TELEPORT_TO_PLOT` | `0x330017` | `0x330019` |

When validating opcodes against sniff captures, **always filter by build version**.
Mixing observations across multiple builds will produce false "unmapped" entries.

## Sister tools

The methodology scripts live in the development worktree at
`I:\TrinityCore\housing\sniff_verify\`:

- `verify_opcodes.py` — cross-references all SMSG opcodes in sniffs against `Opcodes.h`
- `verify_cmsgs.py` — same but for CMSGs
- `probe_unmapped.py` — wire-body sampler for individual opcodes
- `probe_unmapped_cmsgs.py` — wire-body + sequencing-context for CMSGs

These aren't shipped in the repo (depend on `C:\sniff\` capture set) but can be
adapted by anyone with packet captures of matching builds.

## Speculative-opcode retirement history

Reference for the May 2026 cleanup pass:

- Commit `6e7b84dbc1` (2026-05-11): retired 22 speculative SMSGs + remapped 2 to
  real opcodes (`SMSG_HOUSING_UPDATE_HOUSE_INFO → 0x550004`,
  `SMSG_HOUSING_CATALOG_STATE_SYNC → 0x56000E`) + 4 behavioral fixes.

- Commit `82315a6439` (2026-05-12): retired 7 paired TC-CUSTOM CMSGs + handlers
  whose SMSG responses were retired the day before.

The IDA opcode extraction methodology in this doc was used to identify which
candidates were genuinely fake vs which were build-drift artifacts.

## Important caveat

Per IDA verification, `CMSG_HOUSING_SYSTEM_UPDATE_HOUSE_INFO = 0x350004` is
**fake** — no sender in build 67186. The SMSG remap to `0x550004` in commit
`6e7b84dbc1` is technically correct (the SMSG opcode IS real) but the only
emit-site is a handler that never executes (because no CMSG triggers it). House
naming/description is not a wired feature in retail 12.0.5.67186 — the
`Housing::SetHouseNameDescription` server-side method exists but has no
protocol path.
