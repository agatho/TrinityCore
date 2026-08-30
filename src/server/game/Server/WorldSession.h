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

#ifndef __WORLDSESSION_H
#define __WORLDSESSION_H

#include "Common.h"
#include "BattlePayMgr.h"
#include "AsyncCallbackProcessor.h"
#include "AuthDefines.h"
#include "ClientBuildInfo.h"
#include "DatabaseEnvFwd.h"
#include "Duration.h"
#include "IteratorPair.h"
#include "LockedQueue.h"
#include "ObjectGuid.h"
#include "Opcodes.h"
#include "Optional.h"
#include "RaceMask.h"
#include "SharedDefines.h"
#include <boost/circular_buffer_fwd.hpp>
#include <array>
#include <atomic>
#include <bitset>
#include <map>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

class BlackMarketEntry;
class ChatCautionMgr;
class CollectionMgr;
class Creature;
class Group;
class InstanceLock;
class Item;
class LoginQueryHolder;
class ByteBuffer;
class MessageBuffer;
class HousingPlayerHouseEntity;
class HousingNeighborhoodMirrorEntity;
class Player;
class Unit;
class WorldPacket;
class WorldSession;
class WorldSocket;
struct AuctionPosting;
struct BlackMarketTemplate;
struct ChrCustomizationReqEntry;
struct DeclinedName;
struct ItemTemplate;
struct Loot;
struct MovementInfo;
struct Petition;
struct Position;
enum class AuctionCommand : int8;
enum class AuctionResult : int8;
enum class PlayerInteractionType : int32;
enum InventoryResult : uint8;
enum class StableResult : uint8;
enum class TabardVendorType : int32;

namespace Battlenet
{
class Account;
}

namespace BattlePets
{
    class BattlePetMgr;
}

namespace lfg
{
    struct LfgJoinResultData;
    struct LfgPlayerBoot;
    struct LfgProposal;
    struct LfgQueueStatusData;
    struct LfgPlayerRewardData;
    struct LfgRoleCheck;
    struct LfgReadyCheck;
    struct LfgUpdateData;
    enum LfgTeleportResult : uint8;
    enum LfgSlotInvalidReason : uint32;
}

namespace rbac
{
class RBACData;
}

namespace UF
{
    struct ChrCustomizationChoice;
}

namespace WorldPackets
{
    namespace Achievement
    {
        class GuildSetFocusedAchievement;
        class GuildGetAchievementMembers;
    }

    namespace AdventureJournal
    {
        class AdventureJournalOpenQuest;
        class AdventureJournalUpdateSuggestions;
        class EncounterJournalStartArathiRpe;
    }

    namespace AdventureMap
    {
        class CheckIsAdventureMapPoiValid;
        class AdventureMapStartQuest;
    }

    namespace AreaTrigger
    {
        class AreaTrigger;
        class UpdateAreaTriggerVisual;
    }

    namespace Artifact
    {
        class ArtifactAddPower;
        class ArtifactSetAppearance;
        class ConfirmArtifactRespec;
    }

    namespace AuctionHouse
    {
        class AuctionBrowseQuery;
        class AuctionCancelCommoditiesPurchase;
        class AuctionConfirmCommoditiesPurchase;
        class AuctionGetCommodityQuote;
        class AuctionHelloRequest;
        class AuctionListBiddedItems;
        class AuctionListBucketsByBucketKeys;
        class AuctionListItemsByBucketKey;
        class AuctionListItemsByItemID;
        class AuctionListOwnedItems;
        class AuctionPlaceBid;
        class AuctionRemoveItem;
        class AuctionReplicateItems;
        class AuctionSellCommodity;
        class AuctionSellItem;
        class AuctionSetFavoriteItem;
    }

    namespace Auth
    {
        enum class ConnectToSerial : uint32;
        class LatencyReport;
        class LogStreamingError;
        class QueuedMessagesEnd;
        class SuspendCommsAck;
    }

    namespace Azerite
    {
        class AzeriteEmpoweredItemSelectPower;
        class AzeriteEmpoweredItemViewed;
        class AzeriteEssenceUnlockMilestone;
        class AzeriteEssenceActivateEssence;
    }

    namespace Bank
    {
        class AutoBankItem;
        class AutoStoreBankItem;
        class BuyBankTab;
        class UpdateBankTabSettings;
        class AutoDepositCharacterBank;
        class AutoDepositAccountBank;
        class AccountBankDepositMoney;
        class AccountBankWithdrawMoney;
        class BankerActivate;
    }

    namespace Battleground
    {
        class AreaSpiritHealerQuery;
        class AreaSpiritHealerQueue;
        class HearthAndResurrect;
        class PVPLogDataRequest;
        class BattlemasterJoin;
        class BattlemasterJoinArena;
        class BattlemasterJoinRatedBGBlitz;
        class BattlemasterJoinSkirmish;
        class BattlemasterJoinBrawl;
        class JoinRatedBattleground;
        class StartWarGame;
        class AcceptWargameInvite;
        class BattlefieldLeave;
        class BattlefieldPort;
        class BattlefieldListRequest;
        class GetPVPOptionsEnabled;
        class RequestBattlefieldStatus;
        class ReportPvPPlayerAFK;
        class RequestPVPRewards;
        class RequestRatedPvpInfo;
        class RequestScheduledPvpInfo;
    }

    namespace Battlenet
    {
        class ChangeRealmTicket;
        class Request;

        // Einheit w4_cmsg_43_3D, Block B4 (Bnet-Praesenz / Community)
        class AddBattlenetFriend;
        class BattlenetChallengeResponse;
        class ClubPresenceSubscribe;
        class SendCharacterClubInvitation;
        class RequestCharacterGuildFollowInfo;
        class SaveAccountDataExport;
        class RequestRealmGuildMasterInfo;
    }

    // Einheit w4_cmsg_43_3D, Block B8 (Bleep, Token-Proxy)
    namespace Bleep
    {
        class FetchBleepProxies;
        class BleepPong;
        class RefreshBleepTokens;
        class ExpireBleepTokens;
    }

    // Einheit w4_cmsg_43_3D, Block B5 (Live-Region-Kopie)
    namespace LiveRegion
    {
        class GetAccountCharacterList;
        class CharacterCopy;
        class AccountRestore;
        class KeyBindingsCopy;
    }

    // Einheit w4_cmsg_43_3D, Block B6 (Lobby-Matchmaker) und B7 (WoW Labs)
    namespace LobbyMatchmaker
    {
        class LobbyMatchmakerPartyInvite;
        class LobbyMatchmakerAcceptPartyInvite;
        class LobbyMatchmakerRejectPartyInvite;
        class LobbyMatchmakerPartyUninvite;
        class LobbyMatchmakerLeaveParty;
        class LobbyMatchmakerSetPartyPlaylistEntry;
        class LobbyMatchmakerSetPlayerReady;
        class LobbyMatchmakerEnterQueue;
        class LobbyMatchmakerQueueProposalResponse;
        class LobbyMatchmakerAbandonQueue;
        class LobbyMatchmakerCreateCharacter;
        class RegisterFastLogin;
    }

    // Einheit w4_cmsg_43_3D, Block B9 (Voice-Chat)
    namespace VoiceChat
    {
        class VoiceChatLogin;
        class VoiceChannelSttTokenRequest;
        class VoiceChatJoinChannel;
    }

    // Einheit w4_cmsg_43_3D, Block B7 (WoW Labs / Plunderstorm)
    namespace WowLabs
    {
        class QuerySelectedWowLabsArea;
        class QueryWowLabsAreaInfo;
        class SelectWowLabsArea;
    }

    namespace BattlePet
    {
        class BattlePetRequestJournal;
        class BattlePetRequestJournalLock;
        class BattlePetSetBattleSlot;
        class BattlePetModifyName;
        class QueryBattlePetName;
        class BattlePetDeletePet;
        class BattlePetSetFlags;
        class BattlePetClearFanfare;
        class BattlePetSummon;
        class BattlePetUpdateNotify;
        class BattlePetUpdateDisplayNotify;
        class CageBattlePet;
        // Pet Battle combat
        class PetBattleRequestWild;
        class PetBattleInput;
        class PetBattleReplaceFrontPet;
        class PetBattleQuitNotify;
        class PetBattleFinalNotify;
        class PetBattleRequestPVP;
        class JoinPetBattleQueue;
        class LeavePetBattleQueue;
        class PetBattleQueueProposeMatchResult;
        class PetBattleRequestUpdate;
        class PetBattleScriptErrorNotify;
        class PetBattleWildLocationFail;
    }

    namespace BlackMarket
    {
        class BlackMarketOpen;
        class BlackMarketRequestItems;
        class BlackMarketBidOnItem;
        class BlackMarketOutbid;
    }

    namespace Calendar
    {
        class CalendarAddEvent;
        class CalendarCopyEvent;
        class CalendarInvite;
        class CalendarModeratorStatusQuery;
        class CalendarRSVP;
        class CalendarEventSignUp;
        class CalendarStatus;
        class CalendarGetCalendar;
        class CalendarGetEvent;
        class CalendarGetNumPending;
        class CalendarCommunityInviteRequest;
        class CalendarRemoveEvent;
        class CalendarRemoveInvite;
        class CalendarUpdateEvent;
        class SetSavedInstanceExtend;
        class CalendarComplain;
    }

    namespace ChallengeMode
    {
        class RequestMythicPlusSeasonData;
        class RequestMythicPlusAffixes;
        class StartChallengeMode;
        class ResetChallengeMode;
        class MythicPlusRequestMapStats;
        class RequestLeaders;
        class RequestWeeklyRewards;
        class ClaimWeeklyReward;
    }

    namespace Character
    {
        struct CharacterCreateInfo;
        struct CharacterRenameInfo;
        struct CharCustomizeInfo;
        struct CharRaceOrFactionChangeInfo;
        struct CharacterUndeleteInfo;

        class AlterApperance;
        class EnumCharacters;
        class CreateCharacter;
        class CharDelete;
        class CharacterRenameRequest;
        class CharCustomize;
        class CharRaceOrFactionChange;
        class CheckCharacterNameAvailability;
        class GenerateRandomCharacterName;
        class GetUndeleteCharacterCooldownStatus;
        class ReorderCharacters;
        class UndeleteCharacter;
        class PlayerLogin;
        class EncounterJournalStartArathiRpe;
        class SetupWarbandGroups;
        class LogoutRequest;
        class LogoutCancel;
        class LoadingScreenNotify;
        class SetActionBarToggles;
        class RequestPlayedTime;
        class SetTitle;
        class SetFactionAtWar;
        class SetFactionNotAtWar;
        class SetFactionInactive;
        class SetWatchedFaction;
        class SetPlayerDeclinedNames;
        class SavePersonalEmblem;
        class NeutralPlayerSelectFaction;

        enum class LoginFailureReason : uint8;
    }

    namespace ClientConfig
    {
        class RequestAccountData;
        class UserClientUpdateAccountData;
        class SetAdvancedCombatLogging;
    }

    namespace Channel
    {
        class ChannelCommand;
        class ChannelPlayerCommand;
        class ChannelPassword;
        class JoinChannel;
        class LeaveChannel;
    }

    namespace Chat
    {
        class ChatMessage;
        class ChatMessageWhisper;
        class ChatMessageChannel;
        class ChatAddonMessage;
        class ChatAddonMessageTargeted;
        class ChatMessageAFK;
        class ChatMessageDND;
        class ChatMessageEmote;
        class CTextEmote;
        class EmoteClient;
        class ChatRegisterAddonPrefixes;
        class ChatUnregisterAllAddonPrefixes;
        class ChatReportIgnored;
        class CanLocalWhisperTargetRequest;
        class UpdateAADCStatus;
        class CautionaryAction;
    }

    namespace Collections
    {
        class CollectionItemSetFavorite;
    }

    namespace Contribution
    {
        class ContributionContribute;
        class ContributionLastUpdateRequest;
    }

    namespace Combat
    {
        class AttackSwing;
        class AttackStop;
        class SetSheathed;
    }

    namespace Covenant
    {
        class ActivateSoulbind;
        class RequestCovenantCallings;
        class CovenantRenownRequestCatchupState;
    }

    namespace Commentator
    {
        class CommentatorEnable;
        class CommentatorGetMapInfo;
        class CommentatorEnterInstance;
        class CommentatorExitInstance;
        class CommentatorSpectate;
        class CommentatorGetPlayerInfo;
        class CommentatorGetPlayerCooldowns;
        class CommentatorStartWargame;
    }

    namespace Duel
    {
        class CanDuel;
        class DuelResponse;
    }

    namespace EquipmentSet
    {
        class SaveEquipmentSet;
        class AssignEquipmentSetSpec;
        class DeleteEquipmentSet;
        class UseEquipmentSet;
    }

    namespace GameObject
    {
        class GameObjReportUse;
        class GameObjUse;
    }

    namespace Garrison
    {
        class GetGarrisonInfo;
        class GarrisonPurchaseBuilding;
        class GarrisonCancelConstruction;
        class GarrisonRequestBlueprintAndSpecializationData;
        class GarrisonGetMapData;
        class GarrisonStartMission;
        class GarrisonCompleteMission;
        class GarrisonMissionBonusRoll;
        class GarrisonGetMissionReward;
        class OpenMissionNpc;
        class UpgradeGarrison;
        class GarrisonCheckUpgradeable;
        class GarrisonSetBuildingActive;
        class GarrisonSwapBuildings;
        class GarrisonAssignFollowerToBuilding;
        class GarrisonRemoveFollowerFromBuilding;
        class GarrisonRemoveFollower;
        class GarrisonRenameFollower;
        class GarrisonSetFollowerFavorite;
        class GarrisonSetFollowerInactive;
        class GarrisonRecruitFollower;
        class GarrisonGenerateRecruits;
        class GarrisonFullyHealAllFollowers;
        class GarrisonAddFollowerHealth;
        class GarrisonGetClassSpecCategoryInfo;
        class GarrisonSetRecruitmentPreferences;
        class GarrisonLearnTalent;
        class GarrisonResearchTalent;
        class GarrisonSocketTalent;
        class GarrisonRequestShipmentInfo;
        class OpenShipmentNpc;
        class CreateShipment;
        class GetLandingPageShipments;
        class SetUsingPartyGarrison;
        class QueryGarrisonPetName;
        class RequestGarrisonTalentWorldQuestUnlocks;
        class GetTrophyList;
        class ReplaceTrophy;
        class LoadSelectedTrophy;
        class ChangeMonumentAppearance;
        class RevertMonumentAppearance;
        class GarrisonSocketTalent;
    }

    namespace Guild
    {
        class QueryGuildInfo;
        class GuildInviteByName;
        class AcceptGuildInvite;
        class DeclineGuildInvites;
        class GuildDeclineInvitation;
        class GuildGetRoster;
        class GuildPromoteMember;
        class GuildDemoteMember;
        class GuildOfficerRemoveMember;
        class GuildAssignMemberRank;
        class GuildLeave;
        class GuildDelete;
        class GuildUpdateMotdText;
        class GuildGetRanks;
        class GuildAddRank;
        class GuildDeleteRank;
        class GuildShiftRank;
        class GuildUpdateInfoText;
        class GuildSetMemberNote;
        class GuildEventLogQuery;
        class GuildBankRemainingWithdrawMoneyQuery;
        class GuildPermissionsQuery;
        class GuildSetRankPermissions;
        class GuildBankActivate;
        class GuildBankQueryTab;
        class GuildBankDepositMoney;
        class GuildBankWithdrawMoney;
        class AutoGuildBankItem;
        class StoreGuildBankItem;
        class SwapItemWithGuildBankItem;
        class SwapGuildBankItemWithGuildBankItem;
        class MoveGuildBankItem;
        class MergeItemWithGuildBankItem;
        class SplitItemToGuildBank;
        class MergeGuildBankItemWithItem;
        class SplitGuildBankItemToInventory;
        class AutoStoreGuildBankItem;
        class MergeGuildBankItemWithGuildBankItem;
        class SplitGuildBankItem;
        class GuildBankBuyTab;
        class GuildBankUpdateTab;
        class GuildBankLogQuery;
        class GuildBankTextQuery;
        class GuildBankSetTabText;
        class RequestGuildPartyState;
        class RequestGuildRewardsList;
        class GuildQueryNews;
        class GuildNewsUpdateSticky;
        class GuildReplaceGuildMaster;
        class GuildSetGuildMaster;
        class GuildChallengeUpdateRequest;
        class SaveGuildEmblem;
        class GuildSetAchievementTracking;
        class GuildQueryRecipes;
        class GuildQueryMemberRecipes;
        class GuildQueryMembersForRecipe;
        class GuildChangeNameRequest;
    }

    namespace Hotfix
    {
        class DBQueryBulk;
        class HotfixRequest;
        struct HotfixData;
    }


    namespace Housing
    {
        class DeclineNeighborhoodInvites;
    }

    namespace Inspect
    {
        class Inspect;
        class QueryInspectAchievements;
    }

    namespace Instance
    {
        class InstanceAbandonVoteResponse;
        class InstanceInfo;
        class InstanceLockResponse;
        class ResetInstances;
        class StartInstanceAbandonVote;
    }

    namespace Item
    {
        class AutoEquipItem;
        class AutoEquipItemSlot;
        class AutoStoreBagItem;
        class BuyItem;
        class BuyBackItem;
        class DestroyItem;
        class GetItemPurchaseData;
        class ItemPurchaseRefund;
        class PerformItemInteraction;
        class RepairItem;
        class ReadItem;
        class SellItem;
        class SellAllJunkItems;
        class SplitItem;
        class SwapInvItem;
        class SwapItem;
        class WrapItem;
        class CancelTempEnchantment;
        class UseCritterItem;
        class SocketGems;
        class SortAccountBankBags;
        class SortBags;
        class SortBankBags;
        struct ItemInstance;
        class RemoveNewItem;
        class ChangeBagSlotFlag;
        class SetBackpackAutosortDisabled;
        class SetSortBagsRightToLeft;
        class SetInsertItemsLeftToRight;
        class SetBackpackSellJunkDisabled;
        class SetBankAutosortDisabled;
        class PerformItemInteraction;
        class ConvertItemToBindToAccount;
    }

    namespace LFG
    {
        class DFJoin;
        class DFLeave;
        class DFProposalResponse;
        class DFSetRoles;
        class DFBootPlayerVote;
        class DFTeleport;
        class DFGetSystemInfo;
        class DFGetJoinStatus;
        class DFConfirmExpandSearch;
        class DFReadyCheckResponse;
        class LFGLorewalkingUpdateRequest;
        struct RideTicket;
    }

    namespace LFGList
    {
        class LFGListJoin;
        class LFGListUpdateRequest;
        class LFGListLeave;
        class LFGListGetStatus;
        class LFGListSearch;
        class LFGListApplyToGroup;
        class LFGListCancelApplication;
        class LFGListDeclineApplicant;
        class LFGListInviteApplicant;
        class LFGListInviteResponse;
        class LFGListConfirmCensoredActiveEntry;
        class RequestLFGListBlacklist;
    }

    namespace Loot
    {
        class LootUnit;
        class LootItem;
        class MasterLootItem;
        class DoMasterLootRoll;
        class CancelMasterLootRoll;
        class LootRelease;
        class LootMoney;
        class LootRoll;
        class SetLootSpecialization;
    }

    namespace Mail
    {
        class GetRegionwideCharacterRestrictionAndMailData;
        class MailCreateTextItem;
        class MailDelete;
        class MailGetList;
        class MailMarkAsRead;
        class MailQueryNextMailTime;
        class MailReturnToSender;
        class MailTakeItem;
        class MailTakeMoney;
        class SendMail;
    }

    namespace Misc
    {
        class SetSelection;
        class ViolenceLevel;
        class GetCharacterCurrencyTransferLog;
        class RequestCurrencyDataForAccountCharacters;
        class TimeSyncResponse;
        class TimeSyncResponseFailed;
        class TimeSyncResponseDropped;
        class DiscardedTimeSyncAcks;
        class TimeAdjustmentResponse;
        class DiscardedTimeSyncAcks;
        class TutorialSetFlag;
        class SetDungeonDifficulty;
        class SetRaidDifficulty;
        class PortGraveyard;
        class ReclaimCorpse;
        class RepopRequest;
        class ReportStuckInCombat;
        class RequestCemeteryList;
        class SetPreferredCemetery;
        class ResurrectResponse;
        class StandStateChange;
        class ServerTimeOffsetRequest;
        class RandomRollClient;
        class ObjectUpdateFailed;
        class ObjectUpdateRescued;
        class CompleteCinematic;
        class CompleteMovie;
        class NextCinematicCamera;
        class FarSight;
        class LoadCUFProfiles;
        class SaveCUFProfiles;
        class OpeningCinematic;
        class TogglePvP;
        class SetPvP;
        class SetWarMode;
        class MountSpecial;
        class SetTaxiBenchmarkMode;
        class MountSetFavorite;
        class MountClearFanfare;
        class CloseInteraction;
        class CloseRuneforgeInteraction;
        class CloseTraitSystemInteraction;
        class ConversationLineStarted;
        class RequestLatestSplashScreen;
        class QueryCountdownTimer;
        class DoCountdown;
        class GetRemainingGameTime;
        class SetStopConversation;
        class SetCurrencyFlags;

        // Einheit w4_cmsg_43_3D - Sendeseite der Sammelfamilien 0x43 / 0x3D
        class UsedFollow;
        class SeamlessTransferComplete;
        class ReportServerLag;
        class ResetChallengeModeCheat;
        class SpectateEnd;
        class SpectateChange;
        class SpectateSetNextTarget;
        class LowLevelRaid1;
        class QuickJoinAutoAcceptRequests;
        class RequestChatLogin;
        class ClassTalentsNotifyEmptyConfig;
        class ClassTalentsNotifyValidationFailed;
        class TraitsTalentTestUnlearnSpells;
        class GMTicketAcknowledgeSurvey;
        class AddAccountCosmetic;
        class UpdateCraftingNpcRecipes;
        class IslandQueue;
        class ShowTradeSkill;
        class QuestDrivenScenarioStateChange;
        class WorldLootObjectClick;
        class UpgradeRuneforgeLegendary;
        class SetExcludedChatCensorSources;
        class SilenceTalkerInParty;
        class Warden3Data;
        class AddonList;
        class EngineSurvey;
        class StartSpectatorWarGame;
        class QuickJoinRespondToInvite;
        class QuickJoinSignalToastDisplayed;
        class QuickJoinRequestInvite;
        class QuickJoinRequestInviteWithConfirmation;
        class ServerValidationSignatureRequest;
        class AbandonNPEResponse;
        class SubscriptionInterstitialResponse;
        class RequestScheduledAreaPoiUpdate;
        class BonusRoll;
        class KioskEnableGodMode;
        class SetGameEventDebugViewState;
        class RequestCurrencyDataForAccountCharacters;
        class TransferCurrencyFromAccountCharacter;
        class GetCharacterCurrencyTransferLog;
    }

    namespace MajorFactions
    {
        class RequestCatchupState;
    }

    namespace ClubFinder
    {
        class ClubFinderPost;
        class ClubFinderRequestSubscribedClubPostingIds;
        class ClubFinderRequestClubsData;
        class ClubFinderRequestClubsList;
        class ClubFinderRequestMembershipToClub;
        class ClubFinderGetApplicantsList;
        class ClubFinderRequestPendingClubsList;
        class ClubFinderRespondToApplicant;
        class ClubFinderApplicationResponse;
        class ClubFinderWhisperApplicantRequest;
    }

    namespace BattlePay
    {
        class UpdateVasPurchaseStates;
        class VasGetServiceStatus;
        class GetProductList;
        class GetPurchaseList;
        class StartPurchase;
        class OpenCheckout;
        class CatalogShopLicenseGameDataRequest;
        class ConfirmPurchaseResponse;
        class DistributionAssignToTarget;
        class CharacterUpgradeStart;
    }

    namespace ChallengeMode
    {
        class RequestMythicPlusSeasonData;
        class RequestMythicPlusAffixes;
        class StartChallengeMode;
        class ResetChallengeMode;
        class MythicPlusRequestMapStats;
    }

    namespace WeeklyRewards
    {
        class ClaimWeeklyReward;
        class RequestWeeklyRewards;
    }
    namespace Delves
    {
        class DelveTeleportOut;
        class RequestPartyEligibilityForDelveTiers;
        class SelectDelveEntranceTier;
        class TieredEntranceOpen;
    }

