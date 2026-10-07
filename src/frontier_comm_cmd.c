#include "frontier_comm_cmd.h"

#include <nitro.h>

#include "constants/battle_tower.h"

#include "struct_defs/battle_tower.h"
#include "struct_defs/comm_cmd_table.h"

#include "applications/frontier/battle_arcade/main.h"
#include "applications/frontier/battle_castle/opponent_app.h"
#include "applications/frontier/battle_castle/self_app.h"
#include "applications/frontier/battle_factory/main.h"
#include "applications/frontier/battle_hall/main.h"
#include "overlay104/frontier_communication.h"
#include "overlay104/wfc_facility_selector_helpers.h"

#include "battle_frontier_save.h"
#include "communication_system.h"
#include "party.h"
#include "pokemon.h"
#include "comm_cmd.h"

static void BattleTower_HandlePartnerDataCmd(int netId, int unused, void *data, void *context);
static void BattleTower_HandleTrainerIDListCmd(int netID, int unused, void *data, void *context);
static void BattleTower_HandlePartnerReadyCmd(int netId, int unused, void *data, void *context);
static void WFCFacilitySelector_HandleFacilityAndStreakCmd(int netID, int unused, void *data, void *context);
static void WFCFacilitySelector_HandleDidDropOutCmd(int netID, int unused, void *data, void *context);
static void WFCFacilitySelector_HandleSelectedMonsCmd(int netID, int unused, void *data, void *context);
static void WFCFacilitySelector_HandleStreakDeletionChoiceCmd(int netID, int unused, void *data, void *context);
static void WFCFacilitySelector_HandlePlayAgainCmd(int netID, int unused, void *data, void *context);

