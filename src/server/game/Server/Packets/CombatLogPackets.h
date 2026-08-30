/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITYCORE_COMBAT_LOG_PACKETS_H
#define TRINITYCORE_COMBAT_LOG_PACKETS_H

#include "CombatLogPacketsCommon.h"
#include "Optional.h"
#include "Position.h"
#include <string>
#include <vector>

struct SpellLogEffect;

namespace WorldPackets
{
    namespace CombatLog
    {
        struct CombatWorldTextViewerInfo
        {
            ObjectGuid ViewerGUID;
            Optional<uint8> ColorType;
            Optional<uint8> ScaleType;
        };

        class SpellNonMeleeDamageLog final : public CombatLogServerPacket
        {
        public:
            explicit SpellNonMeleeDamageLog() : CombatLogServerPacket(SMSG_SPELL_NON_MELEE_DAMAGE_LOG, 60) { }

            WorldPacket const* Write() override;

            ObjectGuid Me;
            ObjectGuid CasterGUID;
            ObjectGuid CastID;
            int32 SpellID = 0;
            Spells::SpellCastVisual Visual;
            int32 Damage = 0;
            int32 OriginalDamage = 0;
            int32 Overkill = -1;
            uint8 SchoolMask = 0;
            int32 ShieldBlock = 0;
            int32 ReflectingSpellID = 0;
            int32 Resisted = 0;
            bool Periodic = false;
            int32 Absorbed = 0;
            int32 Flags = 0;
            // Optional<SpellNonMeleeDamageLogDebugInfo> DebugInfo;
            Optional<Spells::ContentTuningParams> ContentTuning;
            std::vector<CombatWorldTextViewerInfo> WorldTextViewers;
            std::vector<Spells::SpellSupportInfo> Supporters;
        };

        class EnvironmentalDamageLog final : public CombatLogServerPacket
        {
        public:
            explicit EnvironmentalDamageLog() : CombatLogServerPacket(SMSG_ENVIRONMENTAL_DAMAGE_LOG, 23) { }

            WorldPacket const* Write() override;

            ObjectGuid Victim;
            uint8 Type = 0; ///< @see enum EnviromentalDamage
            int32 Amount = 0;
            int32 Resisted = 0;
            int32 Absorbed = 0;
        };

        class SpellExecuteLog final : public CombatLogServerPacket
        {
        public:
            explicit SpellExecuteLog() : CombatLogServerPacket(SMSG_SPELL_EXECUTE_LOG, 16 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid Caster;
            int32 SpellID = 0;
            std::vector<SpellLogEffect> const* Effects = nullptr;
        };

        class SpellHealLog final : public CombatLogServerPacket
        {
        public:
            explicit SpellHealLog() : CombatLogServerPacket(SMSG_SPELL_HEAL_LOG, 16 + 16 + 4 * 5 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid CasterGUID;
            ObjectGuid TargetGUID;
            int32 SpellID       = 0;
            int32 Health        = 0;
            int32 OriginalHeal  = 0;
            int32 OverHeal      = 0;
            int32 Absorbed      = 0;
            bool Crit           = false;
            Optional<float> CritRollMade;
            Optional<float> CritRollNeeded;
            Optional<Spells::ContentTuningParams> ContentTuning;
            std::vector<Spells::SpellSupportInfo> Supporters;
        };

        struct PeriodicalAuraLogEffectDebugInfo
        {
            float CritRollMade = 0.0f;
            float CritRollNeeded = 0.0f;
        };

        struct PeriodicAuraLogEffect
        {
            int32 Effect              = 0;
            int32 Amount              = 0;
            int32 OriginalDamage      = 0;
            int32 OverHealOrKill      = 0;
            int32 SchoolMaskOrPower   = 0;
            int32 AbsorbedOrAmplitude = 0;
            int32 Resisted            = 0;
            bool Crit                 = false;
            Optional<PeriodicalAuraLogEffectDebugInfo> DebugInfo;
            Optional<Spells::ContentTuningParams> ContentTuning;
            std::vector<Spells::SpellSupportInfo> Supporters;
        };

        class SpellPeriodicAuraLog final : public CombatLogServerPacket
        {
        public:
            explicit SpellPeriodicAuraLog() : CombatLogServerPacket(SMSG_SPELL_PERIODIC_AURA_LOG, 16 + 16 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid TargetGUID;
            ObjectGuid CasterGUID;
            int32 SpellID = 0;
            std::vector<PeriodicAuraLogEffect> Effects;
        };