    namespace Housing
    {
        // Patch 12.1.0 (build 69299) blueprint packets (HousingBlueprintPackets.h)
        class HousingBlueprintRequestCollection;
        class HousingBlueprintRequestContents;
        class HousingBlueprintExport;
        class HousingBlueprintExportRoom;
        class HousingBlueprintRename;
        class HousingBlueprintImport;
        class HouseExteriorCommitPosition;
        class HouseInteriorLeaveHouse;
        class HousingDecorSetEditMode;
        class HousingDecorPlace;
        class HousingDecorMove;
        class HousingDecorRemove;
        class HousingDecorLock;
        class HousingDecorSetDyeSlots;
        class HousingDecorDeleteFromStorage;
        class HousingDecorRequestStorage;
        class HousingDecorRedeemDeferredDecor;
        class HousingDecorStartPlacingNewDecor;
        class HousingDecorCatalogCreateSearcher;
        class GetLastCatalogFetch;
        class UpdateLastCatalogFetch;
        class HousingFixtureSetEditMode;
        class HousingFixtureSetCoreFixture;
        class HousingFixtureCreateFixture;
        class HousingFixtureDeleteFixture;
        class HousingRoomSetLayoutEditMode;
        class HousingRoomAdd;
        class HousingRoomRemove;
        class HousingRoomRotate;
        class HousingRoomMoveRoom;
        class HousingRoomSetComponentTheme;
        class HousingRoomApplyComponentMaterials;
        class HousingRoomSetDoorType;
        class HousingRoomSetCeilingType;
        class HousingSvcsGuildCreateNeighborhood;
        class HousingSvcsNeighborhoodReservePlot;
        class HousingSvcsRelinquishHouse;
        class HousingSvcsUpdateHouseSettings;
        class HousingSvcsPlayerViewHousesByPlayer;
        class HousingSvcsPlayerViewHousesByBnetAccount;
        class HousingSvcsGetPlayerHousesInfo;
        class HousingSvcsTeleportToPlot;
        class HousingSvcsStartTutorial;
        class HousingSvcsSetTutorialState;
        class HousingSvcsCompleteTutorialStep;
        class HousingSvcsSkipTutorial;
        class HousingSvcsQueryPendingInvites;
        class HousingSvcsAcceptNeighborhoodOwnership;
        class HousingSvcsRejectNeighborhoodOwnership;
        class HousingSvcsGetPotentialHouseOwners;
        class HousingSvcsGetHouseFinderInfo;
        class HousingSvcsGetHouseFinderNeighborhood;
        class HousingSvcsGetBnetFriendNeighborhoods;
        class HousingSvcsDeleteAllNeighborhoodInvites;
        class HousingHouseStatus;
        class HousingGetCurrentHouseInfo;
        class HousingGetPlayerPermissions;
        class HousingResetKioskMode;
        class DeclineNeighborhoodInvites;
        class QueryNeighborhoodInfo;
        class InvitePlayerToNeighborhood;
        class GuildGetOthersOwnedHouses;
        class HouseExteriorLock;
        class HousingPhotoSharingCompleteAuthorization;
        class HousingPhotoSharingClearAuthorization;
        class HousingFixtureSetHouseSize;
        class HousingFixtureSetHouseType;
        class GetAllLicensedDecorQuantities;
        class GetDecorRefundList;
        class BulkRefund;
        class HousingRequestEditorAvailability;
        class HousingDecorStartPlacingFromSource;
        class HousingDecorBatchOperation;
        class HousingDecorPlacementPreview;
        // Retired 2026-05-12 (batch 2): 8 fake SVCS CMSG class forward decls deleted.
        // Retired 2026-05-12: group 0x35 system CMSG classes (HouseStatusQuery, GetHouseInfoAlt,
        // HouseSnapshot, ExportHouse, UpdateHouseInfo) â€” no client senders in build 67186.
    }

    namespace Social
    {
        class AddFriend;
        class AddIgnore;
        class DelFriend;
        class DelIgnore;
        class MakeConditionalAppearancePermanent;
        class RecentAllyRequestData;
        class RecentAllySetNote;
        class SendContactList;
        class SetAllowRecentAlliesSeeLocation;
        class SetContactNotes;
        class SocialContractRequest;
        class AcceptSocialContract;
    }

    namespace Contribution
    {
        class ContributionContribute;
        class ContributionLastUpdateRequest;
    }
    namespace Movement
    {
        class ClientPlayerMovement;
        class WorldPortResponse;
        class MoveTeleportAck;
        class MovementAckMessage;
        class MovementSpeedAck;
        class MovementSpeedRangeAck;
        class MoveKnockBackAck;
        class SetActiveMover;
        class MoveSetCollisionHeightAck;
        class MoveTimeSkipped;
        class SummonResponse;
        class MoveSplineDone;
        class SuspendTokenResponse;
        class MoveApplyMovementForceAck;
        class MoveRemoveMovementForceAck;
        class MoveApplyInertiaAck;
        class MoveRemoveInertiaAck;
        class MoveInitActiveMoverComplete;
        class MoveSetTurnRateCheat;
    }

    namespace NPC
    {
        class Hello;
        class GossipRefreshOptions;
        class GossipSelectOption;
        class SpiritHealerActivate;
        class TabardVendorActivate;
        class TrainerBuySpell;
        class RequestStabledPets;
        class SetPetSlot;
    }

    namespace Party
    {
        class PartyCommandResult;
        class PartyInviteClient;
        class PartyInvite;
        class PartyInviteResponse;
        class PartyUninvite;
        class QuickJoinRespondToInvite;
        class GroupDecline;
        class RequestPartyMemberStats;
        class PartyMemberFullState;
        class SetPartyLeader;
        class SetPartyAssignment;
        class SetRole;
        class RoleChangedInform;
        class SetLootMethod;
        class LeaveGroup;
        class MinimapPingClient;
        class MinimapPing;
        class UpdateRaidTarget;
        class SendRaidTargetUpdateSingle;
        class SendRaidTargetUpdateAll;
        class ConvertRaid;
        class RequestPartyJoinUpdates;
        class SetAssistantLeader;
        class DoReadyCheck;
        class ReadyCheckStarted;
        class ReadyCheckResponseClient;
        class ReadyCheckResponse;
        class ReadyCheckCompleted;
        class RequestRaidInfo;
        class OptOutOfLoot;
        class InitiateRolePoll;
        class RolePollInform;
        class GroupNewLeader;
        class PartyUpdate;
        class SetEveryoneIsAssistant;
        class ChangeSubGroup;
        class SwapSubGroups;
        class RaidMarkersChanged;
        class ClearRaidMarker;
        class SetRestrictPingsToAssistants;
        class SendPingUnit;
        class SendPingWorldPoint;
        class SendPingCooldown;
    }

    namespace Pet
    {
        class DismissCritter;
        class RequestPetInfo;
        class PetAbandon;
        class PetAbandonByNumber;
        class PetStopAttack;
        class PetSpellAutocast;
        class PetRename;
        class PetAction;
        class PetCancelAura;
        class PetSetAction;
    }

    namespace Petition
    {
        class DeclinePetition;
        class OfferPetition;
        class PetitionBuy;
        class PetitionRenameGuild;
        class PetitionShowList;
        class PetitionShowSignatures;
        class QueryPetition;
        class SignPetition;
        class TurnInPetition;
    }

    namespace PerksProgram
    {
        class PerksProgramStatusRequest;
        class PerksProgramGetRecentPurchases;
        class PerksProgramRequestPurchase;
        class PerksProgramRequestRefund;
        class PerksProgramSetFrozenVendorItem;
        class PerksProgramRequestCartCheckout;
        class PerksProgramItemsRefreshed;
        class PerksProgramRequestPendingRewards;
    }

    namespace Query
    {
        class QueryCreature;
        struct NameCacheLookupResult;
        struct BNetAccountAndCommunityID;
        class QueryPlayerNames;
        class QueryPlayerNameByCommunityId;
        class QueryPlayerNamesForCommunity;
        class QueryPageText;
        class QueryNPCText;
        class QueryGameObject;
        class QueryCorpseLocationFromClient;
        class QueryCorpseTransport;
        class QueryTime;
        class QueryPetName;
        class QuestPOIQuery;
        class QueryQuestCompletionNPCs;
        class QueryRealmName;
        class ItemTextQuery;
        class QueryTreasurePicker;
    }

    namespace Quest
    {
        class QuestConfirmAccept;
        class QuestGiverStatusQuery;
        class QuestGiverStatusMultipleQuery;
        class QuestGiverHello;
        class QueryQuestInfo;
        class QuestGiverChooseReward;
        class QuestGiverCloseQuest;
        class QuestGiverCompleteQuest;
        class QuestGiverRequestReward;
        class QuestGiverQueryQuest;
        class QuestGiverAcceptQuest;
        class QuestLogRemoveQuest;
        class QuestPushResult;
        class PushQuestToParty;
        class RequestWorldQuestUpdate;
        class ChoiceResponse;
        class CloseQuestChoice;
        class UiMapQuestLinesRequest;
        class SpawnTrackingUpdate;
        class QueryQuestItemUsability;
        class CloseQuestChoice;
    }

    namespace RaF
    {
        class AcceptLevelGrant;
        class GrantLevel;
    }

    namespace Toy
    {
        class AccountToyUpdate;
        class AddToy;
        class UseToy;
        class ToyClearFanfare;
    }

    namespace Scenario
    {
        class QueryScenarioPOI;
    }

    namespace Scenes
    {
        class SceneTriggerEvent;
        class ScenePlaybackComplete;
        class ScenePlaybackCanceled;
    }

    namespace Social
    {
        class AddFriend;
        class AddIgnore;
        class DelFriend;
        class DelIgnore;
        class RecentAllyRequestData;
        class RecentAllySetNote;
        class SetAllowRecentAlliesSeeLocation;
        class RecentAllyRequestData;
        class RecentAllySetNote;
        class SendContactList;
        class SetAllowRecentAlliesSeeLocation;
        class SetContactNotes;
        class SocialContractRequest;
    }

    namespace Spells
    {
        class CancelAura;
        class CancelAutoRepeatSpell;
        class CancelChannelling;
        class CancelGrowthAura;
        class CancelMountAura;
        class CancelModSpeedNoControlAuras;
        class CancelQueuedSpell;
        class PetCancelAura;
        class CancelCast;
        class CastSpell;
        class PetCastSpell;
        class UseItem;
        class OpenItem;
        class SetActionButton;
        class UnlearnSkill;
        class SelfRes;
        class GetMirrorImageData;
        class SpellClick;
        class MissileTrajectoryCollision;
        class UpdateMissileTrajectory;
        class UpdateAuraVisual;
        class TradeSkillSetFavorite;
        class OpenTradeSkillNpc;
        class KeyboundOverride;
        class RequestCrowdControlSpell;
        class SetEmpowerMinHoldStagePercent;
        class SpellEmpowerRelease;
        class SpellEmpowerRestart;
    }

    namespace Talent
    {
        class LearnTalents;
        class UnlearnSpecialization;
        class LearnPvpTalents;
        class ConfirmRespecWipe;
    }

    namespace Taxi
    {
        class ShowTaxiNodes;
        class TaxiNodeStatusQuery;
        class EnableTaxiNode;
        class TaxiQueryAvailableNodes;
        class ActivateTaxi;
        class TaxiRequestEarlyLanding;
    }

    namespace Ticket
    {
        class ChatReportFiltered;
        class GMTicketGetSystemStatus;
        class GMTicketGetCaseStatus;
        class SubmitUserFeedback;
        class SupportTicketSubmitComplaint;
        class BugReport;
        class Complaint;
    }

    namespace Token
    {
        class CommerceTokenGetLog;
        class CommerceTokenGetMarketPrice;
        class CommerceTokenGetCount;
        class AuctionableTokenSell;
        class AuctionableTokenSellAtMarketPrice;
        class CanRedeemTokenForBalance;
        class ConsumableTokenBuy;
        class ConsumableTokenBuyAtMarketPrice;
        class ConsumableTokenCanVeteranBuy;
        class ConsumableTokenRedeem;
        class ConsumableTokenRedeemConfirmation;
    }

    namespace Totem
    {
        class TotemDestroyed;
    }

    namespace Trade
    {
        class AcceptTrade;
        class BeginTrade;
        class BusyTrade;
        class CancelTrade;
        class ClearTradeItem;
        class IgnoreTrade;
        class InitiateTrade;
        class SetTradeCurrency;
        class SetTradeGold;
        class SetTradeItem;
        class UnacceptTrade;
        class TradeStatus;
    }

    namespace Traits
    {
        class TraitsCommitConfig;
        class ClassTalentsRequestNewConfig;
        class ClassTalentsRenameConfig;
        class ClassTalentsDeleteConfig;
        class ClassTalentsSetStarterBuildActive;
        class ClassTalentsSetUsesSharedActionBars;
        class ClassTalentsNotifyEmptyConfig;
        class ClassTalentsNotifyValidationFailed;
        class TraitsTalentTestUnlearnSpells;
        class ConfirmProfessionRespec;
    }

    namespace Transmogrification
    {
        class TransmogrifyItems;
        class TransmogOutfitNew;
        class TransmogOutfitUpdateInfo;
        class TransmogOutfitUpdateSituations;
        class TransmogOutfitUpdateSlots;
    }

    namespace Vehicle
    {
        class MoveDismissVehicle;
        class RequestVehiclePrevSeat;
        class RequestVehicleNextSeat;
        class MoveChangeVehicleSeats;
        class RequestVehicleSwitchSeat;
        class RideVehicleInteract;
        class EjectPassenger;
        class RequestVehicleExit;
        class MoveSetVehicleRecIdAck;
    }

    namespace ContentTracking
    {
        class StartTracking;
        class StopTracking;
    }

    namespace WeeklyRewards
    {
        class ClaimWeeklyReward;
        class RequestWeeklyRewards;
    }

    namespace Who
    {
        class WhoIsRequest;
        class WhoRequestPkt;
    }

    class Null;
}

namespace google
{
    namespace protobuf
    {
        class Message;
    }
}

namespace pb = google::protobuf;

enum AccountDataType
{
    GLOBAL_CONFIG_CACHE                 = 0,
    PER_CHARACTER_CONFIG_CACHE          = 1,
    GLOBAL_BINDINGS_CACHE               = 2,
    PER_CHARACTER_BINDINGS_CACHE        = 3,
    GLOBAL_MACROS_CACHE                 = 4,
    PER_CHARACTER_MACROS_CACHE          = 5,
    PER_CHARACTER_LAYOUT_CACHE          = 6,
    PER_CHARACTER_CHAT_CACHE            = 7,
    GLOBAL_TTS_CACHE                    = 8,
    PER_CHARACTER_TTS_CACHE             = 9,
    GLOBAL_FLAGGED_CACHE                = 10,
    PER_CHARACTER_FLAGGED_CACHE         = 11,
    PER_CHARACTER_CLICK_BINDINGS_CACHE  = 12,
    GLOBAL_EDIT_MODE_CACHE              = 13,
    PER_CHARACTER_EDIT_MODE_CACHE       = 14,
    GLOBAL_FRONTEND_CHAT_SETTINGS       = 15,
    GLOBAL_CHARACTER_LIST_ORDER         = 16,
    GLOBAL_COOLDOWN_MANAGER             = 17,
    PER_CHARACTER_COOLDOWN_MANAGER2     = 18,
    GLOBAL_SHOP2_PENDING_ORDERS         = 19
};

#define NUM_ACCOUNT_DATA_TYPES        20

#define ALL_ACCOUNT_DATA_CACHE_MASK 0x000FFFFFu
#define GLOBAL_CACHE_MASK           0x000BA515u
#define PER_CHARACTER_CACHE_MASK    0x00045AEAu

struct AccountData
{
    time_t Time = 0;
    std::string Data;
};

enum PartyOperation
{
    PARTY_OP_INVITE   = 0,
    PARTY_OP_UNINVITE = 1,
    PARTY_OP_LEAVE    = 2,
    PARTY_OP_SWAP     = 4
};

enum ChatRestrictionType
{
    ERR_CHAT_RESTRICTED      = 0,
    ERR_CHAT_THROTTLED       = 1,
    ERR_USER_SQUELCHED       = 2,
    ERR_YELL_RESTRICTED      = 3,
    ERR_CHAT_RAID_RESTRICTED = 4
};

enum DeclinedNameResult
{
    DECLINED_NAMES_RESULT_SUCCESS = 0,
    DECLINED_NAMES_RESULT_ERROR   = 1
};

enum TutorialsFlag : uint8
{
    TUTORIALS_FLAG_NONE = 0x00,
    TUTORIALS_FLAG_CHANGED = 0x01,
    TUTORIALS_FLAG_LOADED_FROM_DB = 0x02
};

//class to deal with packet processing
//allows to determine if next packet is safe to be processed
class PacketFilter
{
public:
    explicit PacketFilter(WorldSession* pSession) : m_pSession(pSession) { }
    virtual ~PacketFilter() { }

    virtual bool Process(WorldPacket* /*packet*/) { return true; }
    virtual bool ProcessUnsafe() const { return true; }

protected:
    WorldSession* const m_pSession;

private:
    PacketFilter(PacketFilter const& right) = delete;
    PacketFilter& operator=(PacketFilter const& right) = delete;
};

//process only thread-safe packets in Map::Update()
class MapSessionFilter : public PacketFilter
{
public:
    explicit MapSessionFilter(WorldSession* pSession) : PacketFilter(pSession) { }
    ~MapSessionFilter() { }

    virtual bool Process(WorldPacket* packet) override;
    //in Map::Update() we do not process player logout!
    virtual bool ProcessUnsafe() const override { return false; }
};

//class used to filer only thread-unsafe packets from queue
//in order to update only be used in World::UpdateSessions()
class WorldSessionFilter : public PacketFilter
{
public:
    explicit WorldSessionFilter(WorldSession* pSession) : PacketFilter(pSession) { }
    ~WorldSessionFilter() { }

    bool Process(WorldPacket* packet) override;
    bool ProcessUnsafe() const override { return true; }
};

struct PacketCounter
{
    time_t lastReceiveTime;
    uint32 amountCounter;
};

/// Player session in the World
class TC_GAME_API WorldSession
{
    public:
        WorldSession(uint32 id, std::string&& name, uint32 battlenetAccountId, std::string&& battlenetAccountEmail,
            std::shared_ptr<WorldSocket>&& sock, AccountTypes sec, uint8 expansion, time_t mute_time, std::string&& os, Minutes timezoneOffset,
            uint32 build, ClientBuild::VariantId clientBuildVariant, LocaleConstant locale, uint32 recruiter, bool isARecruiter);
        ~WorldSession();

        bool PlayerLoading() const { return !m_playerLoading.IsEmpty(); }
        bool PlayerLogout() const { return m_playerLogout; }
        bool PlayerLogoutWithSave() const { return m_playerLogout && m_playerSave; }
        bool PlayerRecentlyLoggedOut() const { return m_playerRecentlyLogout; }
        bool PlayerDisconnected() const;

        bool IsAddonRegistered(std::string_view prefix) const;

        void SendPacket(WorldPacket const* packet, bool forced = false);

        void SendNotification(char const* format, ...) ATTR_PRINTF(2, 3);
        void SendNotification(uint32 stringId, ...);
        void SendPetNameInvalid(uint32 error, std::string const& name, Optional<DeclinedName> const& declinedName);
        void SendPartyResult(PartyOperation operation, std::string const& member, PartyResult res, uint32 val = 0);
        void SendQueryTimeResponse();

        // Invite confirmations that this session's player, as group leader, has been asked about
        // with SMSG_CONFIRM_PARTY_INVITE and has not answered yet. Pure session state - the guids
        // are tickets and must not survive a relog, so nothing of this is persisted.
        // The client keeps at most four outstanding confirmations (ring buffer, allocator RVA
        // 0x2198990 at build 12.1.0.69382) and silently drops anything beyond that, so the server
        // refuses to send a fifth instead of losing it.
        static constexpr std::size_t MaxPendingInviteConfirmations = 4;
        // How long a ticket stays answerable. A server side choice like SuggestedInviteCooldown -
        // the client prescribes no lifetime - but unlike the cooldown it is load bearing: it is the
        // only thing that frees the four slots above again when a leader never answers at all.
        // UNVERIFIED: whether the client closes the GROUP_INVITE_CONFIRMATION dialog by itself, and
        // after how long. No timer for it is readable in the memory image at build 12.1.0.69382 and
        // no recording shows one, so the dialog may outlive the ticket on screen. Because of that an
        // answer arriving after the ticket ran out is ANSWERED, not dropped - see
        // HandleInviteConfirmationResponse. The 120 s themselves are picked, not measured.
        static constexpr Seconds PendingInviteConfirmationTimeout = Seconds(120);
        // How often this session may make the leader's client show a suggested invite. A server
        // side choice - the client prescribes no interval - and the only bound on a message that
        // one player triggers at a third one. See SendSuggestedInvite.
        static constexpr Seconds SuggestedInviteCooldown = Seconds(10);

        struct PendingInviteConfirmation
        {
            ObjectGuid ApplicantGUID;       // queue key, HighGuid::Player
            ObjectGuid PartyGUID;
            ObjectGuid ReferredByGUID;
            TimePoint ExpireTime;
        };

        bool SendSuggestedInvite(Group* group, Player* suggester, Player* target);
        bool SendInviteConfirmation(Player* leader, Group* group, Player* target, Player* referredBy);
        bool AddPendingInviteConfirmation(PendingInviteConfirmation confirmation);
        // Consumes the ticket matching BOTH guids and returns it even when it has already run out;
        // the caller decides what an expired one means. Returning nothing therefore says "no such
        // ticket", never "too late", so that lateness can be reported instead of swallowed.
        Optional<PendingInviteConfirmation> TakePendingInviteConfirmation(ObjectGuid applicantGuid, ObjectGuid partyGuid);
        // Answer to SMSG_CONFIRM_PARTY_INVITE, reached from HandleQuickJoinRespondToInvite. Note
        // that the client swaps the two guids on the way back: the first guid of the response is
        // SMSG_CONFIRM_PARTY_INVITE.ApplicantGUID, the second is its PartyGUID (writer RVA
        // 0x6AF830, response builder RVA 0x21981D0).
        void HandleInviteConfirmationResponse(ObjectGuid applicantGuid, ObjectGuid partyGuid, bool accept);
        // Called from LogoutPlayer. The session survives the character, the tickets must not - see
        // the definition in GroupHandler.cpp.
        void ClearPendingPartyInviteState();

        void SendAuthResponse(uint32 code, bool queued, uint32 queuePos = 0);
        void SendClientCacheVersion(uint32 version);
        void SendCacheInfo();
        void SendAvailableHotfixes();

        void InitializeSession();
        void InitializeSessionCallback(LoginDatabaseQueryHolder const& holder, CharacterDatabaseQueryHolder const& realmHolder);

        rbac::RBACData* GetRBACData() const;
        bool HasPermission(uint32 permissionId);
        void LoadPermissions();
        QueryCallback LoadPermissionsAsync();
        void InvalidateRBACData(); // Used to force LoadPermissions at next HasPermission check

        AccountTypes GetSecurity() const { return _security; }
        uint32 GetAccountId() const { return _accountId; }
        ObjectGuid GetAccountGUID() const { return ObjectGuid::Create<HighGuid::WowAccount>(GetAccountId()); }
        std::string const& GetAccountName() const { return _accountName; }
        uint32 GetBattlenetAccountId() const;
        ObjectGuid GetBattlenetAccountGUID() const;
        Battlenet::Account& GetBattlenetAccount() const { return *_battlenetAccount; }
        Player* GetPlayer() const { return _player; }
        std::string const& GetPlayerName() const;
        std::string GetPlayerInfo() const;

        void SetSecurity(AccountTypes security) { _security = security; }
        std::string const& GetRemoteAddress() const { return m_Address; }
        void SetPlayer(Player* player);
        uint8 GetAccountExpansion() const { return m_accountExpansion; }
        uint8 GetExpansion() const { return m_expansion; }
        std::string const& GetOS() const { return _os; }
        uint32 GetClientBuild() const { return _clientBuild; }
        ClientBuild::VariantId const& GetClientBuildVariant() const { return _clientBuildVariant; }