// Application-specific comm command table registered through CommCmd_Init. The
// dispatcher offsets into this table by COMM_CMD_BUILTIN_COUNT (22), so entry
// [0] handles command 22 and entry [40] handles command 62. The Battle Tower
// entries at indices 40..42 (commands 62..64) and the WFC facility selector
// entries at indices 35..39 (commands 57..61) are implemented in this file; the
// remaining entries are provided by the individual Battle Frontier
// applications.
static const CommCmdTable sFrontierCommCmdTable[] = {
    { ov104_0222EF30, CommPacketSizeOf_Variable, NULL },
    { FactoryCommunication_ReceiveTrainers, CommPacketSizeOf_Variable, NULL },
    { ov104_0222F03C, CommPacketSizeOf_Variable, NULL },
    { ov104_0222F124, CommPacketSizeOf_Variable, NULL },
    { ov104_0222F1C4, CommPacketSizeOf_Variable, NULL },
    { ov104_0222F210, CommPacketSizeOf_Variable, NULL },
    { ov104_0222F31C, CommPacketSizeOf_Variable, NULL },
    { BattleFactoryApp_DummyCommCommand, CommPacketSizeOf_Variable, NULL },
    { BattleFactoryApp_HandleSelectionUpdateCmd, CommPacketSizeOf_Variable, NULL },
    { BattleFactoryApp_DummyCommCommand2, CommPacketSizeOf_Variable, NULL },
    { BattleFactoryApp_HandleTradeResultCmd, CommPacketSizeOf_Variable, NULL },
    { FrontierCommunication_Unreachable1, CommPacketSizeOf_Variable, NULL },
    { FrontierCommunication_Unreachable3, CommPacketSizeOf_Variable, NULL },
    { HallCommunication_ReceiveTrainers, CommPacketSizeOf_Variable, NULL },
    { HallCommunication_ReceiveOpponentMons, CommPacketSizeOf_Variable, NULL },
    { ov104_0222EE38, CommPacketSizeOf_Variable, NULL },
    { HallCommunication_ReceivePartnersPokemon, CommPacketSizeOf_Variable, HallCommunication_VerifyPacketSize },
    { BattleHall_DummyCommCommand, CommPacketSizeOf_Variable, NULL },
    { BattleHall_HandleTypeSelectionMsg, CommPacketSizeOf_Variable, NULL },
    { BattleHall_HandlePartnerDecisionCmd, CommPacketSizeOf_Variable, NULL },
    { CastleCommunication_ReceivePartnersCP, CommPacketSizeOf_Variable, NULL },
    { CastleCommunications_ReceiveTrainers, CommPacketSizeOf_Variable, NULL },
    { ov104_0222F530, CommPacketSizeOf_Variable, NULL },
    { ov104_0222F650, CommPacketSizeOf_Variable, NULL },
    { ov104_0222F6E8, CommPacketSizeOf_Variable, NULL },
    { FrontierCommunication_Unreachable5, CommPacketSizeOf_Variable, NULL },
    { CastleCommunication_ReceivePartnersParty, CommPacketSizeOf_Variable, CastleCommunication_VerifyPacketSize },
    { BattleCastleSelfApp_HandlePlayerInfoCmd, CommPacketSizeOf_Variable, NULL },
    { BattleCastleSelfApp_HandlePurchaseInfoCmd, CommPacketSizeOf_Variable, NULL },
    { BattleCastleSelfApp_HandleUpdateCursorCmd, CommPacketSizeOf_Variable, NULL },
    { BattleCastleSelfApp_HandleExitAppCmd, CommPacketSizeOf_Variable, NULL },
    { BattleCastleOpponentApp_HandlePlayerInfoCmd, CommPacketSizeOf_Variable, NULL },
    { BattleCastleOpponentApp_HandlePurchaseInfoCmd, CommPacketSizeOf_Variable, NULL },
    { BattleCastleOpponentApp_HandleUpdateCursorCmd, CommPacketSizeOf_Variable, NULL },
    { BattleCastleOpponentApp_HandleExitAppCmd, CommPacketSizeOf_Variable, NULL },
    { WFCFacilitySelector_HandleFacilityAndStreakCmd, CommPacketSizeOf_Variable, NULL },
    { WFCFacilitySelector_HandleDidDropOutCmd, CommPacketSizeOf_Variable, NULL },
    { WFCFacilitySelector_HandleSelectedMonsCmd, CommPacketSizeOf_Variable, NULL },
    { WFCFacilitySelector_HandleStreakDeletionChoiceCmd, CommPacketSizeOf_Variable, NULL },
    { WFCFacilitySelector_HandlePlayAgainCmd, CommPacketSizeOf_Variable, NULL },
    { BattleTower_HandlePartnerDataCmd, CommPacketSizeOf_Variable, NULL },
    { BattleTower_HandleTrainerIDListCmd, CommPacketSizeOf_Variable, NULL },
    { BattleTower_HandlePartnerReadyCmd, CommPacketSizeOf_Variable, NULL },
    { ov104_0222F8A0, CommPacketSizeOf_Variable, NULL },
    { ArcadeCommunication_ReceiveTrainers, CommPacketSizeOf_Variable, NULL },
    { ov104_0222F9C0, CommPacketSizeOf_Variable, NULL },
    { ov104_0222FA5C, CommPacketSizeOf_Variable, NULL },
    { ov104_0222FAA8, CommPacketSizeOf_Variable, NULL },
    { ArcadeCommunication_ReceivePartnersParty, CommPacketSizeOf_Variable, ArcadeCommunication_VerifyPacketSize },
    { BattleArcadeApp_HandleInitialLayoutCmd, CommPacketSizeOf_Variable, NULL },
    { BattleArcadeApp_HandleResultCmd, CommPacketSizeOf_Variable, NULL },
    { BattleArcadeApp_HandleUnusedCmd, CommPacketSizeOf_Variable, NULL }
};

// Registers the Battle Frontier comm command table. Each facility application
// (and the WFC facility selector) calls this once its comm manager is ready;
// `context` is the facility state forwarded to every command handler.
void FrontierCommCmd_Init(void *context)
{
    int cmdCount = sizeof(sFrontierCommCmdTable) / sizeof(CommCmdTable);
    CommCmd_Init(sFrontierCommCmdTable, cmdCount, context);
}

