/*
 * Copyright 2011-2016 "Silver Squirrel Software Handelsbolag"
 * Copyright 2023-2024 "John Högberg"
 *
 * This file is part of tibiarc.
 *
 * tibiarc is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * tibiarc is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with tibiarc. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __TRC_PARSER_HPP__
#define __TRC_PARSER_HPP__

#include "datareader.hpp"
#include "events.hpp"
#include "position.hpp"

#include <unordered_set>
#include <memory>
#include <list>

namespace trc {

class Parser {
public:
    using EventList = std::list<std::unique_ptr<trc::Events::Base>>;

    Parser(const trc::Version &version, bool repair)
        : Version_(version), Repair_(repair) {
    }

    EventList Parse(DataReader &reader);

    /* Tibiacast recordings start with a set of creature initialization
     * packets outside of the Tibia data stream, so we expose this to mark them
     * as known. */
    void MarkCreatureKnown(uint32_t id) {
        (void)KnownCreatures_.insert(id);
    }

private:
    const trc::Version &Version_;

    std::unordered_set<uint32_t> KnownCreatures_;
    trc::Position Position_;
    [[maybe_unused]] bool Repair_;

    struct Repair {};

    void ParseNext(DataReader &reader, Repair &repair, EventList &events);

    trc::Position ParsePosition(DataReader &reader);
    Appearance ParseAppearance(DataReader &reader);
    void ParseItem(DataReader &reader, Object &object);
    void ParseCreatureSeen(DataReader &reader,
                           EventList &events,
                           Object &object);
    void ParseCreatureUpdated(DataReader &reader,
                              EventList &events,
                              Object &object);
    void ParseCreatureCompact(DataReader &reader,
                              EventList &events,
                              Object &object);
    void ParseObject(DataReader &reader, EventList &events, Object &object);
    uint16_t ParseTileDescription(DataReader &reader,
                                  EventList &events,
                                  trc::Events::TileUpdated &event);
    uint16_t ParseFloorDescription(DataReader &reader,
                                   EventList &events,
                                   int X,
                                   int Y,
                                   int Z,
                                   int width,
                                   int height,
                                   int offset,
                                   uint16_t tileSkip);
    void ParseMapDescription(DataReader &reader,
                             EventList &events,
                             int xOffset,
                             int yOffset,
                             int width,
                             int height);

    void ParseAmbientLight(DataReader &reader, EventList &events);
    void ParseBlessings(DataReader &reader, EventList &events);
    void ParseCancelAttack(DataReader &reader, EventList &events);
    void ParseChannelClose(DataReader &reader, EventList &events);
    void ParseChannelEvent(DataReader &reader, EventList &events);
    void ParseChannelList(DataReader &reader, EventList &events);
    void ParseChannelOpen(DataReader &reader, EventList &events);
    void ParseContainerAddItem(DataReader &reader, EventList &events);
    void ParseContainerClose(DataReader &reader, EventList &events);
    void ParseContainerOpen(DataReader &reader, EventList &events);
    void ParseContainerRemoveItem(DataReader &reader, EventList &events);
    void ParseContainerTransformItem(DataReader &reader, EventList &events);
    void ParseCreatureGuildMembersOnline(DataReader &reader, EventList &events);
    void ParseCreatureHealth(DataReader &reader, EventList &events);
    void ParseCreatureImpassable(DataReader &reader, EventList &events);
    void ParseCreatureLight(DataReader &reader, EventList &events);
    void ParseCreatureOutfit(DataReader &reader, EventList &events);
    void ParseCreaturePvPHelpers(DataReader &reader, EventList &events);
    void ParseCreatureShield(DataReader &reader, EventList &events);
    void ParseCreatureSkull(DataReader &reader, EventList &events);
    void ParseCreatureSpeak(DataReader &reader, EventList &events);
    void ParseCreatureSpeed(DataReader &reader, EventList &events);
    void ParseCreatureType(DataReader &reader, EventList &events);
    void ParseDeathDialog(DataReader &reader, EventList &events);
    void ParseFloorChangeDown(DataReader &reader, EventList &events);
    void ParseFloorChangeUp(DataReader &reader, EventList &events);
    void ParseFullMapDescription(DataReader &reader, EventList &events);
    void ParseGMActions(DataReader &reader, EventList &events);
    void ParseGraphicalEffect(DataReader &reader, EventList &events);
    void ParseHotkeyPresets(DataReader &reader, EventList &events);
    void ParseInitialization(DataReader &reader, EventList &events);
    void ParseInventoryClearSlot(DataReader &reader, EventList &events);
    void ParseInventorySetSlot(DataReader &reader, EventList &events);
    void ParseMarkCreature(DataReader &reader, EventList &events);
    void ParseMarketBrowse(DataReader &reader, EventList &events);
    void ParseMarketInitialization(DataReader &reader, EventList &events);
    void ParseMarketItemDetails(DataReader &reader, EventList &events);
    void ParseMinimapFlag(DataReader &reader, EventList &events);
    void ParseMissileEffect(DataReader &reader, EventList &events);
    void ParseMoveDelay(DataReader &reader, EventList &events);
    void ParseMoveDenied(DataReader &reader, EventList &events);
    void ParseMoveEast(DataReader &reader, EventList &events);
    void ParseMoveNorth(DataReader &reader, EventList &events);
    void ParseMoveSouth(DataReader &reader, EventList &events);
    void ParseMoveWest(DataReader &reader, EventList &events);
    void ParseNPCVendorBegin(DataReader &reader, EventList &events);
    void ParseNPCVendorPlayerGoods(DataReader &reader, EventList &events);
    void ParseOffenseReportResponse(DataReader &reader, EventList &events);
    void ParseOpenEditList(DataReader &reader, EventList &events);
    void ParseOpenEditText(DataReader &reader, EventList &events);
    void ParseOpenHouseWindow(DataReader &reader, EventList &events);
    void ParseOpenPrivateConversation(DataReader &reader, EventList &events);
    void ParseOpenPvPSituations(DataReader &reader, EventList &events);
    void ParseOutfitDialog(DataReader &reader, EventList &events);
    void ParsePlayerDataBasic(DataReader &reader, EventList &events);
    void ParsePlayerDataCurrent(DataReader &reader, EventList &events);
    void ParsePlayerIcons(DataReader &reader, EventList &events);
    void ParsePlayerInventory(DataReader &reader, EventList &events);
    void ParsePlayerSkills(DataReader &reader, EventList &events);
    void ParsePlayerTactics(DataReader &reader, EventList &events);
    void ParsePlayerTradeItems(DataReader &reader, EventList &events);
    void ParsePremiumTrigger(DataReader &reader, EventList &events);
    void ParseQuestDialog(DataReader &reader, EventList &events);
    void ParseQuestDialogMission(DataReader &reader, EventList &events);
    void ParseRuleViolation(DataReader &reader, EventList &events);
    void ParseSpellCooldown(DataReader &reader, EventList &events);
    void ParseTextEffect(DataReader &reader, EventList &events);
    void ParseTextMessage(DataReader &reader, EventList &events);
    void ParseTileAddObject(DataReader &reader, EventList &events);
    void ParseTileMoveCreature(DataReader &reader, EventList &events);
    void ParseTileRemoveObject(DataReader &reader, EventList &events);
    void ParseTileSetObject(DataReader &reader, EventList &events);
    void ParseTileUpdate(DataReader &reader, EventList &events);
    void ParseTrappers(DataReader &reader, EventList &events);
    void ParseTutorialShow(DataReader &reader, EventList &events);
    void ParseUnjustifiedPoints(DataReader &reader, EventList &events);
    void ParseUseCooldown(DataReader &reader, EventList &events);
    void ParseVIPOffline(DataReader &reader, EventList &events);
    void ParseVIPOnline(DataReader &reader, EventList &events);
    void ParseVIPStatus(DataReader &reader, EventList &events);
};

} // namespace trc

#endif /* __TRC_PARSER_HPP__ */