        bool CanAccessAlliedRaces() const;

        // Allied-race unlock gate seam (feature/haranir-allied-race).
        // Returns whether this account has completed the account-wide allied-race unlock
        // achievement `achievementId` (e.g. Haranir = 61506, heritage = 61942). Centralizes
        // the single enforcement point that the char-enum and char-create paths consult.
        // Currently a permissive stub - see WorldSession.cpp / HARANIR_ALLIED_RACE_BLUEPRINT.md.
        bool HasRaceUnlockAchievement(uint32 achievementId) const;

        /// Session in auth.queue currently
        void SetInQueue(bool state) { m_inQueue = state; }

        /// Is the user engaged in a log out process?
        bool isLogingOut() const { return _logoutTime || m_playerLogout; }

        /// Engage the logout process for the user
        void SetLogoutStartTime(time_t requestTime)
        {
            _logoutTime = requestTime;
        }

        /// Is logout cooldown expired?
        bool ShouldLogOut(time_t currTime) const
        {
            return (_logoutTime > 0 && currTime >= _logoutTime + 20);
        }

        void LogoutPlayer(bool save);
        void KickPlayer(std::string_view reason);
        // Returns true if all contained hyperlinks are valid
        // May kick player on false depending on world config (handler should abort)
        bool ValidateHyperlinksAndMaybeKick(std::string const& str);
        // Returns true if the message contains no hyperlinks
        // May kick player on false depending on world config (handler should abort)
        bool DisallowHyperlinksAndMaybeKick(std::string const& str);

        void QueuePacket(WorldPacket&& new_packet);
        bool Update(uint32 diff, PacketFilter& updater);

        /// Handle the authentication waiting queue (to be completed)
        void SendAuthWaitQueue(uint32 position);

        void SendSetTimeZoneInformation();
        void SendFeatureSystemStatus();
        void SendFeatureSystemStatusGlueScreen();

        void BuildNameQueryData(ObjectGuid guid, WorldPackets::Query::NameCacheLookupResult& lookupData);
        void SendPlayerNameByCommunityId(WorldPackets::Query::BNetAccountAndCommunityID const& member);

        void SendTrainerList(Creature* npc, uint32 trainerId);
        void SendListInventory(ObjectGuid guid);
        void SendShowBank(ObjectGuid guid, PlayerInteractionType interactionType);
        bool CanOpenMailBox(ObjectGuid guid);
        void SendShowMailBox(ObjectGuid guid);
        void SendTabardVendorActivate(ObjectGuid guid, TabardVendorType type);
        void SendSpiritResurrect();
        void SendBindPoint(Creature* npc);
        void SendOpenTransmogrifier(ObjectGuid const& guid);

        void SendTradeStatus(WorldPackets::Trade::TradeStatus& status);
        void SendUpdateTrade(bool trader_data = true);
        void SendCancelTrade(TradeStatus status);

        void SendPetitionQueryOpcode(ObjectGuid petitionguid);

        // Pet
        void SendQueryPetNameResponse(ObjectGuid guid);
        void SendPetStableResult(StableResult result);
        bool CheckStableMaster(ObjectGuid guid);

        // Account Data
        AccountData const* GetAccountData(AccountDataType type) const { return &_accountData[type]; }
        void SetAccountData(AccountDataType type, time_t time, std::string const& data);
        void SendAccountDataTimes(ObjectGuid playerGuid, uint32 mask);
        void LoadAccountData(PreparedQueryResult result, uint32 mask);

        void LoadTutorialsData(PreparedQueryResult result);
        void SendTutorialsData();
        void SaveTutorialsData(CharacterDatabaseTransaction trans);
        uint32 GetTutorialInt(uint8 index) const { return _tutorials[index]; }
        void SetTutorialInt(uint8 index, uint32 value)
        {
            if (_tutorials[index] != value)
            {
                _tutorials[index] = value;
                _tutorialsChanged |= TUTORIALS_FLAG_CHANGED;
            }
        }
        void LoadInstanceTimeRestrictions(PreparedQueryResult result);
        void SaveInstanceTimeRestrictions(CharacterDatabaseTransaction trans);
        bool UpdateAndCheckInstanceCount(uint32 instanceId);
        void AddInstanceEnterTime(uint32 instanceId, SystemTimePoint enterTime);
        void UpdateInstanceEnterTimes();

        // Mythic keystone deserter state, the data behind C_InstanceLeaver.IsPlayerLeaver().
        // This is NOT the classic LFG deserter aura (spell 71041) - the client keeps the two apart, and only
        // this one gates the mythic+ entries in the group finder (LFGList.lua:1382, :2965).
        void LoadChallengeModeHistory(PreparedQueryResult result);
        void SaveChallengeModeHistory() const;
        bool IsInstanceLeaver() const;
        void SetInstanceLeaver(bool apply);
        void SendInstanceLeaverState();
        void UpdateInstanceLeaverState();

        struct PlayerDataAccount
        {
            struct Element
            {
                uint32 Id;
                bool NeedSave;
                union
                {
                    float FloatValue;
                    int64 Int64Value;
                };
            };

            struct Flag
            {
                uint64 Value;
                bool NeedSave;
            };

            std::vector<Element> Elements;
            std::vector<Flag> Flags;
        };

        void LoadPlayerDataAccount(PreparedQueryResult const& elementsResult, PreparedQueryResult const& flagsResult);
        void SavePlayerDataAccount(LoginDatabaseTransaction const& transaction);

        void SetPlayerDataElementAccount(uint32 dataElementId, float value);
        void SetPlayerDataElementAccount(uint32 dataElementId, int64 value);
        void SetPlayerDataFlagAccount(uint32 dataFlagId, bool on);

        PlayerDataAccount const& GetPlayerDataAccount() const { return _playerDataAccount; }

        // Auction
        void SendAuctionHello(ObjectGuid guid, Unit const* unit);

        /**
         * @fn  void WorldSession::SendAuctionCommandResult(uint32 auctionId, uint32 action, uint32 errorCode, uint32 bagError = 0);
         *
         * @brief   Notifies the client of the result of his last auction operation. It is called when the player bids, creates, or deletes an auction
         *
         * @param   auctionId       The relevant auction id
         * @param   command         The action that was performed.
         * @param   errorCode       The resulting error code.
         * @param   bagResult       (Optional) InventoryResult.
         */
        void SendAuctionCommandResult(uint32 auctionId, AuctionCommand command, AuctionResult errorCode, Milliseconds delayForNextAction, InventoryResult bagResult = InventoryResult(0));
        void SendAuctionClosedNotification(AuctionPosting const* auction, float mailDelay, bool sold);
        void SendAuctionOwnerBidNotification(AuctionPosting const* auction);

        // Black Market
        void SendBlackMarketOpenResult(ObjectGuid guid, Creature* auctioneer);
        void SendBlackMarketBidOnItemResult(int32 result, int32 marketId, WorldPackets::Item::ItemInstance& item);
        void SendBlackMarketWonNotification(BlackMarketEntry const* entry, Item const* item);
        void SendBlackMarketOutbidNotification(BlackMarketTemplate const* templ);

        //Item Enchantment
        void SendEnchantmentLog(ObjectGuid owner, ObjectGuid caster, ObjectGuid itemGuid, uint32 itemId, uint32 enchantId, uint32 enchantSlot);
        void SendItemEnchantTimeUpdate(ObjectGuid Playerguid, ObjectGuid Itemguid, uint32 slot, uint32 Duration);

        //Taxi
        void SendTaxiStatus(ObjectGuid guid);
        void SendTaxiMenu(Creature* unit);
        bool SendLearnNewTaxiNode(Creature* unit);
        void SendDiscoverNewTaxiNode(uint32 nodeid);

        // Guild/Arena Team
        void SendPetitionShowList(ObjectGuid guid);

        void DoLootRelease(Loot* loot);
        void DoLootReleaseAll();

        // Account mute time
        bool CanSpeak() const;
        time_t m_muteTime;

        // Locales
        LocaleConstant GetSessionDbcLocale() const { return m_sessionDbcLocale; }
        LocaleConstant GetSessionDbLocaleIndex() const { return m_sessionDbLocaleIndex; }

        Minutes GetTimezoneOffset() const { return _timezoneOffset; }

        char const* GetTrinityString(uint32 entry) const;

        uint32 GetLatency() const { return m_latency; }
        void SetLatency(uint32 latency) { m_latency = latency; }

        std::atomic<time_t> m_timeOutTime;

        void ResetTimeOutTime(bool onlyActive);

        bool IsConnectionIdle() const;

        // Recruit-A-Friend Handling
        uint32 GetRecruiterId() const { return recruiterId; }
        bool IsARecruiter() const { return isRecruiter; }

        // Time Synchronisation
        void ResetTimeSync();
        void SendTimeSync();
        void SendTimeAdjustment(float timeScale);
        void RegisterTimeSync(uint32 counter);
        uint32 AdjustClientMovementTime(uint32 time) const;

        // SMSG_SUSPEND_COMMS - the sending half of the suspend/ack pair, and the ONLY place a suspend serial is
        // minted. Read the preconditions on WorldPackets::Auth::SuspendComms before adding a call: the client
        // answers a suspend sent to the wrong socket with CMSG_LOG_DISCONNECT(3) and closes it, and the socket
        // this is sent on must be the one being REPLACED, not the replacement.
        // This function has no caller today, and the reason is a property of this server, not of the packet:
        // retail sends it when a connection slot's replacement authenticates while the slot's old socket is still
        // open, and this server never gets into that state - it closes the old instance socket before any new
        // CONNECT_TO and never redirects the realm socket at all. The derivation and the lines that guarantee it
        // are above WorldPackets::Auth::SuspendComms; WorldSession::AddInstanceConnection carries the guard that
        // reports a violation. Decision O1 of unit conn_44_4C, which also records D2/D3 as NOT met for this
        // opcode rather than closing them.
        // The entry point exists anyway so that the serial CMSG_SUSPEND_COMMS_ACK echoes has exactly one issuer;
        // HandleSuspendCommsAck rejects every serial that did not come from here.
        void SendSuspendComms(ConnectionType connection);

        static constexpr uint32 SPECIAL_INIT_ACTIVE_MOVER_TIME_SYNC_COUNTER = 0xFFFFFFFF;
        static constexpr uint32 SPECIAL_RESUME_COMMS_TIME_SYNC_COUNTER      = 0xFFFFFFFE;
        // The counter CMSG_SUSPEND_COMMS_ACK is booked under. The serial from the wire is NEVER used as a key:
        // SendSuspendComms registers this constant, and HandleSuspendCommsAck only looks a sample up under it
        // after the wire serial matched the one we issued. Keying on the wire value instead would let the client
        // pick the key - it could consume the slot of a pending SMSG_TIME_SYNC_REQUEST, so the real
        // CMSG_TIME_SYNC_RESPONSE for that counter is silently dropped, and push a clock delta of its own choice
        // into _timeSyncClockDeltaQueue, which feeds AdjustClientMovementTime and the transport server time.
        static constexpr uint32 SPECIAL_SUSPEND_COMMS_TIME_SYNC_COUNTER     = 0xFFFFFFFD;

        // Client-reported frame rate statistics, fed from CMSG_LATENCY_REPORT. Session lifetime only - see
        // decision O3 of unit conn_44_4C: deliberately aggregated and NOT persisted, because the client sends
        // ~20000 measurement points per play session and there is no retention story for a table that size.
        struct ClientPerformanceStats
        {
            uint32 Reports = 0;             ///< number of accepted (deduplicated) reports
            uint32 Samples = 0;             ///< number of entries folded into the averages
            uint32 MinFrameRate = 0;
            uint32 MaxFrameRate = 0;
            uint32 LastFrameRate = 0;       ///< frame rate of the NEWEST entry that carried one, picked by
                                            ///< timestamp and not by write order - see the selection in
                                            ///< HandleLatencyReport. Assigned in iteration order this would name
                                            ///< the entry the sender appends itself, which is provably the oldest
                                            ///< of its block, so "last" would mean close to the opposite of what
                                            ///< it says.
            uint64 FrameRateSum = 0;
            uint64 LastTimestampMS = 0;     ///< client unix time in ms of the newest entry seen - EVERY entry,
                                            ///< including the ones with Frame == 0 that Samples excludes. It is
                                            ///< the client's own clock, so a value far from server time on an
                                            ///< otherwise healthy session is the one thing this aggregate shows
                                            ///< that the frame rates do not, and it would not be that if it hung
                                            ///< on the frame rates' own selection. Read out in ~WorldSession.
            uint64 LastFrameRateTimestampMS = 0;    ///< selection key for LastFrameRate: the timestamp of the
                                            ///< entry that supplied it. Deliberately NOT LastTimestampMS, even
                                            ///< though the two carry the same value over the whole corpus: that
                                            ///< one advances on every entry, including the Frame == 0 ones the
                                            ///< frame rate branch never sees, so sharing it would freeze the
                                            ///< frame rate for the rest of the session the first time an entry
                                            ///< without a frame rate happened to be the newest of its packet.
                                            ///< Unlike the members above this one is not reported; it only
                                            ///< carries the choice from one report to the next.
            uint32 AverageFrameRate() const { return Samples ? uint32(FrameRateSum / Samples) : 0u; }
        };

        ClientPerformanceStats const& GetClientPerformanceStats() const { return _clientPerformanceStats; }
        static constexpr uint32 SPECIAL_SUSPEND_COMMS_TIME_SYNC_COUNTER     = 0xFFFFFFFD;

        // Packets cooldown
        time_t GetCalendarEventCreationCooldown() const { return _calendarEventCreationCooldown; }
        void SetCalendarEventCreationCooldown(time_t cooldown) { _calendarEventCreationCooldown = cooldown; }

        // Battle Pets
        BattlePets::BattlePetMgr* GetBattlePetMgr() const { return _battlePetMgr.get(); }

        CollectionMgr* GetCollectionMgr() const { return _collectionMgr.get(); }

        // Account-wide Trader's Tender (currency 2032). The authoritative balance lives in the login DB
        // (battlenet_account_perks_tender), shared by every character of the bnet account; -1 means no row
        // has been loaded yet (first login since the account-wide wallet was introduced -> seed from the
        // loading character's existing per-character balance).
        int64 GetAccountPerksTender() const { return _accountPerksTender; }
        void StoreAccountPerksTender(uint32 amount);   // updates the session cache + persists to the login DB

        // The Trading Post interval (UTC month-start) for which the account last received its base monthly Tender
        // (Collector's Cache), used to grant it exactly once per period. Persisted alongside the balance.
        uint64 GetAccountPerksCacheGrantPeriod() const { return _accountPerksCacheGrantPeriod; }
        void SetAccountPerksCacheGrantPeriod(uint64 period) { _accountPerksCacheGrantPeriod = period; }

    public:                                                 // opcodes handlers

        void Handle_NULL(WorldPackets::Null& null);          // not used
        void Handle_EarlyProccess(WorldPackets::Null& null); // just mark packets processed in WorldSocket::ReadDataHandler
        void LogUnprocessedTail(WorldPacket const* packet);

        void HandleCharEnum(CharacterDatabaseQueryHolder const& holder);
        void HandleCharEnumOpcode(WorldPackets::Character::EnumCharacters& /*enumCharacters*/);
        void HandleCharUndeleteEnumOpcode(WorldPackets::Character::EnumCharacters& /*enumCharacters*/);
        void HandleCharDeleteOpcode(WorldPackets::Character::CharDelete& charDelete);
        void HandleSetupWarbandGroups(WorldPackets::Character::SetupWarbandGroups& setupWarbandGroups);
        void HandleCharCreateOpcode(WorldPackets::Character::CreateCharacter& charCreate);
        void HandlePlayerLoginOpcode(WorldPackets::Character::PlayerLogin& playerLogin);

        // Arathi Returning Player Experience: shared in-world entry point (Adventure Guide tile)
        // for CMSG_ENCOUNTER_JOURNAL_START_ARATHI_RPE. See CharacterHandler.cpp for the login-time
        // equivalent in HandlePlayerLogin(). Returns false (no-op) if the player is ineligible or
        // the map is unavailable.
        bool EnterArathiRpe(Player* player);
        void HandleEncounterJournalStartArathiRpe(WorldPackets::Character::EncounterJournalStartArathiRpe& encounterJournalStartArathiRpe);

        void SendConnectToInstance(WorldPackets::Auth::ConnectToSerial serial);
        void HandleContinuePlayerLogin();
        void AbortLogin(WorldPackets::Character::LoginFailureReason reason);
        void HandleLoadScreenOpcode(WorldPackets::Character::LoadingScreenNotify& loadingScreenNotify);
        void HandlePlayerLogin(LoginQueryHolder const& holder);
        void HandleCheckCharacterNameAvailability(WorldPackets::Character::CheckCharacterNameAvailability& checkCharacterNameAvailability);
        void HandleCharRenameOpcode(WorldPackets::Character::CharacterRenameRequest& request);
        void HandleCharRenameCallBack(std::shared_ptr<WorldPackets::Character::CharacterRenameInfo> renameInfo, PreparedQueryResult result);
        void HandleSetPlayerDeclinedNames(WorldPackets::Character::SetPlayerDeclinedNames& packet);
        void HandleAlterAppearance(WorldPackets::Character::AlterApperance& packet);
        void HandleCharCustomizeOpcode(WorldPackets::Character::CharCustomize& packet);
        void HandleCharCustomizeCallback(std::shared_ptr<WorldPackets::Character::CharCustomizeInfo> customizeInfo, PreparedQueryResult result);
        void HandleCharRaceOrFactionChangeOpcode(WorldPackets::Character::CharRaceOrFactionChange& packet);
        void HandleCharRaceOrFactionChangeCallback(std::shared_ptr<WorldPackets::Character::CharRaceOrFactionChangeInfo> factionChangeInfo, PreparedQueryResult result);
        void HandleRandomizeCharNameOpcode(WorldPackets::Character::GenerateRandomCharacterName& packet);
        void HandleReorderCharacters(WorldPackets::Character::ReorderCharacters& reorderChars);
        void HandleOpeningCinematic(WorldPackets::Misc::OpeningCinematic& packet);
        void HandleDoCountdown(WorldPackets::Misc::DoCountdown& doCountdown);
        void HandleGetRemainingGameTime(WorldPackets::Misc::GetRemainingGameTime& getRemainingGameTime);
        void HandleSetStopConversation(WorldPackets::Misc::SetStopConversation& setStopConversation);
        void HandleUnlearnSpecialization(WorldPackets::Talent::UnlearnSpecialization& unlearnSpecialization);
        void HandleGetUndeleteCooldownStatus(WorldPackets::Character::GetUndeleteCharacterCooldownStatus& /*getCooldown*/);
        void HandleUndeleteCooldownStatusCallback(PreparedQueryResult result);
        void HandleCharUndeleteOpcode(WorldPackets::Character::UndeleteCharacter& undeleteInfo);
        void HandleSavePersonalEmblem(WorldPackets::Character::SavePersonalEmblem const& savePersonalEmblem);
        void HandleNeutralPlayerSelectFaction(WorldPackets::Character::NeutralPlayerSelectFaction const& packet);
        bool MeetsChrCustomizationReq(ChrCustomizationReqEntry const* req, Races race, Classes playerClass,
            bool checkRequiredDependentChoices, Trinity::IteratorPair<UF::ChrCustomizationChoice const*> selectedChoices) const;
        bool ValidateAppearance(Races race, Classes playerClass, Gender gender,
            Trinity::IteratorPair<UF::ChrCustomizationChoice const*> customizations); // customizations must be sorted

        void SendCharCreate(ResponseCodes result, ObjectGuid const& guid = ObjectGuid::Empty);
        void SendCharDelete(ResponseCodes result);
        void SendCharRename(ResponseCodes result, WorldPackets::Character::CharacterRenameInfo const* renameInfo);
        void SendCharCustomize(ResponseCodes result, WorldPackets::Character::CharCustomizeInfo const* customizeInfo);
        void SendCharFactionChange(ResponseCodes result, WorldPackets::Character::CharRaceOrFactionChangeInfo const* factionChangeInfo);
        void SendSetPlayerDeclinedNamesResult(DeclinedNameResult result, ObjectGuid guid);
        void SendUndeleteCooldownStatusResponse(uint32 currentCooldown, uint32 maxCooldown);
        void SendUndeleteCharacterResponse(CharacterUndeleteResult result, WorldPackets::Character::CharacterUndeleteInfo const* undeleteInfo);

        // played time
        void HandlePlayedTime(WorldPackets::Character::RequestPlayedTime& packet);

        // cemetery/graveyard related
        void HandlePortGraveyard(WorldPackets::Misc::PortGraveyard& packet);
        void HandleRequestCemeteryList(WorldPackets::Misc::RequestCemeteryList& packet);

        // Inspect
        void HandleInspectOpcode(WorldPackets::Inspect::Inspect& inspect);
        void HandleQueryInspectAchievements(WorldPackets::Inspect::QueryInspectAchievements& inspect);

        void HandleMountSpecialAnimOpcode(WorldPackets::Misc::MountSpecial& mountSpecial);

        // repair
        void HandleRepairItemOpcode(WorldPackets::Item::RepairItem& packet);

        // Knockback
        void HandleMoveKnockBackAck(WorldPackets::Movement::MoveKnockBackAck& movementAck);

        void HandleMoveTeleportAck(WorldPackets::Movement::MoveTeleportAck& packet);
        void HandleForceSpeedChangeAck(WorldPackets::Movement::MovementSpeedAck& packet);
        void HandleSetAdvFlyingSpeedAck(WorldPackets::Movement::MovementSpeedAck& speedAck);
        void HandleSetAdvFlyingSpeedRangeAck(WorldPackets::Movement::MovementSpeedRangeAck& speedRangeAck);
        void HandleSetCollisionHeightAck(WorldPackets::Movement::MoveSetCollisionHeightAck& setCollisionHeightAck);

        // Movement forces
        void HandleMoveApplyMovementForceAck(WorldPackets::Movement::MoveApplyMovementForceAck& moveApplyMovementForceAck);
        void HandleMoveRemoveMovementForceAck(WorldPackets::Movement::MoveRemoveMovementForceAck& moveRemoveMovementForceAck);
        void HandleMoveSetModMovementForceMagnitudeAck(WorldPackets::Movement::MovementSpeedAck& setModMovementForceMagnitudeAck);