        class SpellInterruptLog final : public ServerPacket
        {
        public:
            explicit SpellInterruptLog() : ServerPacket(SMSG_SPELL_INTERRUPT_LOG, 16 + 16 + 4 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid Caster;
            ObjectGuid Victim;
            int32 InterruptedSpellID = 0;
            int32 SpellID = 0;
            bool HideFromCombatLog = false;
        };

        struct SpellDispellData
        {
            int32 SpellID = 0;
            bool Harmful = false;
            Optional<int32> Rolled;
            Optional<int32> Needed;
        };

        class SpellDispellLog : public ServerPacket
        {
        public:
            explicit SpellDispellLog() : ServerPacket(SMSG_SPELL_DISPELL_LOG, 1 + 16 + 16 + 4 + 4 + 20) { }

            WorldPacket const* Write() override;

            std::vector<SpellDispellData> DispellData;
            ObjectGuid CasterGUID;
            ObjectGuid TargetGUID;
            int32 DispelledBySpellID = 0;
            bool IsBreak = false;
            bool IsSteal = false;
        };

        class SpellEnergizeLog final : public CombatLogServerPacket
        {
        public:
            explicit SpellEnergizeLog() : CombatLogServerPacket(SMSG_SPELL_ENERGIZE_LOG, 16 + 16 + 4 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid CasterGUID;
            ObjectGuid TargetGUID;
            int32 SpellID = 0;
            int8 Type = 0;
            int32 Amount = 0;
            int32 OverEnergize = 0;
        };

        class TC_GAME_API SpellInstakillLog final : public ServerPacket
        {
        public:
            explicit SpellInstakillLog() : ServerPacket(SMSG_SPELL_INSTAKILL_LOG, 16 + 16 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid Target;
            ObjectGuid Caster;
            int32 SpellID = 0;
        };

        struct SpellLogMissDebug
        {
            float HitRoll = 0.0f;
            float HitRollNeeded = 0.0f;
        };

        struct SpellLogMissEntry
        {
            SpellLogMissEntry(ObjectGuid const& victim, uint8 missReason) : Victim(victim), MissReason(missReason) { }

            ObjectGuid Victim;
            uint8 MissReason = 0;
            Optional<SpellLogMissDebug> Debug;
        };

        class SpellMissLog final : public ServerPacket
        {
        public:
            explicit SpellMissLog() : ServerPacket(SMSG_SPELL_MISS_LOG) { }

            WorldPacket const* Write() override;

            int32 SpellID = 0;
            ObjectGuid Caster;
            std::vector<SpellLogMissEntry> Entries;
            bool HideFromCombatLog = false;
        };

        class ProcResist final : public ServerPacket
        {
        public:
            explicit ProcResist() : ServerPacket(SMSG_PROC_RESIST, 16 + 4 + 4 + 4 + 16) { }

            WorldPacket const* Write() override;

            ObjectGuid Caster;
            ObjectGuid Target;
            int32 SpellID = 0;
            Optional<float> Rolled;
            Optional<float> Needed;
        };

        class SpellOrDamageImmune final : public ServerPacket
        {
        public:
            explicit SpellOrDamageImmune() : ServerPacket(SMSG_SPELL_OR_DAMAGE_IMMUNE, 16 + 1 + 4 + 16) { }

            WorldPacket const* Write() override;

            ObjectGuid CasterGUID;
            ObjectGuid VictimGUID;
            uint32 SpellID = 0;
            bool IsPeriodic = false;
        };

        class SpellDamageShield final : public CombatLogServerPacket
        {
        public:
            explicit SpellDamageShield() : CombatLogServerPacket(SMSG_SPELL_DAMAGE_SHIELD, 4 + 16 + 4 + 4 + 16 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid Attacker;
            ObjectGuid Defender;
            int32 SpellID = 0;
            int32 TotalDamage = 0;
            int32 OriginalDamage = 0;
            int32 OverKill = 0;
            int32 SchoolMask = 0;
            int32 LogAbsorbed = 0;
        };

        struct SubDamage
        {
            int32 SchoolMask = 0;
            float FDamage = 0.0f; // Float damage (Most of the time equals to Damage)
            int32 Damage = 0;
            int32 Absorbed = 0;
            int32 Resisted = 0;
        };