// Command 62: receives the link partner's Battle Salon packet, laid out as
// [gender, species0, species1, roomNum]. Stores the partner's identity and
// computes a bitmask of species shared with the player's own party (bit 0 for
// party slot 0, bit 1 for party slot 1) into the command result.
static void BattleTower_HandlePartnerDataCmd(int netId, int unused, void *data, void *context)
{
    u16 speciesOverlap;
    BattleTower *battleTower = context;
    const u16 *partnerData = data;

    speciesOverlap = 0;
    battleTower->msgsReceived++;

    if (CommSys_CurNetId() == netId) {
        return;
    }

    battleTower->partnerGender = (u8)partnerData[0];
    battleTower->unk_16[0] = partnerData[1];
    battleTower->unk_16[1] = partnerData[2];
    battleTower->unk_14 = partnerData[3];
    battleTower->partnerID = BT_PARTNERS_COUNT + battleTower->partnerGender;

    if ((battleTower->unk_2E[0] == battleTower->unk_16[0]) || (battleTower->unk_2E[0] == battleTower->unk_16[1])) {
        speciesOverlap += 1;
    }

    if ((battleTower->unk_2E[1] == battleTower->unk_16[0]) || (battleTower->unk_2E[1] == battleTower->unk_16[1])) {
        speciesOverlap += 2;
    }

    battleTower->unk_8D8 = speciesOverlap;
}

// Command 63: sends the generated opponent trainer IDs to the link partner.
BOOL BattleTower_SendTrainerIDListCmd(BattleTower *battleTower)
{
    int dataSize = BT_OPPONENTS_COUNT * 2 * sizeof(u16);
    MI_CpuCopy8(battleTower->trainerIDs, battleTower->unk_83E, dataSize);

    return CommSys_SendData(63, battleTower->unk_83E, dataSize) == TRUE;
}

// Command 63: receives the link partner's opponent trainer IDs. The host (net
// ID 0) does not accept them, so only the guest copies the packet into its
// BattleTower state.
static void BattleTower_HandleTrainerIDListCmd(int netID, int unused, void *data, void *context)
{
    BattleTower *battleTower = context;
    const u16 *trainerIDs = data;

    battleTower->msgsReceived++;

    if (CommSys_CurNetId() == netID) {
        return;
    }

    if (CommSys_CurNetId() == 0) {
        return;
    }

    MI_CpuCopy8(trainerIDs, battleTower->trainerIDs, BT_OPPONENTS_COUNT * 2 * sizeof(u16));
}

// Command 64: receives the link partner's ready flag. The result is 1 when
// either the local partner data was marked ready or the received packet's first
// word is non-zero.
static void BattleTower_HandlePartnerReadyCmd(int netId, int unused, void *data, void *context)
{
    BattleTower *battleTower = context;
    const u16 *partnerData = data;

    battleTower->unk_8D8 = 0;
    battleTower->msgsReceived++;

    if (CommSys_CurNetId() == netId) {
        return;
    }

    if (battleTower->unk_10_3 || partnerData[0]) {
        battleTower->unk_8D8 = 1;
    }
}

// Command 57: sends the selected facility and the host's latest streak index.
BOOL WFCFacilitySelector_SendFacilityAndLatestStreak(WFCFacilitySelector *selector)
{
    selector->commBuffer[0] = selector->selectedFacility;

    int streakIndex = BattleFrontier_GetWFCLatestStreakIndex(selector->selectedFacility);
    selector->commBuffer[1] = BattleFrontierSave_GetStatAutoHostIdx(SaveData_GetBattleFrontier(selector->saveData), streakIndex);

    return CommSys_SendData(57, selector->commBuffer, 40) == TRUE;
}

// Command 57: receives the partner's selected facility and latest streak index.
static void WFCFacilitySelector_HandleFacilityAndStreakCmd(int netID, int unused, void *data, void *context)
{
    WFCFacilitySelector *selector = context;
    const u16 *payload = data;

    selector->msgsReceived++;

    if (CommSys_CurNetId() == netID) {
        return;
    }

    selector->partnersSelectedFacility = payload[0];
    selector->partnersLatestStreak = payload[1];
}

// Command 58: sends whether the player dropped out of the current run.
BOOL WFCFacilitySelector_SendDidDropOutCmd(WFCFacilitySelector *selector, u16 didDropOut)
{
    selector->commBuffer[0] = didDropOut;
    return CommSys_SendData(58, selector->commBuffer, 40) == TRUE;
}