        // Inertia
        void HandleMoveApplyInertiaAck(WorldPackets::Movement::MoveApplyInertiaAck& moveApplyInertiaAck);
        void HandleMoveRemoveInertiaAck(WorldPackets::Movement::MoveRemoveInertiaAck& moveRemoveInertiaAck);
        // ClubFinder
        void HandleClubFinderPost(WorldPackets::ClubFinder::ClubFinderPost& clubFinderPost);
        void HandleClubFinderRequestSubscribedClubPostingIds(WorldPackets::ClubFinder::ClubFinderRequestSubscribedClubPostingIds& request);
        void HandleClubFinderRequestClubsData(WorldPackets::ClubFinder::ClubFinderRequestClubsData& request);
        void HandleClubFinderRequestClubsList(WorldPackets::ClubFinder::ClubFinderRequestClubsList& request);
        void HandleClubFinderRequestMembershipToClub(WorldPackets::ClubFinder::ClubFinderRequestMembershipToClub& request);
        void HandleClubFinderGetApplicantsList(WorldPackets::ClubFinder::ClubFinderGetApplicantsList& request);
        void HandleClubFinderRequestPendingClubsList(WorldPackets::ClubFinder::ClubFinderRequestPendingClubsList& request);
        void HandleClubFinderRespondToApplicant(WorldPackets::ClubFinder::ClubFinderRespondToApplicant& request);
        void HandleClubFinderApplicationResponse(WorldPackets::ClubFinder::ClubFinderApplicationResponse& request);
        void HandleClubFinderWhisperApplicantRequest(WorldPackets::ClubFinder::ClubFinderWhisperApplicantRequest& request);
        void SendClubFinderPendingApplications(uint8 type);
        // Commerce
        // Commerce Token
        void HandleAuctionableTokenSell(WorldPackets::Token::AuctionableTokenSell& auctionableTokenSell);
        void HandleAuctionableTokenSellAtMarketPrice(WorldPackets::Token::AuctionableTokenSellAtMarketPrice& auctionableTokenSellAtMarketPrice);
        void HandleCanRedeemTokenForBalance(WorldPackets::Token::CanRedeemTokenForBalance& canRedeemTokenForBalance);
        void HandleConsumableTokenBuy(WorldPackets::Token::ConsumableTokenBuy& consumableTokenBuy);
        void HandleConsumableTokenBuyAtMarketPrice(WorldPackets::Token::ConsumableTokenBuyAtMarketPrice& consumableTokenBuyAtMarketPrice);
        void HandleConsumableTokenCanVeteranBuy(WorldPackets::Token::ConsumableTokenCanVeteranBuy& consumableTokenCanVeteranBuy);
        void HandleConsumableTokenRedeem(WorldPackets::Token::ConsumableTokenRedeem& consumableTokenRedeem);
        void HandleConsumableTokenRedeemConfirmation(WorldPackets::Token::ConsumableTokenRedeemConfirmation& consumableTokenRedeemConfirmation);
        void SendGenerateSsoToken(uint32 clientToken);
        void SendCharacterEnum();
        // Commerce members
        bool IsCharacterShopBoosted(ObjectGuid::LowType characterGuid) const { return _shopBoostedCharacters.contains(characterGuid); }
        bool IsCharacterShopTrial(ObjectGuid::LowType characterGuid) const { return _shopTrialCharacters.contains(characterGuid); }
        bool _battlePayPurchaseInFlight = false;
        uint32 _lastBattlePayPurchaseMSTime = 0;
        uint32 _battlePayCatalogGeneration = 0;
        uint32 _battlePayPendingProductID = 0;
        uint32 _battlePayConfirmToken = 0;
        std::unordered_set<ObjectGuid::LowType> _shopBoostedCharacters;
        std::unordered_set<ObjectGuid::LowType> _shopTrialCharacters;
        bool _shopBoostAdvertised = false;
        void ApplyBattlePayCharacterBoost(CharacterDatabaseQueryHolder const& queryResult, ObjectGuid target,
            uint32 specializationId, uint64 distributionId, uint64 purchaseId, uint32 productId);
        void HandleCharacterUpgradeStart(WorldPackets::BattlePay::CharacterUpgradeStart& upgradeStart);
        void HandleUpdateVasPurchaseStates(WorldPackets::BattlePay::UpdateVasPurchaseStates& packet);
        void HandleVasGetServiceStatus(WorldPackets::BattlePay::VasGetServiceStatus& packet);
        void HandleCommerceTokenGetCount(WorldPackets::Token::CommerceTokenGetCount& commerceTokenGetCount);
        void SendCommerceTokenUpdate();
        int32 BattlePayCreateEntitlement(ShopProduct const& product, uint64 purchaseID);
        bool HasBattlePayCharacterBoost() const;
        std::vector<ShopEntitlement> _battlePayEntitlements;
        // BattlePay
        void HandleBattlePayGetProductList(WorldPackets::BattlePay::GetProductList& getProductList);
        void HandleBattlePayGetPurchaseList(WorldPackets::BattlePay::GetPurchaseList& getPurchaseList);
        void HandleBattlePayStartPurchase(WorldPackets::BattlePay::StartPurchase& startPurchase);
        void HandleBattlePayOpenCheckout(WorldPackets::BattlePay::OpenCheckout& openCheckout);
        void HandleCatalogShopLicenseGameDataRequest(WorldPackets::BattlePay::CatalogShopLicenseGameDataRequest& request);
        void HandleBattlePayConfirmPurchaseResponse(WorldPackets::BattlePay::ConfirmPurchaseResponse& confirmPurchaseResponse);
        void BattlePayProcessPurchase(uint32 productID);
        void SendBattlePayDistributionList();
        void SendBattlePayDeliveryNotifications(ShopProduct const& product, uint64 purchaseID);
        void HandleBattlePayDistributionAssignToTarget(WorldPackets::BattlePay::DistributionAssignToTarget& assign);
        void LoadBattlePayEntitlements(bool sendList);
        void SendBattlePayDistributionListNow();
        void SendBattlePayDistributionUpdate(ShopEntitlement const& entitlement);
        void SendBattlePayEntitlementSync();
        void RedeemBattlePayEntitlements();
        // Delves
        void HandleDelveTeleportOut(WorldPackets::Delves::DelveTeleportOut& delveTeleportOut);
        void HandleRequestPartyEligibilityForDelveTiers(WorldPackets::Delves::RequestPartyEligibilityForDelveTiers& requestPartyEligibilityForDelveTiers);
        void HandleSelectDelveEntranceTier(WorldPackets::Delves::SelectDelveEntranceTier& selectDelveEntranceTier);
        void HandleTieredEntranceOpen(WorldPackets::Delves::TieredEntranceOpen& tieredEntranceOpen);
        // WeeklyRewards
        void HandleRequestWeeklyRewards(WorldPackets::WeeklyRewards::RequestWeeklyRewards& packet);
        void HandleClaimWeeklyReward(WorldPackets::WeeklyRewards::ClaimWeeklyReward& packet);
        // Challenge Mode (Mythic+)
        void HandleRequestMythicPlusSeasonData(WorldPackets::ChallengeMode::RequestMythicPlusSeasonData& requestMythicPlusSeasonData);
        void HandleRequestMythicPlusAffixes(WorldPackets::ChallengeMode::RequestMythicPlusAffixes& requestMythicPlusAffixes);
        void HandleStartChallengeMode(WorldPackets::ChallengeMode::StartChallengeMode& startChallengeMode);
        void HandleResetChallengeMode(WorldPackets::ChallengeMode::ResetChallengeMode& resetChallengeMode);
        void HandleMythicPlusRequestMapStats(WorldPackets::ChallengeMode::MythicPlusRequestMapStats& request);
        bool HasHousingPlayerHouseEntity() const { return _housingPlayerHouseEntity != nullptr; }
        bool HasHousingNeighborhoodMirrorEntity() const { return _housingNeighborhoodMirrorEntity != nullptr; }
        HousingPlayerHouseEntity& GetHousingPlayerHouseEntity() const { return *_housingPlayerHouseEntity; }
        HousingNeighborhoodMirrorEntity& GetHousingNeighborhoodMirrorEntity() const { return *_housingNeighborhoodMirrorEntity; }
        std::unique_ptr<HousingPlayerHouseEntity> _housingPlayerHouseEntity;
        std::unique_ptr<HousingNeighborhoodMirrorEntity> _housingNeighborhoodMirrorEntity;
        // Housing
        void HandleHouseExteriorSetHousePosition(WorldPackets::Housing::HouseExteriorCommitPosition const& houseExteriorCommitPosition);
        void HandleHouseExteriorLock(WorldPackets::Housing::HouseExteriorLock const& houseExteriorLock);
        void HandleHouseInteriorLeaveHouse(WorldPackets::Housing::HouseInteriorLeaveHouse const& houseInteriorLeaveHouse);
        void HandleHousingDecorSetEditMode(WorldPackets::Housing::HousingDecorSetEditMode const& housingDecorSetEditMode);
        void HandleHousingDecorPlace(WorldPackets::Housing::HousingDecorPlace const& housingDecorPlace);
        void HandleHousingDecorMove(WorldPackets::Housing::HousingDecorMove const& housingDecorMove);
        void HandleHousingDecorRemove(WorldPackets::Housing::HousingDecorRemove const& housingDecorRemove);
        void HandleHousingDecorLock(WorldPackets::Housing::HousingDecorLock const& housingDecorLock);
        void HandleHousingDecorSetDyeSlots(WorldPackets::Housing::HousingDecorSetDyeSlots const& housingDecorSetDyeSlots);
        void HandleHousingDecorDeleteFromStorage(WorldPackets::Housing::HousingDecorDeleteFromStorage const& housingDecorDeleteFromStorage);
        void HandleHousingDecorRequestStorage(WorldPackets::Housing::HousingDecorRequestStorage const& housingDecorRequestStorage);
        void HandleHousingDecorRedeemDeferredDecor(WorldPackets::Housing::HousingDecorRedeemDeferredDecor const& housingDecorRedeemDeferredDecor);
        void HandleGetLastCatalogFetch(WorldPackets::Housing::GetLastCatalogFetch const& getLastCatalogFetch);
        void HandleUpdateLastCatalogFetch(WorldPackets::Housing::UpdateLastCatalogFetch const& updateLastCatalogFetch);
        void HandleHousingBlueprintRequestCollection(WorldPackets::Housing::HousingBlueprintRequestCollection const& packet);
        void HandleHousingBlueprintRequestContents(WorldPackets::Housing::HousingBlueprintRequestContents const& packet);
        void HandleHousingBlueprintExport(WorldPackets::Housing::HousingBlueprintExport const& packet);
        void HandleHousingBlueprintExportRoom(WorldPackets::Housing::HousingBlueprintExportRoom const& packet);
        void HandleHousingBlueprintRename(WorldPackets::Housing::HousingBlueprintRename const& packet);
        void HandleHousingBlueprintImport(WorldPackets::Housing::HousingBlueprintImport const& packet);
        void HandleHousingFixtureSetEditMode(WorldPackets::Housing::HousingFixtureSetEditMode const& housingFixtureSetEditMode);
        void HandleHousingFixtureSetCoreFixture(WorldPackets::Housing::HousingFixtureSetCoreFixture const& housingFixtureSetCoreFixture);
        void HandleHousingFixtureCreateFixture(WorldPackets::Housing::HousingFixtureCreateFixture const& housingFixtureCreateFixture);
        void HandleHousingFixtureDeleteFixture(WorldPackets::Housing::HousingFixtureDeleteFixture const& housingFixtureDeleteFixture);
        void HandleHousingFixtureSetHouseSize(WorldPackets::Housing::HousingFixtureSetHouseSize const& housingFixtureSetHouseSize);
        void HandleHousingFixtureSetHouseType(WorldPackets::Housing::HousingFixtureSetHouseType const& housingFixtureSetHouseType);
        void HandleHousingRoomSetLayoutEditMode(WorldPackets::Housing::HousingRoomSetLayoutEditMode const& housingRoomSetLayoutEditMode);
        void HandleHousingRoomAdd(WorldPackets::Housing::HousingRoomAdd const& housingRoomAdd);
        void HandleHousingRoomRemove(WorldPackets::Housing::HousingRoomRemove const& housingRoomRemove);
        void HandleHousingRoomRotate(WorldPackets::Housing::HousingRoomRotate const& housingRoomRotate);
        void HandleHousingRoomMoveRoom(WorldPackets::Housing::HousingRoomMoveRoom const& housingRoomMoveRoom);
        void HandleHousingRoomSetComponentTheme(WorldPackets::Housing::HousingRoomSetComponentTheme const& housingRoomSetComponentTheme);
        void HandleHousingRoomApplyComponentMaterials(WorldPackets::Housing::HousingRoomApplyComponentMaterials const& housingRoomApplyComponentMaterials);
        void HandleHousingRoomSetDoorType(WorldPackets::Housing::HousingRoomSetDoorType const& housingRoomSetDoorType);
        void HandleHousingRoomSetCeilingType(WorldPackets::Housing::HousingRoomSetCeilingType const& housingRoomSetCeilingType);
        void HandleHousingSvcsGuildCreateNeighborhood(WorldPackets::Housing::HousingSvcsGuildCreateNeighborhood const& housingSvcsGuildCreateNeighborhood);
        void HandleHousingSvcsNeighborhoodReservePlot(WorldPackets::Housing::HousingSvcsNeighborhoodReservePlot const& housingSvcsNeighborhoodReservePlot);
        void HandleHousingSvcsRelinquishHouse(WorldPackets::Housing::HousingSvcsRelinquishHouse const& housingSvcsRelinquishHouse);
        void HandleHousingSvcsUpdateHouseSettings(WorldPackets::Housing::HousingSvcsUpdateHouseSettings const& housingSvcsUpdateHouseSettings);
        void HandleHousingSvcsPlayerViewHousesByPlayer(WorldPackets::Housing::HousingSvcsPlayerViewHousesByPlayer const& housingSvcsPlayerViewHousesByPlayer);
        void HandleHousingSvcsPlayerViewHousesByBnetAccount(WorldPackets::Housing::HousingSvcsPlayerViewHousesByBnetAccount const& housingSvcsPlayerViewHousesByBnetAccount);
        void HandleHousingSvcsGetPlayerHousesInfo(WorldPackets::Housing::HousingSvcsGetPlayerHousesInfo const& housingSvcsGetPlayerHousesInfo);
        void HandleHousingSvcsTeleportToPlot(WorldPackets::Housing::HousingSvcsTeleportToPlot const& housingSvcsTeleportToPlot);
        void HandleHousingSvcsStartTutorial(WorldPackets::Housing::HousingSvcsStartTutorial const& housingSvcsStartTutorial);
        void HandleHousingSvcsAcceptNeighborhoodOwnership(WorldPackets::Housing::HousingSvcsAcceptNeighborhoodOwnership const& housingSvcsAcceptNeighborhoodOwnership);
        void HandleHousingSvcsRejectNeighborhoodOwnership(WorldPackets::Housing::HousingSvcsRejectNeighborhoodOwnership const& housingSvcsRejectNeighborhoodOwnership);
        void HandleHousingSvcsGetPotentialHouseOwners(WorldPackets::Housing::HousingSvcsGetPotentialHouseOwners const& housingSvcsGetPotentialHouseOwners);
        void HandleHousingSvcsGetHouseFinderInfo(WorldPackets::Housing::HousingSvcsGetHouseFinderInfo const& housingSvcsGetHouseFinderInfo);
        void HandleHousingSvcsGetHouseFinderNeighborhood(WorldPackets::Housing::HousingSvcsGetHouseFinderNeighborhood const& housingSvcsGetHouseFinderNeighborhood);
        void HandleHousingSvcsGetBnetFriendNeighborhoods(WorldPackets::Housing::HousingSvcsGetBnetFriendNeighborhoods const& housingSvcsGetBnetFriendNeighborhoods);
        void HandleHousingSvcsDeleteAllNeighborhoodInvites(WorldPackets::Housing::HousingSvcsDeleteAllNeighborhoodInvites const& housingSvcsDeleteAllNeighborhoodInvites);
        void HandleGetAllLicensedDecorQuantities(WorldPackets::Housing::GetAllLicensedDecorQuantities const& getAllLicensedDecorQuantities);
        void HandleGetDecorRefundList(WorldPackets::Housing::GetDecorRefundList const& getDecorRefundList);
        void HandleBulkRefund(WorldPackets::Housing::BulkRefund const& bulkRefund);
        void HandleHousingPhotoSharingCompleteAuthorization(WorldPackets::Housing::HousingPhotoSharingCompleteAuthorization const& packet);
        void HandleHousingPhotoSharingClearAuthorization(WorldPackets::Housing::HousingPhotoSharingClearAuthorization const& packet);
        void HandleHousingHouseStatus(WorldPackets::Housing::HousingHouseStatus const& housingHouseStatus);
        void HandleHousingGetCurrentHouseInfo(WorldPackets::Housing::HousingGetCurrentHouseInfo const& housingGetCurrentHouseInfo);
        void HandleHousingGetPlayerPermissions(WorldPackets::Housing::HousingGetPlayerPermissions const& housingGetPlayerPermissions);
        void HandleHousingResetKioskMode(WorldPackets::Housing::HousingResetKioskMode const& housingResetKioskMode);
        void HandleQueryNeighborhoodInfo(WorldPackets::Housing::QueryNeighborhoodInfo const& queryNeighborhoodInfo);
        void HandleInvitePlayerToNeighborhood(WorldPackets::Housing::InvitePlayerToNeighborhood const& invitePlayerToNeighborhood);
        void HandleGuildGetOthersOwnedHouses(WorldPackets::Housing::GuildGetOthersOwnedHouses const& guildGetOthersOwnedHouses);
        // Social
        void HandleSetAllowRecentAlliesSeeLocation(WorldPackets::Social::SetAllowRecentAlliesSeeLocation& packet);
        void HandleRecentAllyRequestData(WorldPackets::Social::RecentAllyRequestData& packet);
        void HandleRecentAllySetNote(WorldPackets::Social::RecentAllySetNote& packet);
        void HandleAcceptSocialContract(WorldPackets::Social::AcceptSocialContract& acceptSocialContract);
        // Contribution
        void HandleContributionContribute(WorldPackets::Contribution::ContributionContribute& contribute);
        void HandleContributionLastUpdateRequest(WorldPackets::Contribution::ContributionLastUpdateRequest& request);

        void HandleRepopRequest(WorldPackets::Misc::RepopRequest& packet);
        void HandleReportStuckInCombat(WorldPackets::Misc::ReportStuckInCombat& packet);
        void HandleSetPreferredCemetery(WorldPackets::Misc::SetPreferredCemetery& packet);
        void HandleAutostoreLootItemOpcode(WorldPackets::Loot::LootItem& packet);
        void HandleLootMoneyOpcode(WorldPackets::Loot::LootMoney& packet);
        void HandleLootOpcode(WorldPackets::Loot::LootUnit& packet);
        void HandleLootReleaseOpcode(WorldPackets::Loot::LootRelease& packet);
        void HandleLootMasterGiveOpcode(WorldPackets::Loot::MasterLootItem& masterLootItem);
        void HandleDoMasterLootRoll(WorldPackets::Loot::DoMasterLootRoll& packet);
        void HandleCancelMasterLootRoll(WorldPackets::Loot::CancelMasterLootRoll& packet);
        void HandleSetLootSpecialization(WorldPackets::Loot::SetLootSpecialization& packet);

        void HandleWhoOpcode(WorldPackets::Who::WhoRequestPkt& whoRequest);
        void HandleLogoutRequestOpcode(WorldPackets::Character::LogoutRequest& logoutRequest);
        void HandleLogoutCancelOpcode(WorldPackets::Character::LogoutCancel& logoutCancel);

        // GM Ticket opcodes
        void HandleGMTicketGetCaseStatusOpcode(WorldPackets::Ticket::GMTicketGetCaseStatus& packet);
        void HandleGMTicketSystemStatusOpcode(WorldPackets::Ticket::GMTicketGetSystemStatus& packet);
        void HandleSubmitUserFeedback(WorldPackets::Ticket::SubmitUserFeedback& userFeedback);
        void HandleSupportTicketSubmitComplaint(WorldPackets::Ticket::SupportTicketSubmitComplaint& packet);
        void HandleBugReportOpcode(WorldPackets::Ticket::BugReport& bugReport);
        void HandleComplaint(WorldPackets::Ticket::Complaint& packet);

        void HandleTogglePvP(WorldPackets::Misc::TogglePvP& packet);
        void HandleSetPvP(WorldPackets::Misc::SetPvP& packet);
        void HandleSetWarMode(WorldPackets::Misc::SetWarMode& packet);

        void HandleSetSelectionOpcode(WorldPackets::Misc::SetSelection& packet);
        void HandleStandStateChangeOpcode(WorldPackets::Misc::StandStateChange& packet);
        void HandleEmoteOpcode(WorldPackets::Chat::EmoteClient& packet);

        // Social
        void HandleContactListOpcode(WorldPackets::Social::SendContactList& packet);
        void HandleAddFriendOpcode(WorldPackets::Social::AddFriend& packet);
        void HandleDelFriendOpcode(WorldPackets::Social::DelFriend& packet);
        void HandleAddIgnoreOpcode(WorldPackets::Social::AddIgnore& packet);
        void HandleDelIgnoreOpcode(WorldPackets::Social::DelIgnore& packet);
        void HandleSetContactNotesOpcode(WorldPackets::Social::SetContactNotes& packet);

        void HandleAreaTriggerOpcode(WorldPackets::AreaTrigger::AreaTrigger& packet);
        void HandleUpdateAreaTriggerVisual(WorldPackets::AreaTrigger::UpdateAreaTriggerVisual const& updateAreaTriggerVisual);

        void HandleSetFactionAtWar(WorldPackets::Character::SetFactionAtWar& packet);
        void HandleSetFactionNotAtWar(WorldPackets::Character::SetFactionNotAtWar& packet);
        void HandleSetWatchedFactionOpcode(WorldPackets::Character::SetWatchedFaction& packet);
        void HandleSetFactionInactiveOpcode(WorldPackets::Character::SetFactionInactive& packet);

        void HandleUpdateAccountData(WorldPackets::ClientConfig::UserClientUpdateAccountData& packet);
        void HandleRequestAccountData(WorldPackets::ClientConfig::RequestAccountData& request);
        void HandleSetAdvancedCombatLogging(WorldPackets::ClientConfig::SetAdvancedCombatLogging& setAdvancedCombatLogging);
        void HandleSetActionButtonOpcode(WorldPackets::Spells::SetActionButton& packet);

        void HandleGameObjectUseOpcode(WorldPackets::GameObject::GameObjUse& packet);
        void HandleGameobjectReportUse(WorldPackets::GameObject::GameObjReportUse& packet);

        void HandleQueryPlayerNames(WorldPackets::Query::QueryPlayerNames& queryPlayerNames);
        void HandleQueryPlayerNameByCommunityId(WorldPackets::Query::QueryPlayerNameByCommunityId& queryPlayerNameByCommunityId);
        void HandleQueryPlayerNamesForCommunity(WorldPackets::Query::QueryPlayerNamesForCommunity& queryPlayerNamesForCommunity);
        void HandleQueryTimeOpcode(WorldPackets::Query::QueryTime& queryTime);
        void HandleCreatureQuery(WorldPackets::Query::QueryCreature& packet);
        void HandleGameObjectQueryOpcode(WorldPackets::Query::QueryGameObject& packet);

        void HandleDBQueryBulk(WorldPackets::Hotfix::DBQueryBulk& dbQuery);
        void HandleHotfixRequest(WorldPackets::Hotfix::HotfixRequest& hotfixQuery);
        void BuildHotfixRecords(std::span<int32 const> hotfixIds, std::vector<WorldPackets::Hotfix::HotfixData>& records, ByteBuffer& content) const;
        void SendHotfixMessage(std::span<int32 const> hotfixIds);

        void HandleMoveWorldportAckOpcode(WorldPackets::Movement::WorldPortResponse& packet);
        void HandleMoveWorldportAck();                // for server-side calls
        void HandleSuspendTokenResponse(WorldPackets::Movement::SuspendTokenResponse& suspendTokenResponse);

        // Validates that correct unit is moved, coords are in valid range and movement flags
        bool ValidateMovementInfo(Unit const* mover, MovementInfo* mi) const;

        void HandleMovementOpcodes(WorldPackets::Movement::ClientPlayerMovement& packet);
        bool HandleMovementOpcode(OpcodeClient opcode, MovementInfo& movementInfo);
        void HandleSetActiveMoverOpcode(WorldPackets::Movement::SetActiveMover& packet);
        void HandleMoveDismissVehicle(WorldPackets::Vehicle::MoveDismissVehicle& moveDismissVehicle);
        void HandleRequestVehiclePrevSeat(WorldPackets::Vehicle::RequestVehiclePrevSeat& requestVehiclePrevSeat);
        void HandleRequestVehicleNextSeat(WorldPackets::Vehicle::RequestVehicleNextSeat& requestVehicleNextSeat);
        void HandleMoveChangeVehicleSeats(WorldPackets::Vehicle::MoveChangeVehicleSeats& moveChangeVehicleSeats);
        void HandleRequestVehicleSwitchSeat(WorldPackets::Vehicle::RequestVehicleSwitchSeat& requestVehicleSwitchSeat);
        void HandleRideVehicleInteract(WorldPackets::Vehicle::RideVehicleInteract& rideVehicleInteract);
        void HandleEjectPassenger(WorldPackets::Vehicle::EjectPassenger& ejectPassenger);
        void HandleRequestVehicleExit(WorldPackets::Vehicle::RequestVehicleExit& requestVehicleExit);
        void HandleMoveSetVehicleRecAck(WorldPackets::Vehicle::MoveSetVehicleRecIdAck& setVehicleRecIdAck);
        void HandleMoveTimeSkippedOpcode(WorldPackets::Movement::MoveTimeSkipped& moveTimeSkipped);
        void HandleMovementAckMessage(WorldPackets::Movement::MovementAckMessage& movementAck);
        void HandleMoveInitActiveMoverComplete(WorldPackets::Movement::MoveInitActiveMoverComplete const& moveInitActiveMoverComplete);
        void HandleMoveInitialObjectUpdateCompleteAck(WorldPackets::Movement::MovementAckMessage& initialObjectUpdateCompleteAck);
        void HandleMoveGravityModifierChangeAck(WorldPackets::Movement::MovementSpeedAck& gravityModifierAck);
        void HandleMoveRemoveMovementForces(WorldPackets::Movement::ClientPlayerMovement& removeMovementForces);
        void HandleMoveSetTurnRateCheat(WorldPackets::Movement::MoveSetTurnRateCheat& moveSetTurnRateCheat);