        struct HitInfoData
        {
            uint32 ArmorReduction = 0;
            float CritRollNeeded = 0.0f;
            float CombatRoll = 0.0f;
            float MissChance = 0.0f;
            float DodgeChance = 0.0f;
            float ParryChance = 0.0f;
            float BlockChance = 0.0f;
            float GlanceChance = 0.0f;
            float CrushChance = 0.0f;
            float MinDamage = 0.0f;
            float MaxDamage = 0.0f;
            uint32 SinceLastSwing = 0;
        };

        class AttackerStateUpdate final : public CombatLogServerPacket
        {
        public:
            explicit AttackerStateUpdate() : CombatLogServerPacket(SMSG_ATTACKER_STATE_UPDATE, 70) { }

            WorldPacket const* Write() override;

            uint32 Flags = 0; // Flags
            ObjectGuid AttackerGUID;
            ObjectGuid VictimGUID;
            int32 Damage = 0;
            int32 OriginalDamage = 0;
            int32 OverDamage = -1; // (damage - health) or -1 if unit is still alive
            Optional<SubDamage> SubDmg;
            uint8 VictimState = 0;
            uint32 AttackerState = 0;
            uint32 MeleeSpellID = 0;
            int32 BlockAmount = 0;
            int32 RageGained = 0;
            HitInfoData HitInfo;
            float BlockRoll = 0.0f;
            Spells::ContentTuningParams ContentTuning;
        };

        // Victim side twin of SMSG_ATTACKER_STATE_UPDATE, sent for the same swing whenever that swing connects.
        // The 12.0.7 client (68275) deserializes it at 0x7FF7290FB9C0 and handles it at 0x7FF72A87ABC0: it emits
        // the COMBAT_LOG_EVENT_UNFILTERED subevent SWING_DAMAGE_LANDED (combat log subevent name table
        // 0x7FF72CC63EF0, index 50) plus one SWING_DAMAGE_LANDED_SUPPORT (index 62) per Supporters entry,
        // attaches the advanced combat logging fields taken from LogData to the *victim*, and corrects the
        // victim's client side health. SMSG_ATTACKER_STATE_UPDATE can deliver none of that: its LogData
        // describes the attacker and its handler emits SWING_DAMAGE / SWING_MISSED (indices 1 / 2) instead.
        // The body is byte for byte an AttackerStateUpdate prefixed with the Supporters list; verified against
        // 11548 captured 12.0.7 packets (see AttackSwingLandedLog::Write for what retail never populates).
        class AttackSwingLandedLog final : public CombatLogServerPacket
        {
        public:
            explicit AttackSwingLandedLog() : CombatLogServerPacket(SMSG_ATTACK_SWING_LANDED_LOG, 70) { }

            WorldPacket const* Write() override;

            std::vector<Spells::SpellSupportInfo> Supporters;
            uint32 Flags = 0; // Flags
            ObjectGuid AttackerGUID;
            ObjectGuid VictimGUID;
            int32 Damage = 0;
            int32 OriginalDamage = 0;
            int32 OverDamage = -1; // (damage - health) or -1 if unit is still alive
            Optional<SubDamage> SubDmg;
            uint8 VictimState = 0;
            uint32 AttackerState = 0;
            uint32 MeleeSpellID = 0;
            int32 BlockAmount = 0;
            HitInfoData HitInfo;
            Spells::ContentTuningParams ContentTuning;
        };

        class SpellAbsorbLog final : public CombatLogServerPacket
        {
        public:
            explicit SpellAbsorbLog() : CombatLogServerPacket(SMSG_SPELL_ABSORB_LOG, 100) { }

            WorldPacket const* Write() override;

            ObjectGuid Attacker;
            ObjectGuid Victim;
            ObjectGuid Caster;
            int32 AbsorbedSpellID = 0;
            int32 AbsorbSpellID = 0;
            int32 Absorbed = 0;
            int32 OriginalDamage = 0;
            bool Crit = false;
            std::vector<Spells::SpellSupportInfo> Supporters;
        };

        class SpellHealAbsorbLog final : public CombatLogServerPacket
        {
        public:
            explicit SpellHealAbsorbLog() : CombatLogServerPacket(SMSG_SPELL_HEAL_ABSORB_LOG, 100) { }

            WorldPacket const* Write() override;

            ObjectGuid Healer;
            ObjectGuid Target;
            ObjectGuid AbsorbCaster;
            int32 AbsorbSpellID = 0;
            int32 AbsorbedSpellID = 0;
            int32 Absorbed = 0;
            int32 OriginalHeal = 0;
            Optional<Spells::ContentTuningParams> ContentTuning;
        };