// Command 58: receives whether the partner dropped out of the current run.
static void WFCFacilitySelector_HandleDidDropOutCmd(int netID, int unused, void *data, void *context)
{
    WFCFacilitySelector *selector = context;
    const u16 *payload = data;

    selector->msgsReceived++;

    if (CommSys_CurNetId() == netID) {
        return;
    }

    selector->partnerDroppedOut = payload[0];
}

// Command 59: sends the species and held items of the two selected party
// members. A slot of 0xff means no selection, which is sent as zeroes.
BOOL WFCFacilitySelector_SendSelectedMons(WFCFacilitySelector *selector, u16 selectedSlot1, u16 selectedSlot2)
{
    Party *party = SaveData_GetParty(selector->saveData);

    selector->selectedMonSlots[0] = selectedSlot1;
    selector->selectedMonSlots[1] = selectedSlot2;

    if (selectedSlot1 == 0xff) {
        selector->selectedSpecies[0] = 0;
        selector->selectedItems[0] = 0;
        selector->selectedSpecies[1] = 0;
        selector->selectedItems[1] = 0;
    } else {
        Pokemon *mon = Party_GetPokemonBySlotIndex(party, selectedSlot1);

        selector->selectedSpecies[0] = Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL);
        selector->selectedItems[0] = Pokemon_GetValue(mon, MON_DATA_HELD_ITEM, NULL);

        mon = Party_GetPokemonBySlotIndex(party, selectedSlot2);

        selector->selectedSpecies[1] = Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL);
        selector->selectedItems[1] = Pokemon_GetValue(mon, MON_DATA_HELD_ITEM, NULL);
    }

    selector->commBuffer[0] = selector->selectedSpecies[0];
    selector->commBuffer[1] = selector->selectedItems[0];
    selector->commBuffer[2] = selector->selectedSpecies[1];
    selector->commBuffer[3] = selector->selectedItems[1];

    return CommSys_SendData(59, selector->commBuffer, 40) == TRUE;
}

// Command 59: receives the partner's selected species and held items.
static void WFCFacilitySelector_HandleSelectedMonsCmd(int netID, int unused, void *data, void *context)
{
    WFCFacilitySelector *selector = context;
    const u16 *payload = data;

    selector->msgsReceived++;

    if (CommSys_CurNetId() == netID) {
        return;
    }

    selector->partnersSelectedSpecies[0] = payload[0];
    selector->partnersSelectedItems[0] = payload[1];
    selector->partnersSelectedSpecies[1] = payload[2];
    selector->partnersSelectedItems[1] = payload[3];
}

// Command 60: sends the player's choice about deleting the current streak.
BOOL WFCFacilitySelector_SendStreakDeletionChoice(WFCFacilitySelector *selector, u16 streakDeletionChoice)
{
    selector->commBuffer[0] = streakDeletionChoice;
    return CommSys_SendData(60, selector->commBuffer, 40) == TRUE;
}

// Command 60: receives the partner's streak deletion choice.
static void WFCFacilitySelector_HandleStreakDeletionChoiceCmd(int netID, int unused, void *data, void *context)
{
    WFCFacilitySelector *selector = context;
    const u16 *payload = data;

    selector->msgsReceived++;

    if (CommSys_CurNetId() == netID) {
        return;
    }

    selector->partnersStreakDeletionChoice = payload[0];
}

// Command 61: sends whether the player declined to play again.
BOOL WFCFacilitySelector_SendPlayAgainChoice(WFCFacilitySelector *selector, u16 notPlayingAgain)
{
    selector->commBuffer[0] = notPlayingAgain;
    return CommSys_SendData(61, selector->commBuffer, 40) == TRUE;
}

// Command 61: receives whether the partner declined to play again.
static void WFCFacilitySelector_HandlePlayAgainCmd(int netID, int unused, void *data, void *context)
{
    WFCFacilitySelector *selector = context;
    const u16 *payload = data;

    selector->msgsReceived++;

    if (CommSys_CurNetId() == netID) {
        return;
    }

    selector->partnerNotPlayingAgain = payload[0];
}