        void HandleRequestRaidInfoOpcode(WorldPackets::Party::RequestRaidInfo& packet);

        void HandlePartyInviteOpcode(WorldPackets::Party::PartyInviteClient& packet);
        void HandlePartyInviteResponseOpcode(WorldPackets::Party::PartyInviteResponse& packet);
        void HandleQuickJoinRespondToInvite(WorldPackets::Party::QuickJoinRespondToInvite& packet);
        void HandlePartyUninviteOpcode(WorldPackets::Party::PartyUninvite& packet);
        void HandleSetPartyLeaderOpcode(WorldPackets::Party::SetPartyLeader& packet);
        void HandleSetRoleOpcode(WorldPackets::Party::SetRole& packet);
        void HandleLeaveGroupOpcode(WorldPackets::Party::LeaveGroup& packet);
        void HandleOptOutOfLootOpcode(WorldPackets::Party::OptOutOfLoot& packet);
        void HandleSetLootMethodOpcode(WorldPackets::Party::SetLootMethod& packet);
        void HandleLootRoll(WorldPackets::Loot::LootRoll& packet);
        void HandleRequestPartyMemberStatsOpcode(WorldPackets::Party::RequestPartyMemberStats& packet);
        void HandleUpdateRaidTargetOpcode(WorldPackets::Party::UpdateRaidTarget& packet);
        void HandleDoReadyCheckOpcode(WorldPackets::Party::DoReadyCheck& packet);
        void HandleReadyCheckResponseOpcode(WorldPackets::Party::ReadyCheckResponseClient& packet);
        void HandleConvertRaidOpcode(WorldPackets::Party::ConvertRaid& packet);
        void HandleRequestPartyJoinUpdates(WorldPackets::Party::RequestPartyJoinUpdates& packet);
        void HandleChangeSubGroupOpcode(WorldPackets::Party::ChangeSubGroup& packet);
        void HandleSwapSubGroupsOpcode(WorldPackets::Party::SwapSubGroups& packet);
        void HandleSetAssistantLeaderOpcode(WorldPackets::Party::SetAssistantLeader& packet);
        void HandleSetPartyAssignment(WorldPackets::Party::SetPartyAssignment& packet);
        void HandleInitiateRolePoll(WorldPackets::Party::InitiateRolePoll& packet);
        void HandleSetEveryoneIsAssistant(WorldPackets::Party::SetEveryoneIsAssistant& packet);
        void HandleClearRaidMarker(WorldPackets::Party::ClearRaidMarker& packet);
        void HandleSetRestrictPingsToAssistants(WorldPackets::Party::SetRestrictPingsToAssistants const& setRestrictPingsToAssistants);
        void HandleSendPingUnit(WorldPackets::Party::SendPingUnit const& pingUnit);
        void HandleSendPingWorldPoint(WorldPackets::Party::SendPingWorldPoint const& pingWorldPoint);
        void HandleSendPingCooldown(WorldPackets::Party::SendPingCooldown const& pingCooldown);

        void HandlePetitionBuy(WorldPackets::Petition::PetitionBuy& packet);
        void HandlePetitionShowSignatures(WorldPackets::Petition::PetitionShowSignatures& packet);
        void SendPetitionSigns(Petition const* petition, Player* sendTo);
        void HandleQueryPetition(WorldPackets::Petition::QueryPetition& packet);
        void HandlePetitionRenameGuild(WorldPackets::Petition::PetitionRenameGuild& packet);
        void HandleSignPetition(WorldPackets::Petition::SignPetition& packet);
        void HandleDeclinePetition(WorldPackets::Petition::DeclinePetition& packet);
        void HandleOfferPetition(WorldPackets::Petition::OfferPetition& packet);
        void HandleTurnInPetition(WorldPackets::Petition::TurnInPetition& packet);

        void HandleGuildQueryOpcode(WorldPackets::Guild::QueryGuildInfo& query);
        void HandleGuildQueryRecipes(WorldPackets::Guild::GuildQueryRecipes& packet);
        void HandleGuildQueryMemberRecipes(WorldPackets::Guild::GuildQueryMemberRecipes& packet);
        void HandleGuildQueryMembersForRecipe(WorldPackets::Guild::GuildQueryMembersForRecipe& packet);
        void HandleGuildInviteByName(WorldPackets::Guild::GuildInviteByName& packet);
        void HandleGuildOfficerRemoveMember(WorldPackets::Guild::GuildOfficerRemoveMember& packet);
        void HandleGuildAcceptInvite(WorldPackets::Guild::AcceptGuildInvite& invite);
        void HandleGuildDeclineInvitation(WorldPackets::Guild::GuildDeclineInvitation& decline);
        void HandleGuildEventLogQuery(WorldPackets::Guild::GuildEventLogQuery& packet);
        void HandleGuildGetRoster(WorldPackets::Guild::GuildGetRoster& packet);
        void HandleRequestGuildRewardsList(WorldPackets::Guild::RequestGuildRewardsList& packet);
        void HandleGuildPromoteMember(WorldPackets::Guild::GuildPromoteMember& promote);
        void HandleGuildDemoteMember(WorldPackets::Guild::GuildDemoteMember& demote);
        void HandleGuildAssignRank(WorldPackets::Guild::GuildAssignMemberRank& packet);
        void HandleGuildLeave(WorldPackets::Guild::GuildLeave& leave);
        void HandleGuildDelete(WorldPackets::Guild::GuildDelete& packet);
        void HandleGuildReplaceGuildMaster(WorldPackets::Guild::GuildReplaceGuildMaster& replaceGuildMaster);
        void HandleGuildSetAchievementTracking(WorldPackets::Guild::GuildSetAchievementTracking& packet);
        void HandleGuildGetAchievementMembers(WorldPackets::Achievement::GuildGetAchievementMembers& getAchievementMembers);
        void HandleGuildChangeNameRequest(WorldPackets::Guild::GuildChangeNameRequest& packet);

        void HandlePerksProgramStatusRequest(WorldPackets::PerksProgram::PerksProgramStatusRequest& packet);
        void HandlePerksProgramGetRecentPurchases(WorldPackets::PerksProgram::PerksProgramGetRecentPurchases& packet);
        void HandlePerksProgramRequestPurchase(WorldPackets::PerksProgram::PerksProgramRequestPurchase& packet);
        void HandlePerksProgramRequestRefund(WorldPackets::PerksProgram::PerksProgramRequestRefund& packet);
        void HandlePerksProgramSetFrozenVendorItem(WorldPackets::PerksProgram::PerksProgramSetFrozenVendorItem& packet);
        void HandlePerksProgramRequestCartCheckout(WorldPackets::PerksProgram::PerksProgramRequestCartCheckout& packet);
        void HandlePerksProgramItemsRefreshed(WorldPackets::PerksProgram::PerksProgramItemsRefreshed& packet);
        void HandlePerksProgramRequestPendingRewards(WorldPackets::PerksProgram::PerksProgramRequestPendingRewards& packet);
        void SendPerksProgramActivityUpdate();
        void SendPerksAnimToggleKillSwitch();
        void HandleGuildSetGuildMaster(WorldPackets::Guild::GuildSetGuildMaster& packet);
        void HandleGuildUpdateMotdText(WorldPackets::Guild::GuildUpdateMotdText& packet);
        void HandleGuildNewsUpdateSticky(WorldPackets::Guild::GuildNewsUpdateSticky& packet);
        void HandleGuildSetMemberNote(WorldPackets::Guild::GuildSetMemberNote& packet);
        void HandleGuildGetRanks(WorldPackets::Guild::GuildGetRanks& packet);
        void HandleGuildQueryNews(WorldPackets::Guild::GuildQueryNews& newsQuery);
        void HandleGuildSetRankPermissions(WorldPackets::Guild::GuildSetRankPermissions& packet);
        void HandleGuildAddRank(WorldPackets::Guild::GuildAddRank& packet);
        void HandleGuildDeleteRank(WorldPackets::Guild::GuildDeleteRank& packet);
        void HandleGuildShiftRank(WorldPackets::Guild::GuildShiftRank& shiftRank);
        void HandleGuildUpdateInfoText(WorldPackets::Guild::GuildUpdateInfoText& packet);
        void HandleSaveGuildEmblem(WorldPackets::Guild::SaveGuildEmblem& packet);
        void HandleGuildRequestPartyState(WorldPackets::Guild::RequestGuildPartyState& packet);
        void HandleGuildChallengeUpdateRequest(WorldPackets::Guild::GuildChallengeUpdateRequest& packet);
        void HandleDeclineGuildInvites(WorldPackets::Guild::DeclineGuildInvites& packet);

        void HandleDeclineNeighborhoodInvites(WorldPackets::Housing::DeclineNeighborhoodInvites const& declineNeighborhoodInvites);

        void HandleEnableTaxiNodeOpcode(WorldPackets::Taxi::EnableTaxiNode& enableTaxiNode);
        void HandleTaxiNodeStatusQueryOpcode(WorldPackets::Taxi::TaxiNodeStatusQuery& taxiNodeStatusQuery);
        void HandleTaxiQueryAvailableNodesOpcode(WorldPackets::Taxi::TaxiQueryAvailableNodes& taxiQueryAvailableNodes);
        void HandleActivateTaxiOpcode(WorldPackets::Taxi::ActivateTaxi& activateTaxi);
        void HandleMoveSplineDoneOpcode(WorldPackets::Movement::MoveSplineDone& moveSplineDone);
        void SendActivateTaxiReply(ActivateTaxiReply reply);
        void HandleTaxiRequestEarlyLanding(WorldPackets::Taxi::TaxiRequestEarlyLanding& taxiRequestEarlyLanding);

        void HandleTabardVendorActivateOpcode(WorldPackets::NPC::TabardVendorActivate const& tabardVendorActivate);
        void HandleBankerActivateOpcode(WorldPackets::Bank::BankerActivate const& bankerActivate);
        void HandleTrainerListOpcode(WorldPackets::NPC::Hello& packet);
        void HandleTrainerBuySpellOpcode(WorldPackets::NPC::TrainerBuySpell& packet);
        void HandlePetitionShowList(WorldPackets::Petition::PetitionShowList& packet);
        void HandleGossipHelloOpcode(WorldPackets::NPC::Hello& packet);
        void HandleGossipRefreshOptions(WorldPackets::NPC::GossipRefreshOptions& packet);
        void HandleGossipSelectOptionOpcode(WorldPackets::NPC::GossipSelectOption& packet);
        void HandleSpiritHealerActivate(WorldPackets::NPC::SpiritHealerActivate& packet);
        void HandleNpcTextQueryOpcode(WorldPackets::Query::QueryNPCText& packet);
        void HandleBinderActivateOpcode(WorldPackets::NPC::Hello& packet);
        void HandleRequestStabledPets(WorldPackets::NPC::RequestStabledPets& packet);
        void HandleSetPetSlot(WorldPackets::NPC::SetPetSlot& setPetSlot);

        void HandleCanDuel(WorldPackets::Duel::CanDuel& packet);
        void HandleDuelResponseOpcode(WorldPackets::Duel::DuelResponse& duelResponse);
        void HandleDuelAccepted(ObjectGuid arbiterGuid);
        void HandleDuelCancelled();

        void HandleAcceptTradeOpcode(WorldPackets::Trade::AcceptTrade& acceptTrade);
        void HandleBeginTradeOpcode(WorldPackets::Trade::BeginTrade& beginTrade);
        void HandleBusyTradeOpcode(WorldPackets::Trade::BusyTrade& busyTrade);
        void HandleCancelTradeOpcode(WorldPackets::Trade::CancelTrade& cancelTrade);
        void HandleClearTradeItemOpcode(WorldPackets::Trade::ClearTradeItem& clearTradeItem);
        void HandleIgnoreTradeOpcode(WorldPackets::Trade::IgnoreTrade& ignoreTrade);
        void HandleInitiateTradeOpcode(WorldPackets::Trade::InitiateTrade& initiateTrade);
        void HandleSetTradeCurrencyOpcode(WorldPackets::Trade::SetTradeCurrency& setTradeCurrency);
        void HandleSetTradeGoldOpcode(WorldPackets::Trade::SetTradeGold& setTradeGold);
        void HandleSetTradeItemOpcode(WorldPackets::Trade::SetTradeItem& setTradeItem);
        void HandleUnacceptTradeOpcode(WorldPackets::Trade::UnacceptTrade& unacceptTrade);

        void HandleAuctionBrowseQuery(WorldPackets::AuctionHouse::AuctionBrowseQuery& browseQuery);
        void HandleAuctionCancelCommoditiesPurchase(WorldPackets::AuctionHouse::AuctionCancelCommoditiesPurchase& cancelCommoditiesPurchase);
        void HandleAuctionConfirmCommoditiesPurchase(WorldPackets::AuctionHouse::AuctionConfirmCommoditiesPurchase& confirmCommoditiesPurchase);
        void HandleAuctionGetCommodityQuote(WorldPackets::AuctionHouse::AuctionGetCommodityQuote& startCommoditiesPurchase);
        void HandleAuctionHelloOpcode(WorldPackets::AuctionHouse::AuctionHelloRequest& hello);
        void HandleAuctionListBiddedItems(WorldPackets::AuctionHouse::AuctionListBiddedItems& listBiddedItems);
        void HandleAuctionListBucketsByBucketKeys(WorldPackets::AuctionHouse::AuctionListBucketsByBucketKeys& listBucketsByBucketKeys);
        void HandleAuctionListItemsByBucketKey(WorldPackets::AuctionHouse::AuctionListItemsByBucketKey& listItemsByBucketKey);
        void HandleAuctionListItemsByItemID(WorldPackets::AuctionHouse::AuctionListItemsByItemID& listItemsByItemID);
        void HandleAuctionListOwnedItems(WorldPackets::AuctionHouse::AuctionListOwnedItems& listOwnedItems);
        void HandleAuctionPlaceBid(WorldPackets::AuctionHouse::AuctionPlaceBid& placeBid);
        void HandleAuctionRemoveItem(WorldPackets::AuctionHouse::AuctionRemoveItem& removeItem);
        void HandleAuctionReplicateItems(WorldPackets::AuctionHouse::AuctionReplicateItems& replicateItems);
        void SendAuctionFavoriteList();
        void HandleAuctionSellCommodity(WorldPackets::AuctionHouse::AuctionSellCommodity& sellCommodity);
        void HandleAuctionSellItem(WorldPackets::AuctionHouse::AuctionSellItem& sellItem);
        void HandleAuctionSetFavoriteItem(WorldPackets::AuctionHouse::AuctionSetFavoriteItem& setFavoriteItem);

        // Bank
        void HandleAutoBankItemOpcode(WorldPackets::Bank::AutoBankItem& packet);
        void HandleAutoStoreBankItemOpcode(WorldPackets::Bank::AutoStoreBankItem& packet);
        void HandleBuyBankTab(WorldPackets::Bank::BuyBankTab const& buyBankTab);
        void HandleUpdateBankTabSettings(WorldPackets::Bank::UpdateBankTabSettings const& updateBankTabSettings);
        void HandleAutoDepositCharacterBank(WorldPackets::Bank::AutoDepositCharacterBank const& autoDepositCharacterBank);
        void HandleAutoDepositAccountBank(WorldPackets::Bank::AutoDepositAccountBank const& autoDepositAccountBank);
        void HandleAccountBankDepositMoney(WorldPackets::Bank::AccountBankDepositMoney const& accountBankDepositMoney);
        void HandleAccountBankWithdrawMoney(WorldPackets::Bank::AccountBankWithdrawMoney const& accountBankWithdrawMoney);

        // Black Market
        void HandleBlackMarketOpen(WorldPackets::BlackMarket::BlackMarketOpen& blackMarketOpen);
        void HandleBlackMarketRequestItems(WorldPackets::BlackMarket::BlackMarketRequestItems& blackMarketRequestItems);
        void HandleBlackMarketBidOnItem(WorldPackets::BlackMarket::BlackMarketBidOnItem& blackMarketBidOnItem);

        void HandleGetMailList(WorldPackets::Mail::MailGetList& getList);
        void HandleSendMail(WorldPackets::Mail::SendMail& sendMail);
        void HandleMailTakeMoney(WorldPackets::Mail::MailTakeMoney& takeMoney);
        void HandleMailTakeItem(WorldPackets::Mail::MailTakeItem& takeItem);
        void HandleMailMarkAsRead(WorldPackets::Mail::MailMarkAsRead& markAsRead);
        void HandleMailReturnToSender(WorldPackets::Mail::MailReturnToSender& returnToSender);
        void HandleMailDelete(WorldPackets::Mail::MailDelete& mailDelete);
        void HandleItemTextQuery(WorldPackets::Query::ItemTextQuery& itemTextQuery);
        void HandleMailCreateTextItem(WorldPackets::Mail::MailCreateTextItem& createTextItem);
        void HandleQueryNextMailTime(WorldPackets::Mail::MailQueryNextMailTime& queryNextMailTime);
        void HandleGetRegionwideCharacterRestrictionAndMailData(WorldPackets::Mail::GetRegionwideCharacterRestrictionAndMailData& getRegionwideData);

        void HandleSplitItemOpcode(WorldPackets::Item::SplitItem& splitItem);
        void HandleSwapInvItemOpcode(WorldPackets::Item::SwapInvItem& swapInvItem);
        void HandleDestroyItemOpcode(WorldPackets::Item::DestroyItem& destroyItem);
        void HandleAutoEquipItemOpcode(WorldPackets::Item::AutoEquipItem& autoEquipItem);
        void HandleSellItemOpcode(WorldPackets::Item::SellItem const& sellItem);
        void HandleSellAllJunkItems(WorldPackets::Item::SellAllJunkItems const& sellAllJunkItems);
        void HandlePerformItemInteraction(WorldPackets::Item::PerformItemInteraction& performItemInteraction);
        void HandleBuyItemOpcode(WorldPackets::Item::BuyItem& packet);
        void HandleListInventoryOpcode(WorldPackets::NPC::Hello& packet);
        void HandleAutoStoreBagItemOpcode(WorldPackets::Item::AutoStoreBagItem& packet);
        void HandleReadItem(WorldPackets::Item::ReadItem& readItem);
        void HandleAutoEquipItemSlotOpcode(WorldPackets::Item::AutoEquipItemSlot& autoEquipItemSlot);
        void HandleSwapItem(WorldPackets::Item::SwapItem& swapItem);
        void HandleBuybackItem(WorldPackets::Item::BuyBackItem& packet);
        void HandleWrapItem(WorldPackets::Item::WrapItem& packet);
        void HandleUseCritterItem(WorldPackets::Item::UseCritterItem& packet);
        void HandleChangeBagSlotFlag(WorldPackets::Item::ChangeBagSlotFlag const& changeBagSlotFlag);
        void HandleSetBackpackAutosortDisabled(WorldPackets::Item::SetBackpackAutosortDisabled const& setBackpackAutosortDisabled);
        void HandleSetSortBagsRightToLeft(WorldPackets::Item::SetSortBagsRightToLeft const& setSortBagsRightToLeft);
        void HandleSetInsertItemsLeftToRight(WorldPackets::Item::SetInsertItemsLeftToRight const& setInsertItemsLeftToRight);
        void HandleSetBackpackSellJunkDisabled(WorldPackets::Item::SetBackpackSellJunkDisabled const& setBackpackSellJunkDisabled);
        void HandleSetBankAutosortDisabled(WorldPackets::Item::SetBankAutosortDisabled const& setBankAutosortDisabled);
        void SendOpenContainer(ObjectGuid containerGuid);

        void HandleAttackSwingOpcode(WorldPackets::Combat::AttackSwing& packet);
        void HandleAttackStopOpcode(WorldPackets::Combat::AttackStop& packet);
        void HandleSetSheathedOpcode(WorldPackets::Combat::SetSheathed& packet);

        void HandleUseItemOpcode(WorldPackets::Spells::UseItem& packet);
        void HandleOpenItemOpcode(WorldPackets::Spells::OpenItem& packet);
        void HandleOpenWrappedItemCallback(uint16 pos, ObjectGuid itemGuid, PreparedQueryResult result);
        void HandleCastSpellOpcode(WorldPackets::Spells::CastSpell& castRequest);
        void HandleCancelCastOpcode(WorldPackets::Spells::CancelCast& packet);
        void HandleCancelAuraOpcode(WorldPackets::Spells::CancelAura& cancelAura);
        void HandleCancelGrowthAuraOpcode(WorldPackets::Spells::CancelGrowthAura& cancelGrowthAura);
        void HandleCancelMountAuraOpcode(WorldPackets::Spells::CancelMountAura& cancelMountAura);
        void HandleCancelModSpeedNoControlAuras(WorldPackets::Spells::CancelModSpeedNoControlAuras& cancelModSpeedNoControlAuras);
        void HandleCancelAutoRepeatSpellOpcode(WorldPackets::Spells::CancelAutoRepeatSpell& cancelAutoRepeatSpell);
        void HandleCancelQueuedSpellOpcode(WorldPackets::Spells::CancelQueuedSpell& cancelQueuedSpell);
        void HandleCancelChanneling(WorldPackets::Spells::CancelChannelling& cancelChanneling);
        void HandleSetEmpowerMinHoldStagePercent(WorldPackets::Spells::SetEmpowerMinHoldStagePercent const& setEmpowerMinHoldStagePercent);
        void HandleSpellEmpowerRelease(WorldPackets::Spells::SpellEmpowerRelease const& spellEmpowerRelease);
        void HandleSpellEmpowerRestart(WorldPackets::Spells::SpellEmpowerRestart const& spellEmpowerRestart);
        void HandleMissileTrajectoryCollision(WorldPackets::Spells::MissileTrajectoryCollision& packet);
        void HandleUpdateMissileTrajectory(WorldPackets::Spells::UpdateMissileTrajectory& packet);
        void HandleUpdateAuraVisual(WorldPackets::Spells::UpdateAuraVisual const& updateAuraVisual);

        void HandleLearnPvpTalentsOpcode(WorldPackets::Talent::LearnPvpTalents& packet);
        void HandleLearnTalentsOpcode(WorldPackets::Talent::LearnTalents& packet);
        void HandleConfirmRespecWipeOpcode(WorldPackets::Talent::ConfirmRespecWipe& confirmRespecWipe);
        void HandleUnlearnSkillOpcode(WorldPackets::Spells::UnlearnSkill& packet);
        void HandleTradeSkillSetFavorite(WorldPackets::Spells::TradeSkillSetFavorite const& tradeSkillSetFavorite);
        void HandleOpenTradeSkillNpc(WorldPackets::Spells::OpenTradeSkillNpc const& packet);

        void HandleTraitsCommitConfig(WorldPackets::Traits::TraitsCommitConfig const& traitsCommitConfig);
        void HandleClassTalentsRequestNewConfig(WorldPackets::Traits::ClassTalentsRequestNewConfig& classTalentsRequestNewConfig);
        void HandleClassTalentsRenameConfig(WorldPackets::Traits::ClassTalentsRenameConfig& classTalentsRenameConfig);
        void HandleClassTalentsDeleteConfig(WorldPackets::Traits::ClassTalentsDeleteConfig const& classTalentsDeleteConfig);
        void HandleClassTalentsSetStarterBuildActive(WorldPackets::Traits::ClassTalentsSetStarterBuildActive const& classTalentsSetStarterBuildActive);
        void HandleClassTalentsSetUsesSharedActionBars(WorldPackets::Traits::ClassTalentsSetUsesSharedActionBars const& classTalentsSetUsesSharedActionBars);
        void HandleClassTalentsNotifyEmptyConfig(WorldPackets::Traits::ClassTalentsNotifyEmptyConfig const& classTalentsNotifyEmptyConfig);
        void HandleClassTalentsNotifyValidationFailed(WorldPackets::Traits::ClassTalentsNotifyValidationFailed const& classTalentsNotifyValidationFailed);
        void HandleTraitsTalentTestUnlearnSpells(WorldPackets::Traits::TraitsTalentTestUnlearnSpells const& traitsTalentTestUnlearnSpells);
        void HandleConfirmProfessionRespec(WorldPackets::Traits::ConfirmProfessionRespec const& confirmProfessionRespec);