        // ---------------------------------------------------------------------
        // Familie 0x67 - Kampflog-Dateisystem, Debug- und Anzeigekanaele
        // Client-Build 12.1.0.69382, alle RVA gegen ImageBase.
        // ---------------------------------------------------------------------

        // SMSG_SETUP_COMBAT_LOG_FILE_FLUSH (0x67000F) - feste 16 B.
        // Der Server steuert, wie der Client sein WoWCombatLog.txt schreibt. Konsument
        // 0x1E0B270 setzt fuer jedes Feld einen Vorgabewert ein, wenn der Wert <= 0 ist:
        //   +0x00 int32 MaxFileSize   <= 0 -> 0x40000   -> Global 0x6807200
        //   +0x04 float Threshold     <= 0 -> 0.8f      -> Global 0x6807204
        //   +0x08 float FlushInterval <= 0 -> 1.0f      -> Global 0x6807208
        //   +0x0C int32 MaxSeconds    <= 0 -> 300       -> Global 0x680720C
        // Referenzpaket (17 Pakete, alle 16 B): 00000400 cdcc4c3f 0000803f 2c010000
        // = { 262144, 0.8f, 1.0f, 300 } -- exakt die vier Vorgabewerte. Kein Lua-Ereignis.
        class SetupCombatLogFileFlush final : public ServerPacket
        {
        public:
            explicit SetupCombatLogFileFlush() : ServerPacket(SMSG_SETUP_COMBAT_LOG_FILE_FLUSH, 4 + 4 + 4 + 4) { }

            WorldPacket const* Write() override;

            int32 MaxFileSize = 0;
            float Threshold = 0.0f;
            float FlushInterval = 0.0f;
            int32 MaxSeconds = 0;
        };