        void HandleQuestgiverStatusQueryOpcode(WorldPackets::Quest::QuestGiverStatusQuery& packet);
        void HandleQuestgiverStatusMultipleQuery(WorldPackets::Quest::QuestGiverStatusMultipleQuery& packet);
        void HandleQuestgiverHelloOpcode(WorldPackets::Quest::QuestGiverHello& packet);
        void HandleQuestgiverAcceptQuestOpcode(WorldPackets::Quest::QuestGiverAcceptQuest& packet);
        void HandleQuestgiverQueryQuestOpcode(WorldPackets::Quest::QuestGiverQueryQuest& packet);
        void HandleQuestgiverChooseRewardOpcode(WorldPackets::Quest::QuestGiverChooseReward& packet);
        void HandleQuestgiverRequestRewardOpcode(WorldPackets::Quest::QuestGiverRequestReward& packet);
        void HandleQuestQueryOpcode(WorldPackets::Quest::QueryQuestInfo& packet);
        void HandleQuestLogRemoveQuest(WorldPackets::Quest::QuestLogRemoveQuest& packet);
        void HandleQuestConfirmAccept(WorldPackets::Quest::QuestConfirmAccept& packet);
        void HandleQuestgiverCompleteQuest(WorldPackets::Quest::QuestGiverCompleteQuest& packet);
        void HandleQuestgiverCloseQuest(WorldPackets::Quest::QuestGiverCloseQuest& questGiverCloseQuest);
        void HandlePushQuestToParty(WorldPackets::Quest::PushQuestToParty& packet);
        void HandleQuestPushResult(WorldPackets::Quest::QuestPushResult& packet);
        void HandleRequestWorldQuestUpdate(WorldPackets::Quest::RequestWorldQuestUpdate& packet);
        void HandlePlayerChoiceResponse(WorldPackets::Quest::ChoiceResponse const& choiceResponse);
        void HandleCloseQuestChoice(WorldPackets::Quest::CloseQuestChoice& closeQuestChoice);
        void HandleUiMapQuestLinesRequest(WorldPackets::Quest::UiMapQuestLinesRequest& uiMapQuestLinesRequest);
        void HandleQueryTreasurePicker(WorldPackets::Query::QueryTreasurePicker const& queryTreasurePicker);
        void HandleSpawnTrackingUpdate(WorldPackets::Quest::SpawnTrackingUpdate& spawnTrackingUpdate);
        void HandleQueryQuestItemUsability(WorldPackets::Quest::QueryQuestItemUsability& queryQuestItemUsability);
        void HandleCloseQuestChoice(WorldPackets::Quest::CloseQuestChoice& closeQuestChoice);

        void HandleChatMessageOpcode(WorldPackets::Chat::ChatMessage& chatMessage);
        void HandleChatMessageWhisperOpcode(WorldPackets::Chat::ChatMessageWhisper& chatMessageWhisper);
        void HandleChatMessageChannelOpcode(WorldPackets::Chat::ChatMessageChannel& chatMessageChannel);
        ChatMessageResult HandleChatMessage(ChatMsg type, Language lang, std::string msg, std::string target = "", Optional<ObjectGuid> targetGuid = {});
        void SendChatNotInParty(ChatMsg type);
        void HandleChatAddonMessageOpcode(WorldPackets::Chat::ChatAddonMessage& chatAddonMessage);
        void HandleChatAddonMessageTargetedOpcode(WorldPackets::Chat::ChatAddonMessageTargeted& chatAddonMessageTargeted);
        void HandleChatAddonMessage(ChatMsg type, std::string prefix, std::string text, bool isLogged, std::string target = "", Optional<ObjectGuid> targetGuid = {});
        void HandleChatMessageAFKOpcode(WorldPackets::Chat::ChatMessageAFK& chatMessageAFK);
        void HandleChatMessageDNDOpcode(WorldPackets::Chat::ChatMessageDND& chatMessageDND);
        void HandleChatMessageEmoteOpcode(WorldPackets::Chat::ChatMessageEmote& chatMessageEmote);
        void SendChatPlayerNotfoundNotice(std::string const& name);
        void SendPlayerAmbiguousNotice(std::string const& name);
        void SendChatRestricted(ChatRestrictionType restriction);
        void SendChatNotInParty(ChatMsg type);
        void SendChatNotInGuild(uint32 guildCommandError);           ///< @see enum GuildCommandError - NOT a ChatMsg
        void SendChatIgnoredAccountMuted();
        void SendChatServiceStatus(bool available, bool initial);
        void SendChatRegionalServiceStatus(bool available);
        void SendExpectedSpamRecords();
        void SendChatAutoResponded(bool isDND, std::string_view text);
        void SendCautionaryChatMessage(ChatMsg type, Language requestedLang, std::string const& msg, Player const* target);
        /// true while a confirmed held whisper is being re-run - its sender already got his echo when it was held
        bool IsCautionaryChatAccepted() const { return _chatCautionAccepted; }
        void SendCautionaryChannelMessage(std::string const& msg);
        void HandleChatSendCautionaryChatMessage(WorldPackets::Chat::CautionaryAction& packet);
        void HandleChatDropCautionaryChatMessage(WorldPackets::Chat::CautionaryAction& packet);
        void HandleChatSendCautionaryChannelMessage(WorldPackets::Chat::CautionaryAction& packet);
        void HandleChatReportFiltered(WorldPackets::Ticket::ChatReportFiltered& packet);
        void AddChatSpamFilterReport(ObjectGuid reporterGuid);
        /// remembers that this session delivered a whisper to @p receiverGuid while the realm ships
        /// spam patterns, so a CMSG_CHAT_REPORT_FILTERED naming this session can be checked
        /// against something
        void NoteFilterableWhisper(ObjectGuid receiverGuid);
        /// true if this session delivered such a whisper to @p reporterGuid inside the report
        /// window; takes the entry with it, so one whisper is worth at most one report
        bool ConsumeFilterableWhisper(ObjectGuid reporterGuid);
        void HandleTextEmoteOpcode(WorldPackets::Chat::CTextEmote& packet);
        void HandleChatIgnoredOpcode(WorldPackets::Chat::ChatReportIgnored& chatReportIgnored);
        void HandleChatCanLocalWhisperTargetRequest(WorldPackets::Chat::CanLocalWhisperTargetRequest const& canLocalWhisperTargetRequest);
        void HandleChatUpdateAADCStatus(WorldPackets::Chat::UpdateAADCStatus const& updateAADCStatus);

        void HandleUnregisterAllAddonPrefixesOpcode(WorldPackets::Chat::ChatUnregisterAllAddonPrefixes& packet);
        void HandleAddonRegisteredPrefixesOpcode(WorldPackets::Chat::ChatRegisterAddonPrefixes& packet);

        void HandleReclaimCorpse(WorldPackets::Misc::ReclaimCorpse& packet);
        void HandleQueryCorpseLocation(WorldPackets::Query::QueryCorpseLocationFromClient& packet);
        void HandleQueryCorpseTransport(WorldPackets::Query::QueryCorpseTransport& packet);
        void HandleResurrectResponse(WorldPackets::Misc::ResurrectResponse& packet);
        void HandleSummonResponseOpcode(WorldPackets::Movement::SummonResponse& packet);

        void HandleJoinChannel(WorldPackets::Channel::JoinChannel& packet);
        void HandleLeaveChannel(WorldPackets::Channel::LeaveChannel& packet);
        void HandleChannelCommand(WorldPackets::Channel::ChannelCommand& packet);
        void HandleChannelPlayerCommand(WorldPackets::Channel::ChannelPlayerCommand& packet);
        void HandleChannelPassword(WorldPackets::Channel::ChannelPassword& channelPassword);

        void HandleCompleteCinematic(WorldPackets::Misc::CompleteCinematic& packet);
        void HandleNextCinematicCamera(WorldPackets::Misc::NextCinematicCamera& packet);
        void HandleCompleteMovie(WorldPackets::Misc::CompleteMovie& packet);

        void HandleQueryPageText(WorldPackets::Query::QueryPageText& packet);
        void SendInvalidatePageText(uint32 pageTextId);

        void HandleTutorialFlag(WorldPackets::Misc::TutorialSetFlag& packet);

        //Pet
        void HandlePetAction(WorldPackets::Pet::PetAction& packet);
        void HandlePetStopAttack(WorldPackets::Pet::PetStopAttack& packet);
        void HandlePetActionHelper(Unit* pet, ObjectGuid guid1, uint32 spellid, uint16 flag, ObjectGuid guid2, Position const& pos);
        void HandleQueryPetName(WorldPackets::Query::QueryPetName& packet);
        void HandlePetSetAction(WorldPackets::Pet::PetSetAction& packet);
        void HandlePetAbandon(WorldPackets::Pet::PetAbandon& packet);
        void HandlePetAbandonByNumber(WorldPackets::Pet::PetAbandonByNumber const& petAbandonByNumber);
        void HandlePetRename(WorldPackets::Pet::PetRename& packet);
        void HandlePetCancelAuraOpcode(WorldPackets::Spells::PetCancelAura& packet);
        void HandlePetSpellAutocastOpcode(WorldPackets::Pet::PetSpellAutocast& packet);
        void HandlePetCastSpellOpcode(WorldPackets::Spells::PetCastSpell& petCastSpell);

        void HandleSetActionBarToggles(WorldPackets::Character::SetActionBarToggles& packet);

        void HandleTotemDestroyed(WorldPackets::Totem::TotemDestroyed& totemDestroyed);
        void HandleDismissCritter(WorldPackets::Pet::DismissCritter& dismissCritter);

        //Battleground
        void HandleBattlemasterHelloOpcode(WorldPackets::NPC::Hello& hello);
        bool CheckBattlegroundInfoThrottle();
        void HandleBattlemasterJoinOpcode(WorldPackets::Battleground::BattlemasterJoin& battlemasterJoin);
        void HandlePVPLogDataOpcode(WorldPackets::Battleground::PVPLogDataRequest& pvpLogDataRequest);
        void HandleBattleFieldPortOpcode(WorldPackets::Battleground::BattlefieldPort& battlefieldPort);
        void HandleBattlefieldListOpcode(WorldPackets::Battleground::BattlefieldListRequest& battlefieldList);
        void HandleBattlefieldLeaveOpcode(WorldPackets::Battleground::BattlefieldLeave& battlefieldLeave);
        void HandleBattlemasterJoinArena(WorldPackets::Battleground::BattlemasterJoinArena& packet);
        void HandleBattlemasterJoinRatedBGBlitz(WorldPackets::Battleground::BattlemasterJoinRatedBGBlitz& packet);
        void HandleBattlemasterJoinSkirmish(WorldPackets::Battleground::BattlemasterJoinSkirmish& packet);
        void HandleBattlemasterJoinBrawl(WorldPackets::Battleground::BattlemasterJoinBrawl& packet);
        void HandleJoinRatedBattleground(WorldPackets::Battleground::JoinRatedBattleground& packet);
        void HandleStartWarGame(WorldPackets::Battleground::StartWarGame& packet);
        void HandleAcceptWargameInvite(WorldPackets::Battleground::AcceptWargameInvite& packet);
        void HandleReportPvPAFK(WorldPackets::Battleground::ReportPvPPlayerAFK& reportPvPPlayerAFK);

        // Great Vault / weekly rewards
        void HandleRequestWeeklyRewards(WorldPackets::WeeklyRewards::RequestWeeklyRewards& packet);
        void HandleClaimWeeklyReward(WorldPackets::WeeklyRewards::ClaimWeeklyReward& packet);

        // Content tracking
        void HandleContentTrackingStartTracking(WorldPackets::ContentTracking::StartTracking& packet);
        void HandleContentTrackingStopTracking(WorldPackets::ContentTracking::StopTracking& packet);
        void HandleRequestRatedPvpInfo(WorldPackets::Battleground::RequestRatedPvpInfo& packet);
        void HandleRequestScheduledPvpInfo(WorldPackets::Battleground::RequestScheduledPvpInfo& packet);
        void HandleGetPVPOptionsEnabled(WorldPackets::Battleground::GetPVPOptionsEnabled& getPvPOptionsEnabled);
        void HandleRequestPvpReward(WorldPackets::Battleground::RequestPVPRewards& packet);
        void HandleAreaSpiritHealerQueryOpcode(WorldPackets::Battleground::AreaSpiritHealerQuery& areaSpiritHealerQuery);
        void HandleAreaSpiritHealerQueueOpcode(WorldPackets::Battleground::AreaSpiritHealerQueue& areaSpiritHealerQueue);
        void HandleHearthAndResurrect(WorldPackets::Battleground::HearthAndResurrect& hearthAndResurrect);
        void HandleRequestBattlefieldStatusOpcode(WorldPackets::Battleground::RequestBattlefieldStatus& requestBattlefieldStatus);

        void HandleMinimapPingOpcode(WorldPackets::Party::MinimapPingClient& packet);
        void HandleRandomRollOpcode(WorldPackets::Misc::RandomRollClient& packet);
        void HandleFarSightOpcode(WorldPackets::Misc::FarSight& packet);
        void HandleSetDungeonDifficultyOpcode(WorldPackets::Misc::SetDungeonDifficulty& setDungeonDifficulty);
        void HandleSetRaidDifficultyOpcode(WorldPackets::Misc::SetRaidDifficulty& setRaidDifficulty);
        void HandleSetTitleOpcode(WorldPackets::Character::SetTitle& packet);
        void HandleTimeSync(uint32 counter, int64 clientTime, TimePoint responseReceiveTime);
        void HandleTimeSyncResponse(WorldPackets::Misc::TimeSyncResponse const& timeSyncResponse);
        void HandleTimeSyncResponseFailed(WorldPackets::Misc::TimeSyncResponseFailed const& timeSyncResponseFailed);
        void HandleTimeSyncResponseDropped(WorldPackets::Misc::TimeSyncResponseDropped const& timeSyncResponseDropped);
        void HandleDiscardedTimeSyncAcks(WorldPackets::Misc::DiscardedTimeSyncAcks const& discardedTimeSyncAcks);
        void HandleTimeAdjustmentResponse(WorldPackets::Misc::TimeAdjustmentResponse const& timeAdjustmentResponse);
        void HandleQueuedMessagesEnd(WorldPackets::Auth::QueuedMessagesEnd const& queuedMessagesEnd);
        void HandleSuspendCommsAck(WorldPackets::Auth::SuspendCommsAck const& suspendCommsAck);
        void HandleLatencyReport(WorldPackets::Auth::LatencyReport const& latencyReport);
        void HandleLogStreamingError(WorldPackets::Auth::LogStreamingError const& logStreamingError);
        void HandleDiscardedTimeSyncAcks(WorldPackets::Misc::DiscardedTimeSyncAcks const& discardedTimeSyncAcks);
        void HandleQueuedMessagesEnd(WorldPackets::Auth::QueuedMessagesEnd const& queuedMessagesEnd);
        void HandleSuspendCommsAck(WorldPackets::Auth::SuspendCommsAck const& suspendCommsAck);
        void HandleWhoIsOpcode(WorldPackets::Who::WhoIsRequest& packet);
        void HandleResetInstancesOpcode(WorldPackets::Instance::ResetInstances& packet);
        void HandleInstanceLockResponse(WorldPackets::Instance::InstanceLockResponse& packet);
        void HandleStartInstanceAbandonVote(WorldPackets::Instance::StartInstanceAbandonVote& packet);
        void HandleInstanceAbandonVoteResponse(WorldPackets::Instance::InstanceAbandonVoteResponse& packet);

        // Looking for Dungeon/Raid
        void SendLfgPlayerLockInfo();
        void SendLfgPartyLockInfo();
        void HandleLfgJoinOpcode(WorldPackets::LFG::DFJoin& dfJoin);
        void HandleLfgLeaveOpcode(WorldPackets::LFG::DFLeave& dfLeave);
        void HandleLfgProposalResultOpcode(WorldPackets::LFG::DFProposalResponse& dfProposalResponse);
        void HandleLfgSetRolesOpcode(WorldPackets::LFG::DFSetRoles& dfSetRoles);
        void HandleLfgSetBootVoteOpcode(WorldPackets::LFG::DFBootPlayerVote& dfBootPlayerVote);
        void HandleLfgTeleportOpcode(WorldPackets::LFG::DFTeleport& dfTeleport);
        void HandleDFGetSystemInfo(WorldPackets::LFG::DFGetSystemInfo& dfGetSystemInfo);
        void HandleDFGetJoinStatus(WorldPackets::LFG::DFGetJoinStatus& dfGetJoinStatus);
        void HandleDFConfirmExpandSearch(WorldPackets::LFG::DFConfirmExpandSearch& dfConfirmExpandSearch);
        void HandleDFReadyCheckResponse(WorldPackets::LFG::DFReadyCheckResponse& dfReadyCheckResponse);
        void HandleLFGLorewalkingUpdateRequest(WorldPackets::LFG::LFGLorewalkingUpdateRequest& lfgLorewalkingUpdateRequest);
        void SendLfgSuspendLorewalking(bool suspend);
        void SendSetDFFastLaunchResult(bool lfgFastLaunch);

        // Premade Group Finder (LFG List)
        void HandleLFGListJoin(WorldPackets::LFGList::LFGListJoin& packet);
        void HandleLFGListUpdateRequest(WorldPackets::LFGList::LFGListUpdateRequest& packet);
        void HandleLFGListLeave(WorldPackets::LFGList::LFGListLeave& packet);
        void HandleLFGListGetStatus(WorldPackets::LFGList::LFGListGetStatus& packet);
        void HandleLFGListSearch(WorldPackets::LFGList::LFGListSearch& packet);
        void HandleLFGListApplyToGroup(WorldPackets::LFGList::LFGListApplyToGroup& packet);
        void HandleLFGListCancelApplication(WorldPackets::LFGList::LFGListCancelApplication& packet);
        void HandleLFGListDeclineApplicant(WorldPackets::LFGList::LFGListDeclineApplicant& packet);
        void HandleLFGListInviteApplicant(WorldPackets::LFGList::LFGListInviteApplicant& packet);
        void HandleLFGListInviteResponse(WorldPackets::LFGList::LFGListInviteResponse& packet);
        void HandleLFGListConfirmCensoredActiveEntry(WorldPackets::LFGList::LFGListConfirmCensoredActiveEntry& packet);
        void HandleRequestLFGListBlacklist(WorldPackets::LFGList::RequestLFGListBlacklist& packet);
        void SendLFGListUpdateStatus(uint32 listingId, uint8 status = 0x38);
        void SendLFGListCensoredActiveEntryUpdate(uint32 listingId);

        void SendLfgUpdateStatus(lfg::LfgUpdateData const& updateData, bool party);
        void SendLfgRoleChosen(ObjectGuid guid, uint8 roles);
        void SendLfgRoleCheckUpdate(lfg::LfgRoleCheck const& pRoleCheck);
        void SendLfgReadyCheckUpdate(lfg::LfgReadyCheck const& readyCheck);
        void SendLfgReadyCheckResult(ObjectGuid guid, bool ready);
        void SendLfgInstanceShutdownCountdown(WorldPackets::LFG::RideTicket const& ticket, uint32 timeLeftSeconds);
        void SendLfgJoinResult(lfg::LfgJoinResultData const& joinData);
        void SendLfgQueueStatus(lfg::LfgQueueStatusData const& queueData);
        void SendLfgPlayerReward(lfg::LfgPlayerRewardData const& lfgPlayerRewardData);
        void SendLfgBootProposalUpdate(lfg::LfgPlayerBoot const& boot);
        void SendLfgUpdateProposal(lfg::LfgProposal const& proposal);
        void SendLfgDisabled();
        void SendLfgOfferContinue(uint32 dungeonEntry);
        void SendLfgTeleportError(lfg::LfgTeleportResult err);
        void SendLfgExpandSearchPrompt(WorldPackets::LFG::RideTicket const& ticket);
        void SendLfgSlotInvalid(lfg::LfgSlotInvalidReason reason, int32 subReason1, int32 subReason2);

        void HandleSelfResOpcode(WorldPackets::Spells::SelfRes& selfRes);
        void HandleRequestPetInfo(WorldPackets::Pet::RequestPetInfo& requestPetInfo);

        // Socket gem
        void HandleSocketGems(WorldPackets::Item::SocketGems& socketGems);
        void HandleSortAccountBankBags(WorldPackets::Item::SortAccountBankBags& sortBankBags);
        void HandleSortBags(WorldPackets::Item::SortBags& sortBags);
        void HandleSortBankBags(WorldPackets::Item::SortBankBags& sortBankBags);
        void HandleRemoveNewItem(WorldPackets::Item::RemoveNewItem& removeNewItem);
        void HandlePerformItemInteraction(WorldPackets::Item::PerformItemInteraction& performItemInteraction);
        void HandleConvertItemToBindToAccount(WorldPackets::Item::ConvertItemToBindToAccount& convertItemToBindToAccount);

        void HandleCancelTempEnchantmentOpcode(WorldPackets::Item::CancelTempEnchantment& cancelTempEnchantment);

        void HandleGetItemPurchaseData(WorldPackets::Item::GetItemPurchaseData& packet);
        void HandleItemRefund(WorldPackets::Item::ItemPurchaseRefund& packet);

        void HandleSetTaxiBenchmark(WorldPackets::Misc::SetTaxiBenchmarkMode& packet);

        // Guild Bank
        void HandleGuildPermissionsQuery(WorldPackets::Guild::GuildPermissionsQuery& packet);
        void HandleGuildBankMoneyWithdrawn(WorldPackets::Guild::GuildBankRemainingWithdrawMoneyQuery& packet);
        void HandleGuildBankActivate(WorldPackets::Guild::GuildBankActivate& packet);
        void HandleGuildBankQueryTab(WorldPackets::Guild::GuildBankQueryTab& packet);
        void HandleGuildBankLogQuery(WorldPackets::Guild::GuildBankLogQuery& packet);
        void HandleGuildBankDepositMoney(WorldPackets::Guild::GuildBankDepositMoney& packet);
        void HandleGuildBankWithdrawMoney(WorldPackets::Guild::GuildBankWithdrawMoney& packet);
        void HandleAutoGuildBankItem(WorldPackets::Guild::AutoGuildBankItem& autoGuildBankItem);
        void HandleStoreGuildBankItem(WorldPackets::Guild::StoreGuildBankItem& storeGuildBankItem);
        void HandleSwapItemWithGuildBankItem(WorldPackets::Guild::SwapItemWithGuildBankItem& swapItemWithGuildBankItem);
        void HandleSwapGuildBankItemWithGuildBankItem(WorldPackets::Guild::SwapGuildBankItemWithGuildBankItem& swapGuildBankItemWithGuildBankItem);
        void HandleMoveGuildBankItem(WorldPackets::Guild::MoveGuildBankItem& moveGuildBankItem);
        void HandleMergeItemWithGuildBankItem(WorldPackets::Guild::MergeItemWithGuildBankItem& mergeItemWithGuildBankItem);
        void HandleSplitItemToGuildBank(WorldPackets::Guild::SplitItemToGuildBank& splitItemToGuildBank);
        void HandleMergeGuildBankItemWithItem(WorldPackets::Guild::MergeGuildBankItemWithItem& mergeGuildBankItemWithItem);
        void HandleSplitGuildBankItemToInventory(WorldPackets::Guild::SplitGuildBankItemToInventory& splitGuildBankItemToInventory);
        void HandleAutoStoreGuildBankItem(WorldPackets::Guild::AutoStoreGuildBankItem& autoStoreGuildBankItem);
        void HandleMergeGuildBankItemWithGuildBankItem(WorldPackets::Guild::MergeGuildBankItemWithGuildBankItem& mergeGuildBankItemWithGuildBankItem);
        void HandleSplitGuildBankItem(WorldPackets::Guild::SplitGuildBankItem& splitGuildBankItem);

        void HandleGuildBankUpdateTab(WorldPackets::Guild::GuildBankUpdateTab& packet);
        void HandleGuildBankBuyTab(WorldPackets::Guild::GuildBankBuyTab& packet);
        void HandleGuildBankTextQuery(WorldPackets::Guild::GuildBankTextQuery& packet);
        void HandleGuildBankSetTabText(WorldPackets::Guild::GuildBankSetTabText& packet);

        // Calendar
        void HandleCalendarGetCalendar(WorldPackets::Calendar::CalendarGetCalendar& calendarGetCalendar);
        void HandleCalendarGetEvent(WorldPackets::Calendar::CalendarGetEvent& calendarGetEvent);
        void HandleCalendarCommunityInvite(WorldPackets::Calendar::CalendarCommunityInviteRequest& calendarCommunityInvite);
        void HandleCalendarAddEvent(WorldPackets::Calendar::CalendarAddEvent& calendarAddEvent);
        void HandleCalendarUpdateEvent(WorldPackets::Calendar::CalendarUpdateEvent& calendarUpdateEvent);
        void HandleCalendarRemoveEvent(WorldPackets::Calendar::CalendarRemoveEvent& calendarRemoveEvent);
        void HandleCalendarCopyEvent(WorldPackets::Calendar::CalendarCopyEvent& calendarCopyEvent);
        void HandleCalendarInvite(WorldPackets::Calendar::CalendarInvite& calendarEventInvite);
        void HandleCalendarRsvp(WorldPackets::Calendar::CalendarRSVP& calendarRSVP);
        void HandleCalendarEventRemoveInvite(WorldPackets::Calendar::CalendarRemoveInvite& calendarRemoveInvite);
        void HandleCalendarStatus(WorldPackets::Calendar::CalendarStatus& calendarStatus);
        void HandleCalendarModeratorStatus(WorldPackets::Calendar::CalendarModeratorStatusQuery& calendarModeratorStatus);
        void HandleCalendarComplain(WorldPackets::Calendar::CalendarComplain& calendarComplain);
        void HandleCalendarGetNumPending(WorldPackets::Calendar::CalendarGetNumPending& calendarGetNumPending);
        void HandleCalendarEventSignup(WorldPackets::Calendar::CalendarEventSignUp& calendarEventSignUp);

        void SendCalendarRaidLockoutAdded(InstanceLock const* lock);
        void SendCalendarRaidLockoutRemoved(InstanceLock const* lock);
        void HandleSetSavedInstanceExtend(WorldPackets::Calendar::SetSavedInstanceExtend& setSavedInstanceExtend);

        // Collections
        void HandleCollectionItemSetFavorite(WorldPackets::Collections::CollectionItemSetFavorite& collectionItemSetFavorite);

        // Transmogrification
        void HandleTransmogrifyItems(WorldPackets::Transmogrification::TransmogrifyItems& transmogrifyItems);
        void HandleTransmogOutfitNew(WorldPackets::Transmogrification::TransmogOutfitNew const& transmogOutfitNew);
        void HandleTransmogOutfitUpdateInfo(WorldPackets::Transmogrification::TransmogOutfitUpdateInfo const& transmogOutfitUpdateInfo);
        void HandleTransmogOutfitUpdateSituations(WorldPackets::Transmogrification::TransmogOutfitUpdateSituations const& transmogOutfitUpdateSituations);
        void HandleTransmogOutfitUpdateSlots(WorldPackets::Transmogrification::TransmogOutfitUpdateSlots const& transmogOutfitUpdateSlots);

        // Miscellaneous
        void HandleSpellClick(WorldPackets::Spells::SpellClick& spellClick);
        void HandleMirrorImageDataRequest(WorldPackets::Spells::GetMirrorImageData& getMirrorImageData);
        void HandleGuildSetFocusedAchievement(WorldPackets::Achievement::GuildSetFocusedAchievement& setFocusedAchievement);
        void HandleEquipmentSetSave(WorldPackets::EquipmentSet::SaveEquipmentSet& saveEquipmentSet);
        void HandleDeleteEquipmentSet(WorldPackets::EquipmentSet::DeleteEquipmentSet& deleteEquipmentSet);
        void HandleAssignEquipmentSetSpec(WorldPackets::EquipmentSet::AssignEquipmentSetSpec& assignEquipmentSetSpec);
        void HandleUseEquipmentSet(WorldPackets::EquipmentSet::UseEquipmentSet& useEquipmentSet);
        void HandleServerTimeOffsetRequest(WorldPackets::Misc::ServerTimeOffsetRequest& /*request*/);
        void HandleQueryQuestCompletionNPCs(WorldPackets::Query::QueryQuestCompletionNPCs& queryQuestCompletionNPCs);
        void HandleQuestPOIQuery(WorldPackets::Query::QuestPOIQuery& questPoiQuery);
        void HandleViolenceLevel(WorldPackets::Misc::ViolenceLevel& violenceLevel);
        void HandleGetCharacterCurrencyTransferLog(WorldPackets::Misc::GetCharacterCurrencyTransferLog& packet);
        void HandleRequestCurrencyDataForAccountCharacters(WorldPackets::Misc::RequestCurrencyDataForAccountCharacters& packet);
        void HandleObjectUpdateFailedOpcode(WorldPackets::Misc::ObjectUpdateFailed& objectUpdateFailed);
        void HandleObjectUpdateRescuedOpcode(WorldPackets::Misc::ObjectUpdateRescued& objectUpdateRescued);
        void HandleCloseInteraction(WorldPackets::Misc::CloseInteraction& closeInteraction);
        void HandleCloseRuneforgeInteraction(WorldPackets::Misc::CloseRuneforgeInteraction& closeRuneforgeInteraction);
        void HandleCloseTraitSystemInteraction(WorldPackets::Misc::CloseTraitSystemInteraction& closeTraitSystemInteraction);
        void HandleContributionContribute(WorldPackets::Contribution::ContributionContribute& contribute);
        void HandleContributionLastUpdateRequest(WorldPackets::Contribution::ContributionLastUpdateRequest& request);

        // Commentator (spectator) mode
        void HandleCommentatorEnable(WorldPackets::Commentator::CommentatorEnable& packet);
        void HandleCommentatorGetMapInfo(WorldPackets::Commentator::CommentatorGetMapInfo& getMapInfo);
        void HandleCommentatorEnterInstance(WorldPackets::Commentator::CommentatorEnterInstance& enterInstance);
        void HandleCommentatorExitInstance(WorldPackets::Commentator::CommentatorExitInstance& exitInstance);
        void HandleCommentatorSpectate(WorldPackets::Commentator::CommentatorSpectate& spectate);
        void HandleCommentatorGetPlayerInfo(WorldPackets::Commentator::CommentatorGetPlayerInfo& getPlayerInfo);
        void HandleCommentatorGetPlayerCooldowns(WorldPackets::Commentator::CommentatorGetPlayerCooldowns& getPlayerCooldowns);
        void HandleCommentatorStartWargame(WorldPackets::Commentator::CommentatorStartWargame& startWargame);
        bool IsCommentator() const { return _isCommentator; }
        void SetCommentator(bool on) { _isCommentator = on; }
        void HandleConversationLineStarted(WorldPackets::Misc::ConversationLineStarted& conversationLineStarted);
        void HandleKeyboundOverride(WorldPackets::Spells::KeyboundOverride& keyboundOverride);
        void HandleRequestCrowdControlSpell(WorldPackets::Spells::RequestCrowdControlSpell& requestCrowdControlSpell);
        void HandleQueryCountdownTimer(WorldPackets::Misc::QueryCountdownTimer& queryCountdownTimer);
        void HandleSetCurrencyFlags(WorldPackets::Misc::SetCurrencyFlags const& setCurrenctFlags);

        // Client family 0x64 - player state and UI remote control (12.1.0.69382)
        void HandleAbandonNPEResponse(WorldPackets::Misc::AbandonNPEResponse& abandonNpeResponse);
        void HandleSubscriptionInterstitialResponse(WorldPackets::Misc::SubscriptionInterstitialResponse& subscriptionInterstitialResponse);
        void HandleRequestScheduledAreaPoiUpdate(WorldPackets::Misc::RequestScheduledAreaPoiUpdate& requestScheduledAreaPoiUpdate);
        void HandleBonusRoll(WorldPackets::Misc::BonusRoll& bonusRoll);
        // GM / Cheat / Debug (families 0x45 and 0x3D)
        void HandleKioskEnableGodMode(WorldPackets::Misc::KioskEnableGodMode& kioskEnableGodMode);
        void HandleSetGameEventDebugViewState(WorldPackets::Misc::SetGameEventDebugViewState& setGameEventDebugViewState);

        // Subscription state of the client debug views (table 0x43BD1C0, indices 0..39), driven by
        // CMSG_SET_GAME_EVENT_DEBUG_VIEW_STATE. Deliberately session bound and never persisted: the
        // client re-announces every subscription after each SMSG_DEBUG_MENU_MANAGER_FULL_UPDATE.
        bool IsDebugViewSubscribed(uint32 viewIndex) const { return viewIndex < MaxDebugViews && _debugViewSubscriptions.test(viewIndex); }
        std::bitset<40> const& GetDebugViewSubscriptions() const { return _debugViewSubscriptions; }

        // Adventure Journal
        void HandleAdventureJournalOpenQuest(WorldPackets::AdventureJournal::AdventureJournalOpenQuest& openQuest);
        void HandleAdventureJournalUpdateSuggestions(WorldPackets::AdventureJournal::AdventureJournalUpdateSuggestions& updateSuggestions);
        void HandleEncounterJournalStartArathiRpe(WorldPackets::AdventureJournal::EncounterJournalStartArathiRpe& startArathiRpe);

        // Covenant
        void HandleActivateSoulbind(WorldPackets::Covenant::ActivateSoulbind& packet);
        void HandleRequestCovenantCallings(WorldPackets::Covenant::RequestCovenantCallings& packet);
        void HandleCovenantRenownRequestCatchupState(WorldPackets::Covenant::CovenantRenownRequestCatchupState& packet);

        // Adventure Map
        void HandleCheckIsAdventureMapPoiValid(WorldPackets::AdventureMap::CheckIsAdventureMapPoiValid& CheckIsAdventureMapPoiValid);
        void HandleAdventureMapStartQuest(WorldPackets::AdventureMap::AdventureMapStartQuest& startQuest);

        // Toys
        void HandleAddToy(WorldPackets::Toy::AddToy& packet);
        void HandleUseToy(WorldPackets::Toy::UseToy& packet);
        void HandleToyClearFanfare(WorldPackets::Toy::ToyClearFanfare& toyClearFanfare);

        void HandleMountSetFavorite(WorldPackets::Misc::MountSetFavorite& mountSetFavorite);
        void HandleMountClearFanfare(WorldPackets::Misc::MountClearFanfare& mountClearFanfare);

        // Scenes
        void HandleSceneTriggerEvent(WorldPackets::Scenes::SceneTriggerEvent& sceneTriggerEvent);
        void HandleScenePlaybackComplete(WorldPackets::Scenes::ScenePlaybackComplete& scenePlaybackComplete);
        void HandleScenePlaybackCanceled(WorldPackets::Scenes::ScenePlaybackCanceled& scenePlaybackCanceled);

        // Token
        void HandleCommerceTokenGetLog(WorldPackets::Token::CommerceTokenGetLog& updateListedAuctionableTokens);
        void HandleCommerceTokenGetMarketPrice(WorldPackets::Token::CommerceTokenGetMarketPrice& requestWowTokenMarketPrice);

        // Compact Unit Frames (4.x)
        void HandleSaveCUFProfiles(WorldPackets::Misc::SaveCUFProfiles& packet);
        void SendLoadCUFProfiles();

        // Challenge Mode (Mythic+)
        void HandleRequestMythicPlusSeasonData(WorldPackets::ChallengeMode::RequestMythicPlusSeasonData& requestMythicPlusSeasonData);
        void HandleRequestMythicPlusAffixes(WorldPackets::ChallengeMode::RequestMythicPlusAffixes& requestMythicPlusAffixes);
        void HandleStartChallengeMode(WorldPackets::ChallengeMode::StartChallengeMode& startChallengeMode);
        void HandleResetChallengeMode(WorldPackets::ChallengeMode::ResetChallengeMode& resetChallengeMode);
        void HandleMythicPlusRequestMapStats(WorldPackets::ChallengeMode::MythicPlusRequestMapStats& request);
        void HandleChallengeModeRequestLeaders(WorldPackets::ChallengeMode::RequestLeaders& request);
        // CMSG_REQUEST_WEEKLY_REWARDS / CMSG_CLAIM_WEEKLY_REWARD are bound to the WorldPackets::WeeklyRewards
        // overloads (WeeklyRewardHandler.cpp), which serve all three vault rows - see ChallengeModeHandler.cpp.

        // Garrison
        void HandleGetGarrisonInfo(WorldPackets::Garrison::GetGarrisonInfo& getGarrisonInfo);
        void HandleGarrisonPurchaseBuilding(WorldPackets::Garrison::GarrisonPurchaseBuilding& garrisonPurchaseBuilding);
        void HandleGarrisonCancelConstruction(WorldPackets::Garrison::GarrisonCancelConstruction& garrisonCancelConstruction);
        void HandleGarrisonRequestBlueprintAndSpecializationData(WorldPackets::Garrison::GarrisonRequestBlueprintAndSpecializationData& garrisonRequestBlueprintAndSpecializationData);
        void HandleGarrisonGetMapData(WorldPackets::Garrison::GarrisonGetMapData& garrisonGetMapData);
        void HandleGarrisonStartMission(WorldPackets::Garrison::GarrisonStartMission& garrisonStartMission);
        void HandleGarrisonCompleteMission(WorldPackets::Garrison::GarrisonCompleteMission& garrisonCompleteMission);
        void HandleGarrisonMissionBonusRoll(WorldPackets::Garrison::GarrisonMissionBonusRoll& garrisonMissionBonusRoll);
        void HandleGarrisonGetMissionReward(WorldPackets::Garrison::GarrisonGetMissionReward& garrisonGetMissionReward);
        void HandleOpenMissionNpc(WorldPackets::Garrison::OpenMissionNpc& openMissionNpc);
        void HandleUpgradeGarrison(WorldPackets::Garrison::UpgradeGarrison& upgradeGarrison);
        void HandleGarrisonCheckUpgradeable(WorldPackets::Garrison::GarrisonCheckUpgradeable& garrisonCheckUpgradeable);
        void HandleGarrisonSetBuildingActive(WorldPackets::Garrison::GarrisonSetBuildingActive& garrisonSetBuildingActive);
        void HandleGarrisonSwapBuildings(WorldPackets::Garrison::GarrisonSwapBuildings& garrisonSwapBuildings);
        void HandleGarrisonAssignFollowerToBuilding(WorldPackets::Garrison::GarrisonAssignFollowerToBuilding& garrisonAssignFollowerToBuilding);
        void HandleGarrisonRemoveFollowerFromBuilding(WorldPackets::Garrison::GarrisonRemoveFollowerFromBuilding& garrisonRemoveFollowerFromBuilding);
        void HandleGarrisonRemoveFollower(WorldPackets::Garrison::GarrisonRemoveFollower& garrisonRemoveFollower);
        void HandleGarrisonRenameFollower(WorldPackets::Garrison::GarrisonRenameFollower& garrisonRenameFollower);
        void HandleGarrisonSetFollowerFavorite(WorldPackets::Garrison::GarrisonSetFollowerFavorite& garrisonSetFollowerFavorite);
        void HandleGarrisonSetFollowerInactive(WorldPackets::Garrison::GarrisonSetFollowerInactive& garrisonSetFollowerInactive);
        void HandleGarrisonRecruitFollower(WorldPackets::Garrison::GarrisonRecruitFollower& garrisonRecruitFollower);
        void HandleGarrisonGenerateRecruits(WorldPackets::Garrison::GarrisonGenerateRecruits& garrisonGenerateRecruits);
        void HandleGarrisonFullyHealAllFollowers(WorldPackets::Garrison::GarrisonFullyHealAllFollowers& garrisonFullyHealAllFollowers);
        void HandleGarrisonAddFollowerHealth(WorldPackets::Garrison::GarrisonAddFollowerHealth& garrisonAddFollowerHealth);
        void HandleGarrisonGetClassSpecCategoryInfo(WorldPackets::Garrison::GarrisonGetClassSpecCategoryInfo& garrisonGetClassSpecCategoryInfo);
        void HandleGarrisonSetRecruitmentPreferences(WorldPackets::Garrison::GarrisonSetRecruitmentPreferences& garrisonSetRecruitmentPreferences);
        void HandleGarrisonLearnTalent(WorldPackets::Garrison::GarrisonLearnTalent& garrisonLearnTalent);
        void HandleGarrisonResearchTalent(WorldPackets::Garrison::GarrisonResearchTalent& garrisonResearchTalent);
        void HandleGarrisonSocketTalent(WorldPackets::Garrison::GarrisonSocketTalent& garrisonSocketTalent);
        void HandleGarrisonRequestShipmentInfo(WorldPackets::Garrison::GarrisonRequestShipmentInfo& garrisonRequestShipmentInfo);
        void HandleOpenShipmentNpc(WorldPackets::Garrison::OpenShipmentNpc& openShipmentNpc);
        void HandleCreateShipment(WorldPackets::Garrison::CreateShipment& createShipment);
        void HandleGetLandingPageShipments(WorldPackets::Garrison::GetLandingPageShipments& getLandingPageShipments);
        void HandleSetUsingPartyGarrison(WorldPackets::Garrison::SetUsingPartyGarrison& setUsingPartyGarrison);
        void HandleQueryGarrisonPetName(WorldPackets::Garrison::QueryGarrisonPetName& queryGarrisonPetName);
        void HandleRequestGarrisonTalentWorldQuestUnlocks(WorldPackets::Garrison::RequestGarrisonTalentWorldQuestUnlocks& requestGarrisonTalentWorldQuestUnlocks);
        void HandleGetTrophyList(WorldPackets::Garrison::GetTrophyList& getTrophyList);
        void HandleReplaceTrophy(WorldPackets::Garrison::ReplaceTrophy& replaceTrophy);
        void HandleLoadSelectedTrophy(WorldPackets::Garrison::LoadSelectedTrophy& loadSelectedTrophy);
        void HandleChangeMonumentAppearance(WorldPackets::Garrison::ChangeMonumentAppearance& changeMonumentAppearance);
        void HandleRevertMonumentAppearance(WorldPackets::Garrison::RevertMonumentAppearance& revertMonumentAppearance);
        void HandleGarrisonSocketTalent(WorldPackets::Garrison::GarrisonSocketTalent& garrisonSocketTalent);

        // Battle Pets
        void HandleBattlePetRequestJournal(WorldPackets::BattlePet::BattlePetRequestJournal& battlePetRequestJournal);
        void HandleBattlePetRequestJournalLock(WorldPackets::BattlePet::BattlePetRequestJournalLock& battlePetRequestJournalLock);
        void HandleBattlePetSetBattleSlot(WorldPackets::BattlePet::BattlePetSetBattleSlot& battlePetSetBattleSlot);
        void HandleBattlePetModifyName(WorldPackets::BattlePet::BattlePetModifyName& battlePetModifyName);
        void HandleQueryBattlePetName(WorldPackets::BattlePet::QueryBattlePetName& queryBattlePetName);
        void HandleBattlePetDeletePet(WorldPackets::BattlePet::BattlePetDeletePet& battlePetDeletePet);
        void HandleBattlePetSetFlags(WorldPackets::BattlePet::BattlePetSetFlags& battlePetSetFlags);
        void HandleBattlePetClearFanfare(WorldPackets::BattlePet::BattlePetClearFanfare& battlePetClearFanfare);
        void HandleBattlePetSummon(WorldPackets::BattlePet::BattlePetSummon& battlePetSummon);
        void HandleBattlePetUpdateNotify(WorldPackets::BattlePet::BattlePetUpdateNotify& battlePetUpdateNotify);
        void HandleBattlePetUpdateDisplayNotify(WorldPackets::BattlePet::BattlePetUpdateDisplayNotify& battlePetUpdateDisplayNotify);
        void HandleCageBattlePet(WorldPackets::BattlePet::CageBattlePet& cageBattlePet);
        // Pet Battle combat
        void HandlePetBattleRequestWild(WorldPackets::BattlePet::PetBattleRequestWild& petBattleRequestWild);
        void StartNPCPetBattle(Creature* trainer);
        void HandlePetBattleInput(WorldPackets::BattlePet::PetBattleInput& petBattleInput);
        void HandlePetBattleReplaceFrontPet(WorldPackets::BattlePet::PetBattleReplaceFrontPet& petBattleReplaceFrontPet);
        void HandlePetBattleQuitNotify(WorldPackets::BattlePet::PetBattleQuitNotify& petBattleQuitNotify);
        void HandlePetBattleFinalNotify(WorldPackets::BattlePet::PetBattleFinalNotify& petBattleFinalNotify);
        void HandlePetBattleRequestPVP(WorldPackets::BattlePet::PetBattleRequestPVP& petBattleRequestPVP);
        void HandleJoinPetBattleQueue(WorldPackets::BattlePet::JoinPetBattleQueue& joinPetBattleQueue);
        void HandleLeavePetBattleQueue(WorldPackets::BattlePet::LeavePetBattleQueue& leavePetBattleQueue);
        void HandlePetBattleQueueProposeMatchResult(WorldPackets::BattlePet::PetBattleQueueProposeMatchResult& petBattleQueueProposeMatchResult);
        void HandlePetBattleRequestUpdate(WorldPackets::BattlePet::PetBattleRequestUpdate& petBattleRequestUpdate);
        void HandlePetBattleScriptErrorNotify(WorldPackets::BattlePet::PetBattleScriptErrorNotify& petBattleScriptErrorNotify);
        void HandlePetBattleWildLocationFail(WorldPackets::BattlePet::PetBattleWildLocationFail& petBattleWildLocationFail);

        // Battlenet
        void HandleBattlenetChangeRealmTicket(WorldPackets::Battlenet::ChangeRealmTicket& changeRealmTicket);
        void HandleBattlenetRequest(WorldPackets::Battlenet::Request& request);

        void SendBattlenetResponse(uint32 serviceHash, uint32 methodId, uint32 token, pb::Message const* response);
        void SendBattlenetResponse(uint32 serviceHash, uint32 methodId, uint32 token, uint32 status);
        void SendBattlenetRequest(uint32 serviceHash, uint32 methodId, pb::Message const* request, std::function<void(MessageBuffer)> callback);
        void SendBattlenetRequest(uint32 serviceHash, uint32 methodId, pb::Message const* request);

        std::array<uint8, 32> const& GetRealmListSecret() const { return _realmListSecret; }
        void SetRealmListSecret(std::array<uint8, 32> const& secret) { _realmListSecret = secret; }

        std::unordered_map<uint32, uint8> const& GetRealmCharacterCounts() const { return _realmCharacterCounts; }

        void HandleQueryRealmName(WorldPackets::Query::QueryRealmName& queryRealmName);

        // Artifact
        void HandleArtifactAddPower(WorldPackets::Artifact::ArtifactAddPower& artifactAddPower);
        void HandleArtifactSetAppearance(WorldPackets::Artifact::ArtifactSetAppearance& artifactSetAppearance);
        void HandleConfirmArtifactRespec(WorldPackets::Artifact::ConfirmArtifactRespec& confirmArtifactRespec);

        // Scenario
        void HandleQueryScenarioPOI(WorldPackets::Scenario::QueryScenarioPOI& queryScenarioPOI);

        // Azerite
        void HandleAzeriteEssenceUnlockMilestone(WorldPackets::Azerite::AzeriteEssenceUnlockMilestone& azeriteEssenceUnlockMilestone);
        void HandleAzeriteEssenceActivateEssence(WorldPackets::Azerite::AzeriteEssenceActivateEssence& azeriteEssenceActivateEssence);
        void HandleAzeriteEmpoweredItemViewed(WorldPackets::Azerite::AzeriteEmpoweredItemViewed& azeriteEmpoweredItemViewed);
        void HandleAzeriteEmpoweredItemSelectPower(WorldPackets::Azerite::AzeriteEmpoweredItemSelectPower& azeriteEmpoweredItemSelectPower);
        void SendAzeriteRespecNPC(ObjectGuid npc);

        void HandleRequestLatestSplashScreen(WorldPackets::Misc::RequestLatestSplashScreen& requestLatestSplashScreen);

        void HandleSocialContractRequest(WorldPackets::Social::SocialContractRequest& socialContractRequest);