        // SMSG_FLUSH_COMBAT_LOG_FILE (0x670010) - leere Nachricht.
        // Konsument 0x1E0B2E0: `if (!Global 0x6808FF4) return;` -- laeuft kein /combatlog,
        // wird die Nachricht wirkungslos und ohne Rueckmeldung verworfen. Kein Lua-Ereignis.
        // Draht: 86 Pakete, alle 0 B.
        class FlushCombatLogFile final : public ServerPacket
        {
        public:
            explicit FlushCombatLogFile() : ServerPacket(SMSG_FLUSH_COMBAT_LOG_FILE, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        // SMSG_DISPLAY_WORLD_TEXT_ON_TARGET (0x670055) - Bildschirmtext ueber einem Ziel.
        // Beleg: Leser 0x68B530, vtable 0x3BF2EB0 (Slot 3 @0x68B750 schreibt den Opcode
        // selbst), Konsument 0x1E20E60 (der einzige PTR-Hook der Familie).
        //   RGUID 0x68B552 | R32 0x68B55E/0x68B56A/0x68B576 (movss -> Position)
        //   R32 0x68B58B (ArgsCount, Resize 0x68B59B) | R32 0x68B5BD je Arg
        //   Bit-Gruppe genau 16 Bit MSB-first: bits<12> TextLength (0x68B5ED/0x68B5FF,
        //   Zusammenbau 0x68B613), HasColor 0x68B626, HasStyle 0x68B64E, HasAngle 0x68B671,
        //   Unique 0x68B69A -- danach implizites Flush.
        //   RBYTES 0x68B6B3 (Text, KEIN NUL am Draht), dann optional
        //   R8 0x68B6D3 Color, R8 0x68B6F9 Style, R32 0x68B71F Angle (movss).
        // Textpuffer: Inline-Region +0x3C..+0xBF8 = 3004 B, Konsument kappt bei 0xBB9 = 3001,
        // ceil(log2(3001)) = 12 -- passt zur gemessenen Bitbreite.
        // SICHERHEIT: der Leser prueft die 12-Bit-Laenge NICHT und schreibt in einen Puffer auf
        // dem Dispatcher-Stack (rbp-0x80, 0x751DC2). Werte ueber 3003 ueberschreiben den
        // Client-Stack. Der Server klemmt deshalb hart auf 3000 (MaxTextLength).
        // Kein Lua-Ereignis, keine Dauer am Draht (die kommt aus 0x1D054B0).
        class DisplayWorldTextOnTarget final : public ServerPacket
        {
        public:
            static constexpr std::size_t MaxTextLength = 3000;

            explicit DisplayWorldTextOnTarget() : ServerPacket(SMSG_DISPLAY_WORLD_TEXT_ON_TARGET, 18 + 12 + 4 + 2) { }

            WorldPacket const* Write() override;

            ObjectGuid TargetGUID;
            TaggedPosition<Position::XYZ> Position;         ///< Fallback-Anker, wenn TargetGUID nicht aufloesbar
            std::vector<int32> Args;                        ///< Format-Substitutionsargumente
            std::string Text;
            bool Unique = false;                            ///< Anzeigetyp 8: ersetzt den vorherigen Eintrag
            Optional<uint8> Color;                          ///< Index in die Farbtabelle 0x1D04100, nur 0..5 behandelt
            Optional<uint8> Style;                          ///< Vorgabe 3, wenn nicht gesetzt
            Optional<float> Angle;                          ///< in GRAD; Client rechnet rad = Angle * pi/180 - pi
        };

        // SMSG_DAMAGE_CALC_LOG (0x670056) - die Rechenschritte der Schadensformel.
        // Beleg: Top-Level-Leser inline im Dispatcher, 0x751E3A..0x751F4C (case 6750294):
        //   RGUID 0x751E71 Attacker | RGUID 0x751E7D Victim | R32 0x751E92 SourceType
        //   R32 0x751EB0 SpellID | R32 0x751EDD StepCount (Resize 0x68EB50, Stride 0x138)
        //   ... Elemente ... | Bit 0x751F1D/0x751F29 -- LETZTES Feld, NACH dem Array.
        // Element-Leser 0x68B760 (575 B, nicht 298 -- die lokale Funktionsgroesse luegt):
        //   bits<5> VarNameLen (R8 0x68B78E, shr 3 @0x68B7A7) | bits<1> VariantSel (shr 2 & 1
        //   @0x68B7B3) | 2 Restbits verworfen | R32 0x68B7C3 CalcStepType | RBYTES 0x68B7DE
        //   VarName -- der uint32 steht VOR den Stringbytes.
        //   Variante A (VariantSel == 0): 3x movss (0x68B8FC/0x68B91F/0x68B942) + R32 0x68B95D
        //   SupportCount + Elemente ueber 0x71AA30 (JamSpellSupportInfo, 32 B).
        //   Variante B (VariantSel == 1): bits<9> TextLen ((b1<<1)|(b2>>7), 0x68B820/0x68B836)
        //   + bits<1> IsSupport ((b2>>6)&1, 0x68B860) | 6 Restbits verworfen | RBYTES 0x68B86F.
        // +0x28 ist eine Union, die Zweige duerfen NICHT hintereinander gelesen werden.
        // Feldnamen aus dem Formatstring-Block 0x3D51380..0x3D51520 (Log-Kategorie
        // "Damage Calculator") und Blizzards Reflexionsdeskriptor 0x389B0E0 fuer
        // JamSpellSupportInfo. Kein Lua-Ereignis - reines Client-Debug-Log.
        class DamageCalcLog final : public ServerPacket
        {
        public:
            static constexpr std::size_t MaxVarNameLength = 31;     ///< bits<5>
            static constexpr std::size_t MaxTextLength = 256;       ///< bits<9>, Puffer 257 B

            struct Step
            {
                std::string VarName;                        ///< "val" laesst den Client "NOT VALIDATED" drucken
                int32 CalcStepType = 0;                     ///< Tabelle 0x43C89B0, 0..60
                // Variante A - numerisch (gilt, solange Text leer ist)
                float Value = 0.0f;
                float Total = 0.0f;
                float Unknown3 = 0.0f;                      ///< UNVERIFIED: vom Konsumenten nicht gedruckt
                std::vector<Spells::SpellSupportInfo> Supports;
                // Variante B - Text. Die Anwesenheit von Text IST der Diskriminator
                // (Draht: bits<1> VariantSel), die beiden Zweige sind eine Union.
                Optional<std::string> Text;
                bool IsSupport = false;                     ///< 0 -> "DMG INFO", 1 -> "DMG SUPPORT INFO"
            };

            explicit DamageCalcLog() : ServerPacket(SMSG_DAMAGE_CALC_LOG, 18 + 18 + 4 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid Attacker;
            ObjectGuid Victim;
            int32 SourceType = 0;                           ///< UNVERIFIED: nur als "sourceType=%d" belegt, ohne Wertetabelle
            int32 SpellID = 0;
            std::vector<Step> Steps;
            bool Flag = false;                              ///< letztes Feld, vom Konsumenten nicht gelesen
        };
    }
}

#endif // TRINITYCORE_COMBAT_LOG_PACKETS_H