        // ----------------------------------------------------------------------------------
        // Einheit w4_cmsg_43_3D - Sendeseite der Sammelfamilien 0x43 / 0x3D, Phase A.
        // Belege je Handler in Handlers/MiscHandler.cpp, BattlenetHandler.cpp,
        // LiveRegionHandler.cpp und BleepHandler.cpp.
        // ----------------------------------------------------------------------------------

        // Track A - der Server haelt den Zustand bereits (oder haelt ihn ausdruecklich nicht)
        void HandleUsedFollow(WorldPackets::Misc::UsedFollow& usedFollow);
        void HandleSeamlessTransferComplete(WorldPackets::Misc::SeamlessTransferComplete& seamlessTransferComplete);
        void HandleResetChallengeModeCheat(WorldPackets::Misc::ResetChallengeModeCheat& resetChallengeModeCheat);
        void HandleClassTalentsNotifyEmptyConfig(WorldPackets::Misc::ClassTalentsNotifyEmptyConfig& classTalentsNotifyEmptyConfig);
        void HandleClassTalentsNotifyValidationFailed(WorldPackets::Misc::ClassTalentsNotifyValidationFailed& classTalentsNotifyValidationFailed);
        void HandleTraitsTalentTestUnlearnSpells(WorldPackets::Misc::TraitsTalentTestUnlearnSpells& traitsTalentTestUnlearnSpells);
        void HandleAddAccountCosmetic(WorldPackets::Misc::AddAccountCosmetic& addAccountCosmetic);
        void HandleUpdateCraftingNpcRecipes(WorldPackets::Misc::UpdateCraftingNpcRecipes& updateCraftingNpcRecipes);
        void HandleIslandQueue(WorldPackets::Misc::IslandQueue& islandQueue);
        void HandleShowTradeSkill(WorldPackets::Misc::ShowTradeSkill& showTradeSkill);
        void HandleQuestDrivenScenarioStateChange(WorldPackets::Misc::QuestDrivenScenarioStateChange& questDrivenScenarioStateChange);
        void HandleWorldLootObjectClick(WorldPackets::Misc::WorldLootObjectClick& worldLootObjectClick);
        void HandleUpgradeRuneforgeLegendary(WorldPackets::Misc::UpgradeRuneforgeLegendary& upgradeRuneforgeLegendary);
        void HandleSetExcludedChatCensorSources(WorldPackets::Misc::SetExcludedChatCensorSources& setExcludedChatCensorSources);
        void HandleSilenceTalkerInParty(WorldPackets::Misc::SilenceTalkerInParty& silenceTalkerInParty);
        void HandleAddonList(WorldPackets::Misc::AddonList& addonList);
        void HandleStartSpectatorWarGame(WorldPackets::Misc::StartSpectatorWarGame& startSpectatorWarGame);
        void HandleLowLevelRaid1(WorldPackets::Misc::LowLevelRaid1& lowLevelRaid1);
        void HandleSpectateChange(WorldPackets::Misc::SpectateChange& spectateChange);
        void HandleSpectateSetNextTarget(WorldPackets::Misc::SpectateSetNextTarget& spectateSetNextTarget);
        void HandleSpectateEnd(WorldPackets::Misc::SpectateEnd& spectateEnd);
        void HandleQuickJoinAutoAcceptRequests(WorldPackets::Misc::QuickJoinAutoAcceptRequests& quickJoinAutoAcceptRequests);
        void HandleQuickJoinSignalToastDisplayed(WorldPackets::Misc::QuickJoinSignalToastDisplayed& quickJoinSignalToastDisplayed);
        void HandleQuickJoinRespondToInvite(WorldPackets::Misc::QuickJoinRespondToInvite& quickJoinRespondToInvite);
        void HandleQuickJoinRequestInvite(WorldPackets::Misc::QuickJoinRequestInvite& quickJoinRequestInvite);
        void HandleQuickJoinRequestInviteWithConfirmation(WorldPackets::Misc::QuickJoinRequestInviteWithConfirmation& quickJoinRequestInviteWithConfirmation);

        // B3 - Telemetrie & Support
        void HandleReportServerLag(WorldPackets::Misc::ReportServerLag& reportServerLag);
        void HandleGMTicketAcknowledgeSurvey(WorldPackets::Misc::GMTicketAcknowledgeSurvey& gmTicketAcknowledgeSurvey);
        void HandleEngineSurvey(WorldPackets::Misc::EngineSurvey& engineSurvey);
        void HandleServerValidationSignatureRequest(WorldPackets::Misc::ServerValidationSignatureRequest& serverValidationSignatureRequest);
        // Mindestabstand zweier protokollierter CMSG_REPORT_SERVER_LAG derselben Sitzung, in Sekunden.
        static constexpr time_t REPORT_SERVER_LAG_INTERVAL = 10;

        // B11 - Warden3
        void HandleWarden3Data(WorldPackets::Misc::Warden3Data& warden3Data);

        // B4 - Bnet-Praesenz / Community
        void HandleRequestChatLogin(WorldPackets::Misc::RequestChatLogin& requestChatLogin);
        void HandleAddBattlenetFriend(WorldPackets::Battlenet::AddBattlenetFriend& addBattlenetFriend);
        void HandleBattlenetChallengeResponse(WorldPackets::Battlenet::BattlenetChallengeResponse& battlenetChallengeResponse);
        void HandleClubPresenceSubscribe(WorldPackets::Battlenet::ClubPresenceSubscribe& clubPresenceSubscribe);
        void HandleSendCharacterClubInvitation(WorldPackets::Battlenet::SendCharacterClubInvitation& sendCharacterClubInvitation);
        void HandleRequestCharacterGuildFollowInfo(WorldPackets::Battlenet::RequestCharacterGuildFollowInfo& requestCharacterGuildFollowInfo);
        void HandleSaveAccountDataExport(WorldPackets::Battlenet::SaveAccountDataExport& saveAccountDataExport);
        void HandleRequestRealmGuildMasterInfo(WorldPackets::Battlenet::RequestRealmGuildMasterInfo& requestRealmGuildMasterInfo);

        // B5 - Live-Region-Kopie
        void HandleLiveRegionGetAccountCharacterList(WorldPackets::LiveRegion::GetAccountCharacterList& getAccountCharacterList);
        void HandleLiveRegionCharacterCopy(WorldPackets::LiveRegion::CharacterCopy& characterCopy);
        void HandleLiveRegionAccountRestore(WorldPackets::LiveRegion::AccountRestore& accountRestore);
        void HandleLiveRegionKeyBindingsCopy(WorldPackets::LiveRegion::KeyBindingsCopy& keyBindingsCopy);

        // B8 - Bleep
        void HandleFetchBleepProxies(WorldPackets::Bleep::FetchBleepProxies& fetchBleepProxies);
        void HandleBleepPong(WorldPackets::Bleep::BleepPong& bleepPong);
        void HandleRefreshBleepTokens(WorldPackets::Bleep::RefreshBleepTokens& refreshBleepTokens);
        void HandleExpireBleepTokens(WorldPackets::Bleep::ExpireBleepTokens& expireBleepTokens);

        // B6 - Lobby-Matchmaker
        void HandleLobbyMatchmakerPartyInvite(WorldPackets::LobbyMatchmaker::LobbyMatchmakerPartyInvite& lobbyMatchmakerPartyInvite);
        void HandleLobbyMatchmakerAcceptPartyInvite(WorldPackets::LobbyMatchmaker::LobbyMatchmakerAcceptPartyInvite& lobbyMatchmakerAcceptPartyInvite);
        void HandleLobbyMatchmakerRejectPartyInvite(WorldPackets::LobbyMatchmaker::LobbyMatchmakerRejectPartyInvite& lobbyMatchmakerRejectPartyInvite);
        void HandleLobbyMatchmakerPartyUninvite(WorldPackets::LobbyMatchmaker::LobbyMatchmakerPartyUninvite& lobbyMatchmakerPartyUninvite);
        void HandleLobbyMatchmakerLeaveParty(WorldPackets::LobbyMatchmaker::LobbyMatchmakerLeaveParty& lobbyMatchmakerLeaveParty);
        void HandleLobbyMatchmakerSetPartyPlaylistEntry(WorldPackets::LobbyMatchmaker::LobbyMatchmakerSetPartyPlaylistEntry& lobbyMatchmakerSetPartyPlaylistEntry);
        void HandleLobbyMatchmakerSetPlayerReady(WorldPackets::LobbyMatchmaker::LobbyMatchmakerSetPlayerReady& lobbyMatchmakerSetPlayerReady);
        void HandleLobbyMatchmakerEnterQueue(WorldPackets::LobbyMatchmaker::LobbyMatchmakerEnterQueue& lobbyMatchmakerEnterQueue);
        void HandleLobbyMatchmakerQueueProposalResponse(WorldPackets::LobbyMatchmaker::LobbyMatchmakerQueueProposalResponse& lobbyMatchmakerQueueProposalResponse);
        void HandleLobbyMatchmakerAbandonQueue(WorldPackets::LobbyMatchmaker::LobbyMatchmakerAbandonQueue& lobbyMatchmakerAbandonQueue);
        void HandleLobbyMatchmakerCreateCharacter(WorldPackets::LobbyMatchmaker::LobbyMatchmakerCreateCharacter& lobbyMatchmakerCreateCharacter);
        void HandleRegisterFastLogin(WorldPackets::LobbyMatchmaker::RegisterFastLogin& registerFastLogin);

        // B9 - Voice-Chat
        void HandleVoiceChatLogin(WorldPackets::VoiceChat::VoiceChatLogin& voiceChatLogin);
        void HandleVoiceChannelSttTokenRequest(WorldPackets::VoiceChat::VoiceChannelSttTokenRequest& voiceChannelSttTokenRequest);
        void HandleVoiceChatJoinChannel(WorldPackets::VoiceChat::VoiceChatJoinChannel& voiceChatJoinChannel);

        // B7 - WoW Labs / Plunderstorm
        void HandleQuerySelectedWowLabsArea(WorldPackets::WowLabs::QuerySelectedWowLabsArea& querySelectedWowLabsArea);
        void HandleQueryWowLabsAreaInfo(WorldPackets::WowLabs::QueryWowLabsAreaInfo& queryWowLabsAreaInfo);
        void HandleSelectWowLabsArea(WorldPackets::WowLabs::SelectWowLabsArea& selectWowLabsArea);
        void HandleRequestCurrencyDataForAccountCharacters(WorldPackets::Misc::RequestCurrencyDataForAccountCharacters& packet);
        void HandleTransferCurrencyFromAccountCharacter(WorldPackets::Misc::TransferCurrencyFromAccountCharacter& packet);
        void HandleGetCharacterCurrencyTransferLog(WorldPackets::Misc::GetCharacterCurrencyTransferLog& packet);

        union ConnectToKey
        {
            struct
            {
                uint64 AccountId : 32;
                uint64 ConnectionType : 1;
                uint64 Key : 31;
            } Fields;

            uint64 Raw;
        };

        uint64 GetConnectToInstanceKey() const { return _instanceConnectKey.Raw; }
        static void AddInstanceConnection(WorldSession* session, std::weak_ptr<WorldSocket> sockRef, ConnectToKey key);

    public:
        QueryCallbackProcessor& GetQueryProcessor() { return _queryProcessor; }
        TransactionCallback& AddTransactionCallback(TransactionCallback&& callback);
        SQLQueryHolderCallback& AddQueryHolderCallback(SQLQueryHolderCallback&& callback);

    private:
        void ProcessQueryCallbacks();

        QueryCallbackProcessor _queryProcessor;
        AsyncCallbackProcessor<TransactionCallback> _transactionCallbacks;
        AsyncCallbackProcessor<SQLQueryHolderCallback> _queryHolderProcessor;

    friend class World;
    protected:
        class DosProtection
        {
            friend class World;
            public:
                DosProtection(WorldSession* s);
                bool EvaluateOpcode(WorldPacket& p, time_t time) const;
            protected:
                enum Policy
                {
                    POLICY_LOG,
                    POLICY_KICK,
                    POLICY_BAN,
                };

                uint32 GetMaxPacketCounterAllowed(uint32 opcode) const;

                WorldSession* Session;

            private:
                Policy _policy;
                typedef std::unordered_map<uint32, PacketCounter> PacketThrottlingMap;
                // mark this member as "mutable" so it can be modified even in const functions
                mutable PacketThrottlingMap _PacketThrottlingMap;

                DosProtection(DosProtection const& right) = delete;
                DosProtection& operator=(DosProtection const& right) = delete;
        } AntiDOS;

    private:
        // private trade methods
        void moveItems(Item* myItems[], Item* hisItems[]);

        bool CanUseBank(ObjectGuid bankerGUID = ObjectGuid::Empty) const;
        // True only if this session is allowed to MUTATE the account-wide (warband) bank right
        // now: it must have a valid Bnet linkage (bnetId != 0) and hold the single-holder
        // account inventory lock. Every account bank mutation opcode gates on this so a second
        // same-bnet session (or an injected packet from one) can never mutate concurrently.
        bool CanMutateAccountBank() const;

        // logging helper
        void LogUnexpectedOpcode(WorldPacket* packet, char const* status, const char *reason);

        // EnumData helpers
        bool IsLegitCharacterForAccount(ObjectGuid lowGUID)
        {
            return _legitCharacters.find(lowGUID) != _legitCharacters.end();
        }

        // Movement helpers
        Unit* ValidateAndGetUnitBeingMoved(ObjectGuid guid, OpcodeClient opcode, bool forStatusAck) const;

        // this stores the GUIDs of the characters who can login
        // characters who failed on Player::BuildEnumData shouldn't login
        GuidSet _legitCharacters;

        ObjectGuid::LowType m_GUIDLow;                      // set logined or recently logout player (while m_playerRecentlyLogout set)
        Player* _player;
        std::array<std::shared_ptr<WorldSocket>, MAX_CONNECTION_TYPES> m_Socket;
        std::string m_Address;                              // Current Remote Address
     // std::string m_LAddress;                             // Last Attempted Remote Adress - we can not set attempted ip for a non-existing session!

        AccountTypes _security;
        uint32 _accountId;
        std::string _accountName;
        std::unique_ptr<Battlenet::Account> _battlenetAccount;
        uint8 m_accountExpansion;
        uint8 m_expansion;
        std::string _os;
        uint32 _clientBuild;
        ClientBuild::VariantId _clientBuildVariant;

        std::array<uint8, 32> _realmListSecret;
        std::unordered_map<uint32 /*realmAddress*/, uint8> _realmCharacterCounts;
        std::unordered_map<uint32, std::function<void(MessageBuffer)>> _battlenetResponseCallbacks;
        uint32 _battlenetRequestToken;

        time_t _logoutTime;
        bool m_inQueue;                                     // session wait in auth.queue
        ObjectGuid m_playerLoading;                         // code processed in LoginPlayer
        bool m_playerLoginRPE = false;                      // CMSG_PLAYER_LOGIN.RPE - Returning Player ("Catch Up") Experience enter
        bool m_playerLogout;                                // code processed in LogoutPlayer
        bool m_playerRecentlyLogout;
        bool m_playerSave;
        LocaleConstant m_sessionDbcLocale;
        LocaleConstant m_sessionDbLocaleIndex;
        Minutes _timezoneOffset;
        std::atomic<uint32> m_latency;
        ClientPerformanceStats _clientPerformanceStats;
        // CMSG_LOG_STREAMING_ERROR is a free-form string from an authenticated but otherwise untrusted client and
        // the client keeps a 64 slot error ring it can drain in a burst, so the log output is capped per session.
        // Reports past the cap keep being counted and ~WorldSession prints the total once the cap was reached;
        // otherwise the operator would see exactly ten lines with no hint that hundreds followed. The cap lives
        // here rather than in the handler so the handler and its reader cannot drift apart.
        static constexpr uint32 MaxStreamingErrorsLoggedPerSession = 10;
        uint32 _streamingErrorsReported = 0;
        AccountData _accountData[NUM_ACCOUNT_DATA_TYPES];
        std::array<uint32, MAX_ACCOUNT_TUTORIAL_VALUES> _tutorials;
        uint8 _tutorialsChanged;

        std::unordered_map<uint32 /*instanceId*/, SystemTimePoint/*releaseTime*/> _instanceResetTimes;

        uint32 _challengeModeTotalLeaves;
        SystemTimePoint _challengeModeLeaverPenaltyExpiration;

        PlayerDataAccount _playerDataAccount;
        std::vector<std::string> _registeredAddonPrefixes;
        bool _filterAddonMessages;
        // Garrison login prologue (FOLLOWER_FATIGUE_CLEARED + FOLLOWER_ACTIVATIONS_SET) is sniff-confirmed
        // to be sent only before the FIRST GetGarrisonInfo result of a session.
        bool _sentGarrisonLoginPrologue = false;
        uint32 recruiterId;
        bool isRecruiter;
        bool _isCommentator = false;                        // account is currently in commentator (spectator) mode
        LockedQueue<WorldPacket*> _recvQueue;
        rbac::RBACData* _RBACData;
        uint32 expireTime;
        bool forceExit;

        std::unique_ptr<boost::circular_buffer<std::pair<int64, uint32>>> _timeSyncClockDeltaQueue; // first member: clockDelta. Second member: latency of the packet exchange that was used to compute that clockDelta.
        int64 _timeSyncClockDelta;
        void ComputeNewClockDelta();

        std::map<uint32, int64> _pendingTimeSyncRequests; // key: counter. value: server time when packet with that counter was sent.
        uint32 _timeSyncNextCounter;
        // Serial of the SMSG_SUSPEND_COMMS this session is waiting to have acknowledged. Empty means no suspend
        // is outstanding and every CMSG_SUSPEND_COMMS_ACK is unsolicited.
        Optional<uint32> _suspendCommsPendingSerial;
        uint32 _suspendCommsNextSerial = 1;
        // Rejected CMSG_SUSPEND_COMMS_ACK packets, and the cap on how many of them get a log line. Same reasoning
        // as MaxStreamingErrorsLoggedPerSession above: an authenticated client can send this opcode as often as
        // the shared DosProtection default lets it (it has no entry of its own in GetMaxPacketCounterAllowed), and
        // because SendSuspendComms has no caller today, EVERY ack that arrives is a rejection. Without a
        // cap that is a client controlled tap on the log. Rejections past the cap keep being counted and
        // ~WorldSession prints the total, so the counter has a reader for every value it can take: 1..cap from
        // the per rejection lines in HandleSuspendCommsAck, everything above it from the destructor - and both
        // sit on network.telemetry, the one network logger worldserver.conf.dist actually enables, so "reader"
        // means visible as shipped and not merely present in the source.
        static constexpr uint32 MaxSuspendCommsAcksLoggedPerSession = 10;
        uint32 _suspendCommsAcksRejected = 0;
        uint32 _timeSyncTimer;
        bool _timeSyncRestartedByClient; // CMSG_TIME_SYNC_RESPONSE_FAILED is honoured at most once per world entry.

        // Damping for the movement force repair in HandleMoveRemoveMovementForces. Guid-agnostic on
        // purpose: a per-force cooldown is defeated by a client that alternates two guids, a session
        // wide budget is not, and it needs no bookkeeping that grows with the session.
        static constexpr Milliseconds MOVEMENT_FORCE_REPAIR_WINDOW = Milliseconds(1000);
        static constexpr uint32 MOVEMENT_FORCE_REPAIR_BURST = 5;
        TimePoint _movementForceRepairWindowStart;
        uint32 _movementForceRepairCount;
        bool _movementForceRepairThrottleLogged;

        // Packets cooldown
        time_t _calendarEventCreationCooldown;

        std::vector<PendingInviteConfirmation> _pendingInviteConfirmations;
        TimePoint _nextSuggestedInviteTime = TimePoint::min();
        // Who the suggestion that opened the running cooldown was about. Without it the cooldown
        // cannot tell a repetition from an unrelated second suggestion and swallows both in
        // silence; with it only the repetition is swallowed. See SendSuggestedInvite.
        ObjectGuid _lastSuggestedInviteTarget;
        // Battlemaster gossip rate limit, see CheckBattlegroundInfoThrottle. Session local and volatile on
        // purpose: nothing behind SMSG_BATTLEGROUND_INFO_THROTTLED outlives the connection.
        TimePoint _battlegroundInfoThrottlePeriodEnd = TimePoint::min();
        uint32 _battlegroundInfoRequestsRemaining = 0;

        std::unique_ptr<BattlePets::BattlePetMgr> _battlePetMgr;

        std::unique_ptr<CollectionMgr> _collectionMgr;

        // Chat messages this session is holding back until the player confirms or drops them
        // (SMSG_CAUTIONARY_CHAT_MESSAGE). Session state on purpose - it must not survive a relog.
        std::unique_ptr<ChatCautionMgr> _chatCautionMgr;
        bool _chatCautionAccepted;      ///< true while re-running a confirmed held message: stops it being held again and stops the flood brake counting it twice

        // Distinct reporters that told us this session's player tripped their client side spam
        // filter (CMSG_CHAT_REPORT_FILTERED), each with the point in time at which its report stops
        // counting. One entry per reporter, so a single client cannot inflate the count, and a
        // sliding window, so reports scattered over a whole session do not add up to a mute.
        std::map<ObjectGuid, time_t> _chatSpamFilterReporters;

        // Whispers this session delivered while the realm had `chat_spam_record` patterns to ship -
        // the only whispers a client can legitimately report as filtered - with the point in time
        // at which each stops being reportable. This is the server side precondition of
        // CMSG_CHAT_REPORT_FILTERED: without it any session that knows a GUID could report any
        // online player, because the opcode's whole trigger otherwise lives in the client.
        std::map<ObjectGuid, time_t> _filterableWhispers;
        int64 _accountPerksTender = -1;   // cached account-wide Trader's Tender balance; -1 = not loaded / no row yet
        uint64 _accountPerksCacheGrantPeriod = 0;   // interval the base monthly Tender was last granted for this account

        ConnectToKey _instanceConnectKey;

        // Einheit w4_cmsg_43_3D - Sitzungszustand der Sammelfamilien 0x43 / 0x3D.
        // Beide sind bewusst FLUECHTIG (DoD D4): der Client haelt sie als CVar bzw. als
        // UI-Zustand und sendet sie nach jedem Login erneut - CMSG_SET_EXCLUDED_CHAT_CENSOR_SOURCES
        // ist mit 22 Paketen ueber vier Builds genau so am Draht belegt.
        // Beide werden bisher von nichts gelesen; die Handler vermerken das als D2 = offen.
        // CMSG_LOW_LEVEL_RAID1 stand hier ebenfalls und gehoert NICHT hierher: sein Traeger ist
        // PLAYER_FLAGS_LOW_LEVEL_RAID_ENABLED in characters.playerFlags.
        uint8 _excludedChatCensorSources;    // Enum.ExcludedCensorSources als Bitmaske
        bool _quickJoinAutoAcceptRequests;

        // B3 - Entprellung von CMSG_ENGINE_SURVEY. Retail sendet das 294-Byte-Hardwareprofil
        // genau einmal je (Schema-Version, Client-Patch) und damit hoechstens einmal je Sitzung;
        // jede Wiederholung wird verworfen. Das Profil selbst wird NICHT abgelegt (D4 fluechtig),
        // solange es keinen Auswertungspfad gibt - siehe MiscHandler.cpp, Blockkopf B3.
        bool _engineSurveyReceived;

        // B3 - Drossel fuer CMSG_REPORT_SERVER_LAG. Bewusst eine Sitzungsdrossel und KEIN Eintrag
        // in DosProtection::GetMaxPacketCounterAllowed: der Eintrag drosselt nicht, er verwirft das
        // ueberzaehlige Paket ungelesen und trennt die Sitzung (Vorgabe POLICY_KICK). GMReportLag
        // haengt an einem Knopf, und ein Doppelklick darf keinen Spieler die Sitzung kosten.
        time_t _lastReportServerLagTime;

        // Der Baum hat kein Anti-Cheat. SMSG_WARDEN3_DISABLED geht genau EINMAL je Sitzung
        // hinaus - der Client schickt sonst rund 1400 Datenpakete ins Leere.
        bool _warden3DisabledSent;
        static constexpr uint32 MaxDebugViews = 40;
        std::bitset<MaxDebugViews> _debugViewSubscriptions;

        WorldSession(WorldSession const& right) = delete;
        WorldSession& operator=(WorldSession const& right) = delete;
};

#endif
