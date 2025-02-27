/*
 * Copyright 2011-2016 "Silver Squirrel Software Handelsbolag"
 * Copyright 2023-2025 "John Högberg"
 *
 * This file is part of tibiarc.
 *
 * tibiarc is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later Version_.
 *
 * tibiarc is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with tibiarc. If not, see <https://www.gnu.org/licenses/>.
 */

#include "datareader.hpp"
#include "types.hpp"
#include "versions.hpp"
#include "events.hpp"
#include "parser.hpp"

#include <stdio.h>
#include <math.h>

#include "utils.hpp"

#include <unordered_set>
#include <vector>

#define ParseAssert(Expr)                                                      \
    if (!(Expr)) {                                                             \
        throw InvalidDataError();                                              \
    }

namespace trc {

using namespace trc::Events;

template <typename T> static T &AddEvent(Parser::EventList &events) {
    return static_cast<T &>(*events.emplace_back(std::make_unique<T>()));
}

Position Parser::ParsePosition(DataReader &reader) {
    auto x = reader.ReadU16<Map::TileBufferWidth,
                            std::numeric_limits<uint16_t>::max() -
                                    Map::TileBufferWidth>();
    auto y = reader.ReadU16<Map::TileBufferHeight,
                            std::numeric_limits<uint16_t>::max() -
                                    Map::TileBufferHeight>();
    auto z = reader.ReadU8<0, 15>();

    return trc::Position(x, y, z);
}

Appearance Parser::ParseAppearance(DataReader &reader) {
    /* FIXME: C++ migration, null initialization? */
    Appearance outfit;

    if (Version_.Protocol.OutfitsU16) {
        outfit.Id = reader.ReadU16();
    } else {
        outfit.Id = reader.ReadU8();
    }

    if (outfit.Id == 0) {
        /* Extra information like stack count or fluid color is omitted when
         * items are used as outfits, so we shouldn't use `ParseObject`
         * here. */
        outfit.Item.Id = reader.ReadU16();
        outfit.Item.ExtraByte = 0;

        if (outfit.Item.Id != 0) {
            /* Assertion. */
            (void)Version_.GetItem(outfit.Item.Id);
        }
    } else {
        /* Assertion. */
        (void)Version_.GetOutfit(outfit.Id);

        outfit.HeadColor = reader.ReadU8();
        outfit.PrimaryColor = reader.ReadU8();
        outfit.SecondaryColor = reader.ReadU8();
        outfit.DetailColor = reader.ReadU8();

        if (Version_.Protocol.OutfitAddons) {
            outfit.Addons = reader.ReadU8();
        }
    }

    if (Version_.Protocol.Mounts) {
        outfit.MountId = reader.ReadU16();

        if (outfit.MountId != 0) {
            /* Assertion. */
            (void)Version_.GetOutfit(outfit.MountId);
        }
    } else {
        outfit.MountId = 0;
    }

    return outfit;
}

void Parser::ParseCreatureSeen(DataReader &reader,
                               EventList &events,
                               Object &object) {
    Assert(object.Id == 0x61);

    auto removeId = reader.ReadU32();
    auto addId = reader.ReadU32();

    if (addId != removeId) {
        if (KnownCreatures_.erase(removeId) == 1) {
            auto &event = AddEvent<CreatureRemoved>(events);

            event.CreatureId = removeId;
        }
    }

    /* 0x61 for a known creature is not a protocol violation; in some
     * versions it's the only way to update Impassable. */
    (void)KnownCreatures_.insert(addId);

    auto &event = AddEvent<CreatureSeen>(events);

    object.CreatureId = addId;
    event.CreatureId = addId;

    if (Version_.Protocol.CreatureTypes) {
        event.Type = reader.Read<CreatureType>();
    } else if (event.CreatureId < 0x10000000) {
        /* In these old versions, all player creatures had an identifier below
         * this magic number. */
        event.Type = CreatureType::Player;
    } else {
        event.Type = CreatureType::Monster;
    }

    event.Name = reader.ReadString();
    event.Health = reader.ReadU8();

    event.Heading = reader.Read<Creature::Direction>();
    event.Outfit = ParseAppearance(reader);

    event.LightIntensity = reader.ReadU8();
    event.LightColor = reader.ReadU8();
    event.Speed = reader.ReadU16();

    if (Version_.Protocol.SkullIcon) {
        event.Skull = reader.Read<CharacterSkull>();
    }

    if (Version_.Protocol.ShieldIcon) {
        event.Shield = reader.Read<PartyShield>();
    }

    if (Version_.Protocol.WarIcon) {
        event.War = reader.Read<WarIcon>();
    }

    if (Version_.Protocol.CreatureMarks) {
        ParseAssert(event.Type == reader.Read<CreatureType>());

        if (Version_.Protocol.NPCCategory) {
            event.NPCCategory = reader.Read<NPCCategory>();
        }

        event.Mark = reader.ReadU8();
        event.GuildMembersOnline = reader.ReadU16();
        event.MarkIsPermanent = true;
    }

    if (Version_.Protocol.PassableCreatures) {
        event.Impassable = reader.ReadU8();
    }
}

void Parser::ParseCreatureUpdated(DataReader &reader,
                                  EventList &events,
                                  Object &object) {
    Assert(object.Id == 0x62);

    object.CreatureId = reader.ReadU32();

    {
        auto &event = AddEvent<CreatureHealthUpdated>(events);

        event.CreatureId = object.CreatureId;
        event.Health = reader.ReadU8();
    }

    {
        auto &event = AddEvent<CreatureHeadingUpdated>(events);

        event.CreatureId = object.CreatureId;
        event.Heading = reader.Read<Creature::Direction>();
    }

    {
        auto &event = AddEvent<CreatureOutfitUpdated>(events);

        event.CreatureId = object.CreatureId;
        event.Outfit = ParseAppearance(reader);
    }

    {
        auto &event = AddEvent<CreatureLightUpdated>(events);

        event.CreatureId = object.CreatureId;

        event.Intensity = reader.ReadU8();
        event.Color = reader.ReadU8();
    }

    {
        auto &event = AddEvent<CreatureSpeedUpdated>(events);

        event.CreatureId = object.CreatureId;
        event.Speed = reader.ReadU16();
    }

    if (Version_.Protocol.SkullIcon) {
        auto &event = AddEvent<CreatureSkullUpdated>(events);

        event.CreatureId = object.CreatureId;
        event.Skull = reader.Read<CharacterSkull>();
    }

    if (Version_.Protocol.ShieldIcon) {
        auto &event = AddEvent<CreatureShieldUpdated>(events);

        event.CreatureId = object.CreatureId;
        event.Shield = reader.Read<PartyShield>();
    }

    if (Version_.Protocol.CreatureMarks) {
        {
            auto &event = AddEvent<CreatureTypeUpdated>(events);

            event.CreatureId = object.CreatureId;
            event.Type = reader.Read<CreatureType>();
        }

        if (Version_.Protocol.NPCCategory) {
            auto &event = AddEvent<CreatureNPCCategoryUpdated>(events);

            event.CreatureId = object.CreatureId;
            event.Category = reader.Read<NPCCategory>();
        }

        {
            auto &event = AddEvent<CreaturePvPHelpersUpdated>(events);

            event.CreatureId = object.CreatureId;

            event.Mark = reader.ReadU8();
            event.MarkIsPermanent = true;
        }

        {
            auto &event = AddEvent<CreatureGuildMembersUpdated>(events);

            event.CreatureId = object.CreatureId;
            event.GuildMembersOnline = reader.ReadU16();
        }
    }

    if (Version_.Protocol.PassableCreatures) {
        auto &event = AddEvent<CreatureImpassableUpdated>(events);

        event.CreatureId = object.CreatureId;
        event.Impassable = reader.ReadU8<0, 1>();
    }
}

void Parser::ParseCreatureCompact(DataReader &reader,
                                  EventList &events,
                                  Object &object) {
    object.CreatureId = reader.ReadU32();

    {
        auto &event = AddEvent<CreatureHeadingUpdated>(events);

        event.CreatureId = object.CreatureId;
        event.Heading = reader.Read<Creature::Direction>();
    }

    if (Version_.Protocol.PassableCreatureUpdate) {
        auto &event = AddEvent<CreatureImpassableUpdated>(events);

        event.CreatureId = object.CreatureId;
        event.Impassable = reader.ReadU8<0, 1>();
    }
}

void Parser::ParseItem(DataReader &reader, Object &object) {
    const auto &type = Version_.GetItem(object.Id);

    if (Version_.Protocol.ItemMarks) {
        object.Mark = reader.ReadU8();
    } else {
        object.Mark = 255;
    }

    if ((type.Properties.LiquidContainer || type.Properties.LiquidPool)) {
        object.ExtraByte = reader.ReadU8();

        /* Assertion. */
        (void)Version_.TranslateFluidColor(object.ExtraByte);
    } else if (type.Properties.Stackable) {
        object.ExtraByte = reader.ReadU8();
    } else if (type.Properties.Rune && Version_.Protocol.RuneChargeCount) {
        object.ExtraByte = reader.ReadU8();
    } else {
        /* Fall back to a count of 1 in case this item has become stackable in
         * later versions. */
        object.ExtraByte = 1;
    }

    if (Version_.Protocol.ItemAnimation && type.Properties.Animated) {
        object.Animation = reader.ReadU8();
    } else {
        object.Animation = 0;
    }
}

void Parser::ParseObject(DataReader &reader,
                         EventList &events,
                         Object &object) {
    object.Id = reader.ReadU16();

    switch (object.Id) {
    case 0:
        if (Version_.Protocol.NullObjects) {
            throw InvalidDataError();
        }
        break;
    case 0x61:
        ParseCreatureSeen(reader, events, object);
        object.Id = Object::CreatureMarker;
        break;
    case 0x62:
        ParseCreatureUpdated(reader, events, object);
        object.Id = Object::CreatureMarker;
        break;
    case 0x63:
        ParseCreatureCompact(reader, events, object);
        object.Id = Object::CreatureMarker;
        break;
    default:
        ParseItem(reader, object);
    }
}

uint16_t Parser::ParseTileDescription(DataReader &reader,
                                      EventList &events,
                                      TileUpdated &event) {
    auto peekValue = reader.Peek<uint16_t>();

    if (Version_.Protocol.EnvironmentalEffects) {
        /* This is either a tile skip or an environmental effect. Since we
         * haven't implemented rendering for the latter, just ignore it. */
        if (peekValue < 0xFF00) {
            reader.SkipU16();
            peekValue = reader.Peek<uint16_t>();
        }
    }

    while (peekValue < 0xFF00) {
        auto &object = event.Objects.emplace_back();
        ParseObject(reader, events, object);

        peekValue = reader.Peek<uint16_t>();
    }

    peekValue = reader.ReadU16();
    return peekValue & 0xFF;
}

uint16_t Parser::ParseFloorDescription(DataReader &reader,
                                       EventList &events,
                                       int X,
                                       int Y,
                                       int Z,
                                       int width,
                                       int height,
                                       int offset,
                                       uint16_t tileSkip) {
    for (int xIdx = X + offset; xIdx <= (X + offset + width - 1); xIdx++) {
        for (int yIdx = Y + offset; yIdx <= (Y + offset + height - 1); yIdx++) {
            if (tileSkip == 0) {
                auto &event = AddEvent<TileUpdated>(events);

                event.Position = trc::Position(xIdx, yIdx, Z);

                tileSkip = ParseTileDescription(reader, events, event);
            } else {
                auto &event = AddEvent<TileUpdated>(events);

                event.Position = trc::Position(xIdx, yIdx, Z);

                tileSkip--;
            }
        }
    }

    return tileSkip;
}

void Parser::ParseMapDescription(DataReader &reader,
                                 EventList &events,
                                 int xOffset,
                                 int yOffset,
                                 int width,
                                 int height) {
    int zIdx, endZ, zStep;
    uint16_t tileSkip;

    if (Position_.Z > 7) {
        zIdx = Position_.Z - 2;
        endZ = std::min<int>(15, Position_.Z + 2);
        zStep = 1;
    } else {
        zIdx = 7;
        endZ = 0;
        zStep = -1;
    }

    tileSkip = 0;

    for (; zIdx != (endZ + zStep); zIdx += zStep) {
        tileSkip = ParseFloorDescription(reader,
                                         events,
                                         Position_.X + xOffset,
                                         Position_.Y + yOffset,
                                         zIdx,
                                         width,
                                         height,
                                         Position_.Z - zIdx,
                                         tileSkip);
    }

    ParseAssert(tileSkip == 0);
}

void Parser::ParseTileUpdate(DataReader &reader, EventList &events) {
    auto &event = AddEvent<TileUpdated>(events);

    event.Position = ParsePosition(reader);

    auto tileSkip = ParseTileDescription(reader, events, event);
    ParseAssert(tileSkip == 0);
}

void Parser::ParseTileAddObject(DataReader &reader, EventList &events) {
    auto &event = AddEvent<TileObjectAdded>(events);

    event.TilePosition = ParsePosition(reader);

    if (Version_.Protocol.AddObjectStackPosition) {
        event.StackPosition = reader.ReadU8();
    } else {
        event.StackPosition = Tile::StackPositionTop;
    }

    ParseObject(reader, events, event.Object);
}

void Parser::ParseTileSetObject(DataReader &reader, EventList &events) {
    auto peekValue = reader.Peek<uint16_t>();

    if (peekValue != 0xFFFF) {
        auto &event = AddEvent<TileObjectTransformed>(events);

        event.TilePosition = ParsePosition(reader);
        event.StackPosition = reader.ReadU8();

        ParseAssert(event.StackPosition < Tile::MaxObjects);

        ParseObject(reader, events, event.Object);
    } else {
        ParseAssert(Version_.Features.ModernStacking);
        Object dummyObject;

        reader.SkipU16();
        auto creatureId = reader.ReadU32();

        ParseAssert(KnownCreatures_.contains(creatureId));

        ParseObject(reader, events, dummyObject);
    }
}

void Parser::ParseTileRemoveObject(DataReader &reader, EventList &events) {
    auto peekValue = reader.Peek<uint16_t>();

    if (peekValue != 0xFFFF) {
        auto &event = AddEvent<TileObjectRemoved>(events);

        event.TilePosition = ParsePosition(reader);
        event.StackPosition = reader.ReadU8();

        ParseAssert(event.StackPosition < Tile::MaxObjects);
    } else {
        ParseAssert(Version_.Features.ModernStacking);

        reader.SkipU16();
        auto creatureId = reader.ReadU32();

        ParseAssert(KnownCreatures_.contains(creatureId));
    }
}

void Parser::ParseTileMoveCreature(DataReader &reader, EventList &events) {
    auto peekValue = reader.Peek<uint16_t>();

    auto &event = AddEvent<CreatureMoved>(events);

    if (peekValue != 0xFFFF) {
        event.From = ParsePosition(reader);
        event.StackPosition = reader.ReadU8();
    } else {
        ParseAssert(Version_.Features.ModernStacking);

        reader.SkipU16();
        auto creatureId = reader.ReadU32();

        ParseAssert(KnownCreatures_.contains(creatureId));

        event.StackPosition = Tile::StackPositionTop;

        event.From.X = 0xFFFF;
        event.From.Y = 0xFFFF;
        event.From.Z = 0xFF;
    }

    event.To = ParsePosition(reader);
}

void Parser::ParseFullMapDescription(DataReader &reader, EventList &events) {
    auto &event = AddEvent<PlayerMoved>(events);

    event.Position = Position_ = ParsePosition(reader);

    ParseMapDescription(reader,
                        events,
                        -8,
                        -6,
                        Map::TileBufferWidth,
                        Map::TileBufferHeight);
}

void Parser::ParseInitialization(DataReader &reader, EventList &events) {
    auto &event = AddEvent<WorldInitialized>(events);

    event.PlayerId = reader.ReadU32();
    event.BeatDuration = reader.ReadU16();

    if (Version_.Protocol.SpeedAdjustment) {
        event.SpeedA = reader.ReadFloat();
        event.SpeedB = reader.ReadFloat();
        event.SpeedC = reader.ReadFloat();
    }

    if (Version_.Protocol.BugReporting) {
        event.AllowBugReports = reader.ReadU8();
    }

    if (Version_.Protocol.PvPFraming) {
        event.PvPFraming = reader.ReadU8();
    }

    if (Version_.Protocol.ExpertMode) {
        event.ExpertMode = reader.ReadU8();
    }

    /* This is Tibiacast-specific, for versions where it accidentally
     * generated buggy initialization packets. */
    if (Version_.Protocol.TibiacastBuggedInitialization) {
        reader.SkipU8();
    }
}

void Parser::ParseGMActions(DataReader &reader,
                            [[maybe_unused]] EventList &events) {
    unsigned skipCount;

    if (Version_.AtLeast(8, 50)) {
        skipCount = 19;
    } else if (Version_.AtLeast(8, 41)) {
        skipCount = 22;
    } else if (Version_.AtLeast(8, 40)) {
        /* Wild guess based on one TTM file; this is a rare packet type so it's
         * hard to say whether this is correct. */
        skipCount = 27;
    } else if (Version_.AtLeast(8, 30)) {
        /* Wild guess based on YATC code. */
        skipCount = 28;
    } else if (Version_.AtLeast(7, 40)) {
        skipCount = 32;
    } else {
        /* Actual value seen in 7.30 recording, may need further tweaks. */
        skipCount = 30;
    }

    reader.Skip(skipCount);
}

void Parser::ParseContainerOpen(DataReader &reader, EventList &events) {
    auto &event = AddEvent<ContainerOpened>(events);

    event.ContainerId = reader.ReadU8();
    event.ItemId = reader.ReadU16();

    if (Version_.Protocol.ItemMarks) {
        event.Mark = reader.ReadU8();
    }

    const auto &containerType = Version_.GetItem(event.ItemId);
    if (Version_.Protocol.ItemAnimation && containerType.Properties.Animated) {
        event.Animation = reader.ReadU8();
    }

    event.Name = reader.ReadString();
    event.SlotsPerPage = reader.ReadU8();
    event.HasParent = reader.ReadU8();

    if (Version_.Protocol.ContainerPagination) {
        event.DragAndDrop = reader.ReadU8();
        event.Pagination = reader.ReadU8();
        event.TotalObjects = reader.ReadU16();
        event.StartIndex = reader.ReadU16();
    }

    auto itemCount = reader.ReadU8();

    if (!Version_.Protocol.ContainerPagination) {
        event.TotalObjects = itemCount;
    }

    for (auto idx = 0u; idx < itemCount; idx++) {
        auto &item = event.Items.emplace_back();
        ParseObject(reader, events, item);
    }
}

void Parser::ParseContainerClose(DataReader &reader, EventList &events) {
    auto &event = AddEvent<ContainerClosed>(events);

    event.ContainerId = reader.ReadU8();
}

void Parser::ParseContainerAddItem(DataReader &reader, EventList &events) {
    auto &event = AddEvent<ContainerAddedItem>(events);

    event.ContainerId = reader.ReadU8();

    if (Version_.Protocol.ContainerIndexU16) {
        /* Note that container index is only present at all in versions that
         * have 16-wide indexes. */
        event.ContainerIndex = reader.ReadU16();
    } else {
        event.ContainerIndex = 0;
    }

    ParseObject(reader, events, event.Item);
}

void Parser::ParseContainerTransformItem(DataReader &reader,
                                         EventList &events) {
    auto &event = AddEvent<ContainerTransformedItem>(events);

    event.ContainerId = reader.ReadU8();

    if (Version_.Protocol.ContainerIndexU16) {
        event.ContainerIndex = reader.ReadU16();
    } else {
        event.ContainerIndex = reader.ReadU8();
    }

    ParseObject(reader, events, event.Item);
}

void Parser::ParseContainerRemoveItem(DataReader &reader, EventList &events) {
    auto &event = AddEvent<ContainerRemovedItem>(events);

    event.ContainerId = reader.ReadU8();

    if (Version_.Protocol.ContainerIndexU16) {
        event.ContainerIndex = reader.ReadU16();
        ParseObject(reader, events, event.Backfill);
    } else {
        event.ContainerIndex = reader.ReadU8();
        event.Backfill.Id = 0;
    }

    /* Referencing a nonexistent container or poking at the wrong index is not
     * a protocol violation. */
}

void Parser::ParseInventorySetSlot(DataReader &reader, EventList &events) {
    auto &event = AddEvent<PlayerInventoryUpdated>(events);

    event.Slot = reader.Read<InventorySlot>();
    ParseObject(reader, events, event.Item);
}

void Parser::ParseInventoryClearSlot(DataReader &reader, EventList &events) {
    auto &event = AddEvent<PlayerInventoryUpdated>(events);

    event.Slot = reader.Read<InventorySlot>();
    event.Item = {};
}

void Parser::ParseNPCVendorBegin(DataReader &reader,
                                 [[maybe_unused]] EventList &events) {
    uint16_t itemCount;

    if (Version_.Protocol.NPCVendorName) {
        reader.SkipString();
    }

    if (Version_.Protocol.NPCVendorItemCountU16) {
        itemCount = reader.ReadU16();
    } else {
        itemCount = reader.ReadU8();
    }

    while (itemCount--) {
        /* itemId */
        reader.SkipU16();
        /* extraByte */
        reader.SkipU8();
        reader.SkipString();

        if (Version_.Protocol.NPCVendorWeight) {
            reader.SkipU32();
        }

        /* buyPrice */
        reader.SkipU32();
        /* sellPrice */
        reader.SkipU32();
    }
}

void Parser::ParseNPCVendorPlayerGoods(DataReader &reader,
                                       [[maybe_unused]] EventList &events) {
    uint8_t itemCount;

    if (Version_.Protocol.PlayerMoneyU64) {
        reader.SkipU64();
    } else {
        reader.SkipU32();
    }

    itemCount = reader.ReadU8();

    while (itemCount--) {
        /* itemId */
        reader.SkipU16();
        /* extraByte */
        reader.SkipU8();
    }
}

void Parser::ParsePlayerTradeItems(DataReader &reader, EventList &events) {
    uint8_t itemCount;

    reader.SkipString();
    itemCount = reader.ReadU8();

    while (itemCount--) {
        Object nullObject;
        ParseObject(reader, events, nullObject);
    }
}

void Parser::ParseAmbientLight(DataReader &reader, EventList &events) {
    auto &event = AddEvent<AmbientLightChanged>(events);

    event.Intensity = reader.ReadU8();
    event.Color = reader.ReadU8();
}

void Parser::ParseGraphicalEffect(DataReader &reader, EventList &events) {
    auto &event = AddEvent<GraphicalEffectPopped>(events);

    event.Position = ParsePosition(reader);
    event.Id = reader.ReadU8();

    if (!Version_.Protocol.RawEffectIds) {
        event.Id += 1;
    }

    /* Assertion. */
    (void)Version_.GetEffect(event.Id);
}

void Parser::ParseMissileEffect(DataReader &reader, EventList &events) {
    auto &event = AddEvent<MissileFired>(events);

    event.Origin = ParsePosition(reader);
    event.Target = ParsePosition(reader);
    event.Id = reader.ReadU8();

    if (!Version_.Protocol.RawEffectIds) {
        event.Id += 1;
    }

    /* Assertion. */
    (void)Version_.GetMissile(event.Id);
}

void Parser::ParseTextEffect(DataReader &reader, EventList &events) {
    /* Text effects were replaced by message effects, so we've misparsed a
     * previous packet if we land here on a version that uses the latter. */
    ParseAssert(!Version_.Protocol.MessageEffects);

    auto &event = AddEvent<NumberEffectPopped>(events);

    event.Position = ParsePosition(reader);
    event.Color = reader.ReadU8();

    auto message = reader.ReadString();
    ParseAssert(sscanf(message.c_str(), "%u", &event.Value) == 1);
}

void Parser::ParseMarkCreature(DataReader &reader,
                               [[maybe_unused]] EventList &events) {
    /* creature id, color */
    reader.SkipU32();
    reader.SkipU8();
}

void Parser::ParseTrappers(DataReader &reader,
                           [[maybe_unused]] EventList &events) {
    auto count = reader.ReadU8();

    /* Creature ids */
    reader.Skip(count * sizeof(uint32_t));
}

void Parser::ParseCreatureHealth(DataReader &reader, EventList &events) {
    auto creatureId = reader.ReadU32();
    auto health = reader.ReadU8();

    if (KnownCreatures_.contains(creatureId)) {
        auto &event = AddEvent<CreatureHealthUpdated>(events);

        event.CreatureId = creatureId;
        event.Health = health;
    }
}

void Parser::ParseCreatureLight(DataReader &reader, EventList &events) {
    auto creatureId = reader.ReadU32();
    auto intensity = reader.ReadU8();
    auto color = reader.ReadU8();

    if (KnownCreatures_.contains(creatureId)) {
        auto &event = AddEvent<CreatureLightUpdated>(events);

        event.CreatureId = creatureId;
        event.Intensity = intensity;
        event.Color = color;
    }
}

void Parser::ParseCreatureOutfit(DataReader &reader, EventList &events) {
    auto creatureId = reader.ReadU32();
    auto outfit = ParseAppearance(reader);

    if (KnownCreatures_.contains(creatureId)) {
        auto &event = AddEvent<CreatureOutfitUpdated>(events);

        event.CreatureId = creatureId;
        event.Outfit = outfit;
    }
}

void Parser::ParseCreatureSpeed(DataReader &reader, EventList &events) {
    auto creatureId = reader.ReadU32();
    auto speed = reader.ReadU16();

    if (KnownCreatures_.contains(creatureId)) {
        auto &event = AddEvent<CreatureSpeedUpdated>(events);

        event.CreatureId = creatureId;
        event.Speed = speed;
    }

    if (Version_.Protocol.CreatureSpeedPadding) {
        reader.SkipU16();
    }
}

void Parser::ParseCreatureSkull(DataReader &reader, EventList &events) {
    auto creatureId = reader.ReadU32();
    auto skull = reader.Read<CharacterSkull>();

    if (KnownCreatures_.contains(creatureId)) {
        auto &event = AddEvent<CreatureSkullUpdated>(events);

        event.CreatureId = creatureId;
        event.Skull = skull;
    }
}

void Parser::ParseCreatureShield(DataReader &reader, EventList &events) {
    auto creatureId = reader.ReadU32();
    auto shield = reader.Read<PartyShield>();

    if (KnownCreatures_.contains(creatureId)) {
        auto &event = AddEvent<CreatureShieldUpdated>(events);

        event.CreatureId = creatureId;
        event.Shield = shield;
    }
}

void Parser::ParseCreatureImpassable(DataReader &reader, EventList &events) {
    ParseAssert(Version_.Protocol.PassableCreatures);

    auto creatureId = reader.ReadU32();
    auto impassable = reader.ReadU8<0, 1>();

    if (KnownCreatures_.contains(creatureId)) {
        auto &event = AddEvent<CreatureImpassableUpdated>(events);

        event.CreatureId = creatureId;
        event.Impassable = impassable;
    }
}

void Parser::ParseCreaturePvPHelpers(DataReader &reader, EventList &events) {
    auto creatureCount = 1;

    if (!Version_.Protocol.SinglePvPHelper) {
        creatureCount = reader.ReadU8();
    }

    while (creatureCount--) {
        auto creatureId = reader.ReadU32();
        auto markIsPermanent = reader.ReadU8<0, 1>();
        auto mark = reader.ReadU8();

        if (KnownCreatures_.contains(creatureId)) {
            auto &event = AddEvent<CreaturePvPHelpersUpdated>(events);

            event.CreatureId = creatureId;
            event.MarkIsPermanent = markIsPermanent;
            event.Mark = mark;
        }
    }
}

void Parser::ParseCreatureGuildMembersOnline(DataReader &reader,
                                             EventList &events) {
    auto creatureId = reader.ReadU32();
    auto guildMembersOnline = reader.ReadU16();

    if (KnownCreatures_.contains(creatureId)) {
        auto &event = AddEvent<CreatureGuildMembersUpdated>(events);

        event.CreatureId = creatureId;
        event.GuildMembersOnline = guildMembersOnline;
    }
}

void Parser::ParseCreatureType(DataReader &reader, EventList &events) {
    auto creatureId = reader.ReadU32();
    auto type = reader.Read<CreatureType>();

    if (KnownCreatures_.contains(creatureId)) {
        auto &event = AddEvent<CreatureTypeUpdated>(events);

        event.CreatureId = creatureId;
        event.Type = type;
    }
}

void Parser::ParseOpenEditText(DataReader &reader, EventList &events) {
    if (Version_.Protocol.TextEditObject) {
        Object textObject;
        ParseObject(reader, events, textObject);
    } else {
        /* Window id, item id? */
        reader.SkipU32();
        reader.SkipU16();
    }

    reader.SkipU16();
    reader.SkipString();

    if (Version_.Protocol.TextEditAuthorName) {
        reader.SkipString();
    }

    if (Version_.Protocol.TextEditDate) {
        reader.SkipString();
    }
}

void Parser::ParseOpenHouseWindow(DataReader &reader,
                                  [[maybe_unused]] EventList &events) {
    /* Kind + window id? */
    reader.SkipU8();
    reader.SkipU32();
    reader.SkipString();
}

void Parser::ParseBlessings(DataReader &reader, EventList &events) {
    auto &event = AddEvent<PlayerBlessingsUpdated>(events);

    event.Blessings = reader.ReadU16();
}

void Parser::ParseHotkeyPresets(DataReader &reader, EventList &events) {
    auto &event = AddEvent<PlayerHotkeyPresetUpdated>(events);

    event.HotkeyPreset = reader.ReadU32();
}

void Parser::ParseOpenEditList(DataReader &reader,
                               [[maybe_unused]] EventList &events) {
    /* type */
    reader.SkipU8();
    /* id */
    reader.SkipU16();
    reader.SkipString();
}

void Parser::ParsePremiumTrigger(DataReader &reader,
                                 [[maybe_unused]] EventList &events) {
    auto count = reader.ReadU8();

    reader.Skip(count + 1);
}

void Parser::ParsePlayerDataBasic(DataReader &reader, EventList &events) {
    auto &event = AddEvent<PlayerDataBasicUpdated>(events);

    event.IsPremium = reader.ReadU8();

    if (Version_.Protocol.PremiumUntil) {
        event.PremiumUntil = reader.ReadU32();
    }

    event.Vocation = reader.ReadU8();

    auto spellCount = reader.ReadU16();
    while (spellCount--) {
        event.Spells.push_back(reader.ReadU16());
    }
}

void Parser::ParsePlayerDataCurrent(DataReader &reader, EventList &events) {
    auto &event = AddEvent<PlayerDataUpdated>(events);

    event.Health = reader.ReadS16();
    event.MaxHealth = reader.ReadS16();

    if (Version_.Protocol.CapacityU32) {
        event.Capacity = reader.ReadU32();

        if (Version_.Protocol.MaxCapacity) {
            event.MaxCapacity = reader.ReadU32();
        }
    } else {
        event.Capacity = reader.ReadU16();
    }

    if (Version_.Protocol.ExperienceU64) {
        event.Experience = reader.ReadU64();
    } else {
        event.Experience = reader.ReadU32();
    }

    if (Version_.Protocol.LevelU16) {
        event.Level = reader.ReadU16();
    } else {
        event.Level = reader.ReadU8();
    }

    if (Version_.Protocol.SkillPercentages) {
        event.LevelPercent = reader.ReadU8<0, 100>();
    }

    if (Version_.Protocol.ExperienceBonus) {
        event.ExperienceBonus = reader.ReadFloat();
    }

    event.Mana = reader.ReadS16();
    event.MaxMana = reader.ReadS16();

    /* Mana can be negative for de-leveled mages. */
    ParseAssert(CheckRange(event.Mana, 0, event.MaxMana) ||
                (event.MaxMana < 0 && event.Mana == 0));

    event.MagicLevel = reader.ReadU8();

    if (Version_.Protocol.SkillBonuses) {
        event.MagicLevelBase = reader.ReadU8();
    } else {
        event.MagicLevelBase = event.MagicLevel;
    }

    if (Version_.Protocol.SkillPercentages) {
        event.MagicLevelPercent = reader.ReadU8<0, 100>();
    }

    if (Version_.Protocol.SoulPoints) {
        event.SoulPoints = reader.ReadU8<0, 200>();
    }

    if (Version_.Protocol.Stamina) {
        event.Stamina = reader.ReadU16();
    }

    if (Version_.Protocol.PlayerSpeed) {
        event.Speed = reader.ReadU16();
    }

    if (Version_.Protocol.PlayerHunger) {
        event.Fed = reader.ReadU16();
    }

    if (Version_.Protocol.OfflineStamina) {
        event.OfflineStamina = reader.ReadU16();
    }
}

void Parser::ParsePlayerSkills(DataReader &reader, EventList &events) {
    auto &event = AddEvent<PlayerSkillsUpdated>(events);

    for (int i = 0; i < PLAYER_SKILL_COUNT; i++) {
        if (Version_.Protocol.SkillsU16) {
            event.Skills[i].Effective = reader.ReadU16();
            event.Skills[i].Actual = reader.ReadU16();
            event.Skills[i].Percent = reader.ReadU8();
        } else {
            event.Skills[i].Effective = reader.ReadU8();

            if (Version_.Protocol.SkillBonuses) {
                event.Skills[i].Actual = reader.ReadU8();
            } else {
                event.Skills[i].Actual = event.Skills[i].Effective;
            }

            if (Version_.Protocol.SkillPercentages) {
                event.Skills[i].Percent = reader.ReadU8<0, 100>();
            } else {
                event.Skills[i].Percent = 0;
            }
        }
    }

    if (Version_.Protocol.SkillsUnknownPadding) {
        reader.Skip(6 * 4);
    }
}

void Parser::ParsePlayerIcons(DataReader &reader, EventList &events) {
    auto &event = AddEvent<PlayerIconsUpdated>(events);

    /* Make sure we don't forget to add a ParseAssert if this is widened in a
     * later version (>11.x?). */
    static_assert(sizeof(std::underlying_type<StatusIcon>::type) == 2,
                  "Status icons must fit in 16 bits");
    if (Version_.Protocol.IconsU16) {
        event.Icons = (StatusIcon)reader.ReadU16();
    } else {
        event.Icons = (StatusIcon)reader.ReadU8();
    }
}

void Parser::ParseCancelAttack(DataReader &reader,
                               [[maybe_unused]] EventList &events) {
    if (Version_.Protocol.CancelAttackId) {
        reader.SkipU32();
    }
}

void Parser::ParseSpellCooldown(DataReader &reader,
                                [[maybe_unused]] EventList &events) {
    /* spellId */
    reader.SkipU8();
    /* cooldownTime */
    reader.SkipU32();
}

void Parser::ParseUseCooldown(DataReader &reader,
                              [[maybe_unused]] EventList &events) {
    /* cooldownTime */
    reader.SkipU32();
}

void Parser::ParsePlayerTactics(DataReader &reader, EventList &events) {
    auto &event = AddEvent<PlayerTacticsUpdated>(events);

    event.AttackMode = reader.ReadU8();
    event.ChaseMode = reader.ReadU8();
    event.SecureMode = reader.ReadU8();
    event.PvPMode = reader.ReadU8();
}

static void ValidateTextMessage(
        [[maybe_unused]] MessageMode messageMode,
        [[maybe_unused]] const std::string &message,
        [[maybe_unused]] const std::string &author = std::string()) {
#ifndef NDEBUG
    if (author.size() > 0 && author.at(0) == 'a') {
        /* Names that start with a lowercase "a" or "an" are in all likelyhood
         * a monster, though there are exceptions like the ghostly knight in
         * PoI levers. */
        if (!(messageMode == MessageMode::MonsterSay ||
              messageMode == MessageMode::MonsterYell ||
              ((messageMode == MessageMode::Say ||
                messageMode == MessageMode::NPCStart ||
                messageMode == MessageMode::NPCContinued) &&
               ((author == "a ghostly knight" || author == "a ghostly woman" ||
                 author == "a dead bureaucrat" || author == "a prisoner" ||
                 author == "an old dragonlord" || author == "a ghostly sage" ||
                 author == "a ghostly guardian" ||
                 author == "a wrinkled beholder") ||
                message == "Hicks!")))) {
            throw InvalidDataError();
        }
    }

    switch (messageMode) {
    case MessageMode::MonsterSay:
    case MessageMode::MonsterYell:
        /* We cannot differentiate between a boss (e.g. "Vashresamun") and
         * a player based on their name alone, and some quests use this without
         * an author for effect, e.g. deeper cultist cave on Vandura ("You
         * already know the second verse of the hymn"), so we can't validate
         * these further. */
        break;
    case MessageMode::Broadcast:
    case MessageMode::ChannelOrange:
    case MessageMode::ChannelRed:
    case MessageMode::ChannelWhite:
    case MessageMode::ChannelYellow:
    case MessageMode::GMToPlayer:
    case MessageMode::PlayerToGM:
    case MessageMode::PlayerToNPC:
    case MessageMode::PrivateIn:
    case MessageMode::PrivateOut:
    case MessageMode::Say:
    case MessageMode::Whisper:
    case MessageMode::Yell:
        /* These can contain anything, we don't really have a shot at
         * validating them other than saying that they should have an author
         * name. */
        if (author.empty()) {
            throw InvalidDataError();
        }
        break;
    default: {
        /* Certain texts only happen with certain modes, so we can use them to
         * check whether our versioned message mode mapping is correct. */
        using Spec = std::tuple<std::string, std::vector<MessageMode>>;
        static const std::initializer_list<Spec> checks = {
                {"Message sent to", {MessageMode::Failure}},
                {"Sorry, not possible", {MessageMode::Failure}},
                {"Target lost", {MessageMode::Failure}},
                {"You advanced ", {MessageMode::Game}},
                {"Your last visit in Tibia:", {MessageMode::Login}},
                {"Recorded with ", {MessageMode::Login, MessageMode::Warning}},
                {"You have left the party", {MessageMode::Look}},
                {"You see a", {MessageMode::Look}},
                {"Your party has been", {MessageMode::Look}},
                {"Loot of ", {MessageMode::Look, MessageMode::Loot}},
                /* OpenTibia servers sometimes use Login here. */
                {"You are poisoned", {MessageMode::Status, MessageMode::Login}},
                {"Your depot contains",
                 {MessageMode::Status, MessageMode::Login}},
                {"Server is saving game", {MessageMode::Warning}},
                {"Warning! The murder of ", {MessageMode::Warning}},
                /* Some 8.x recordings consistently use Login for
                 * hotkeys, while others have it under LOOK. I'm at a
                 * loss as to why, perhaps they're on OT? */
                {"Using ",
                 {MessageMode::Look, MessageMode::Hotkey, MessageMode::Login}}};

        for (const auto &[prefix, modes] : checks) {
            if (message.starts_with(prefix) &&
                std::none_of(modes.cbegin(), modes.cend(), [=](auto mode) {
                    return mode == messageMode;
                })) {
                throw InvalidDataError();
            }
        }
    }
    }
#endif
}

void Parser::ParseCreatureSpeak(DataReader &reader, EventList &events) {
    uint32_t messageId = 0;

    if (Version_.Protocol.ReportMessages) {
        messageId = reader.ReadU32();
    }

    std::string authorName = reader.ReadString();

    uint16_t speakerLevel = 0;
    if (Version_.Protocol.SpeakerLevel) {
        speakerLevel = reader.ReadU16();
    }

    auto messageMode = Version_.TranslateSpeakMode(reader.ReadU8());

    switch (messageMode) {
    case MessageMode::Say:
    case MessageMode::Whisper:
    case MessageMode::Yell:
    case MessageMode::Spell:
    case MessageMode::NPCStart:
    case MessageMode::MonsterSay:
    case MessageMode::MonsterYell: {
        auto &event = AddEvent<CreatureSpokeOnMap>(events);

        event.MessageId = messageId;
        event.Mode = messageMode;
        event.AuthorName = authorName;
        event.AuthorLevel = speakerLevel;

        /* There's no need to cut off messages that are seemingly incorrect;
         * the Tibia client displays all received messages regardless of
         * coordinates. */
        event.Position = ParsePosition(reader);
        event.Message = reader.ReadString();

        ValidateTextMessage(event.Mode, event.Message, event.AuthorName);
        break;
    }
    case MessageMode::NPCContinued:
    case MessageMode::Broadcast: {
        auto &event = AddEvent<CreatureSpokeOnMap>(events);

        event.MessageId = messageId;
        event.Mode = messageMode;
        event.AuthorName = authorName;
        event.AuthorLevel = speakerLevel;

        /* These message types use the null position. */
        event.Message = reader.ReadString();

        break;
    }
    case MessageMode::PrivateIn: {
        auto &event = AddEvent<CreatureSpoke>(events);

        event.MessageId = messageId;
        event.Mode = messageMode;
        event.AuthorName = authorName;
        event.AuthorLevel = speakerLevel;
        event.Message = reader.ReadString();

        break;
    }
    case MessageMode::ChannelOrange:
    case MessageMode::ChannelRed:
    case MessageMode::ChannelWhite:
    case MessageMode::ChannelYellow: {
        auto &event = AddEvent<CreatureSpokeInChannel>(events);

        event.MessageId = messageId;
        event.Mode = messageMode;
        event.AuthorName = authorName;
        event.AuthorLevel = speakerLevel;

        event.ChannelId = reader.ReadU16();
        event.Message = reader.ReadString();
        break;
    }
    default:
        throw InvalidDataError();
    }
}

void Parser::ParseChannelList(DataReader &reader, EventList &events) {
    auto &event = AddEvent<ChannelListUpdated>(events);

    auto channelCount = reader.ReadU8();

    while (channelCount--) {
        auto id = reader.ReadU16();
        auto name = reader.ReadString();

        event.Channels.emplace_back(id, name);
    }
}

void Parser::ParseChannelOpen(DataReader &reader, EventList &events) {
    auto &event = AddEvent<ChannelOpened>(events);

    event.Id = reader.ReadU16();
    event.Name = reader.ReadString();

    if (Version_.Protocol.ChannelParticipants) {
        auto participantCount = reader.ReadU16();

        event.Participants.reserve(participantCount);
        while (participantCount--) {
            event.Participants.push_back(reader.ReadString());
        }

        auto inviteeCount = reader.ReadU16();

        event.Invitees.reserve(inviteeCount);
        while (inviteeCount--) {
            event.Invitees.push_back(reader.ReadString());
        }
    }
}

void Parser::ParseChannelClose(DataReader &reader, EventList &events) {
    auto &event = AddEvent<ChannelClosed>(events);

    event.Id = reader.ReadU16();
}

void Parser::ParseOpenPrivateConversation(DataReader &reader,
                                          EventList &events) {
    auto &event = AddEvent<PrivateConversationOpened>(events);

    event.Name = reader.ReadString();
}

void Parser::ParseTextMessage(DataReader &reader, EventList &events) {
    auto messageMode = Version_.TranslateMessageMode(reader.ReadU8());

    switch (messageMode) {
    case MessageMode::Guild:
    case MessageMode::Party:
    case MessageMode::PartyWhite:
        if (!Version_.Protocol.GuildPartyChannelId) {
            break;
        }
        [[fallthrough]];
    case MessageMode::ChannelWhite: {
        auto &event = AddEvent<StatusMessageReceivedInChannel>(events);

        event.Mode = messageMode;
        event.ChannelId = reader.ReadU16();
        event.Message = reader.ReadString();
        return;
    }
    case MessageMode::DamageDealt:
    case MessageMode::DamageReceived:
    case MessageMode::DamageReceivedOthers: {
        if (!Version_.Protocol.MessageEffects) {
            break;
        }

        auto position = ParsePosition(reader);
        auto damageValue = reader.ReadU32();
        auto color = reader.ReadU8();

        if (damageValue > 0) {
            auto &event = AddEvent<NumberEffectPopped>(events);

            event.Position = position;
            event.Color = color;
            event.Value = damageValue;
        }

        damageValue = reader.ReadU32();
        color = reader.ReadU8();

        if (damageValue > 0) {
            auto &event = AddEvent<NumberEffectPopped>(events);

            event.Position = position;
            event.Color = color;
            event.Value = damageValue;
        }
        break;
    }
    case MessageMode::Healing:
    case MessageMode::HealingOthers:
    case MessageMode::Experience:
    case MessageMode::ExperienceOthers:
    case MessageMode::Mana: {
        if (!Version_.Protocol.MessageEffects) {
            break;
        }

        auto position = ParsePosition(reader);
        auto value = reader.ReadU32();
        auto color = reader.ReadU8();

        if (value > 0) {
            auto &event = AddEvent<NumberEffectPopped>(events);

            event.Position = position;
            event.Color = color;
            event.Value = value;
        }

        break;
    }
    case MessageMode::Hotkey:
    case MessageMode::NPCTrade:
    case MessageMode::Game:
    case MessageMode::Look:
    case MessageMode::Loot:
    case MessageMode::Login:
    case MessageMode::Warning:
    case MessageMode::Failure:
    case MessageMode::Status: {
        break;
    }
    default:
        throw InvalidDataError();
    }

    auto &event = AddEvent<StatusMessageReceived>(events);

    event.Mode = messageMode;
    event.Message = reader.ReadString();

    ValidateTextMessage(messageMode, event.Message);
}

void Parser::ParseMoveDenied(DataReader &reader,
                             [[maybe_unused]] EventList &events) {
    if (Version_.Protocol.MoveDeniedDirection) {
        reader.Skip<Creature::Direction>();
    }
}

void Parser::ParseMoveDelay(DataReader &reader,
                            [[maybe_unused]] EventList &events) {
    reader.SkipU16();
}

void Parser::ParseOpenPvPSituations(DataReader &reader, EventList &events) {
    auto &event = AddEvent<PvPSituationsChanged>(events);

    event.OpenSituations = reader.ReadU8();
}

void Parser::ParseUnjustifiedPoints(DataReader &reader,
                                    [[maybe_unused]] EventList &events) {
    /* ProgressDay */
    reader.SkipU8();
    /* KillsRemainingDay */
    reader.SkipU8();
    /* ProgressWeek */
    reader.SkipU8();
    /* KillsRemainingWeek */
    reader.SkipU8();
    /* ProgressMonth */
    reader.SkipU8();
    /* KillsRemainingMonth */
    reader.SkipU8();
    /* SkullDuration */
    reader.SkipU8();
}

void Parser::ParseFloorChangeUp(DataReader &reader, EventList &events) {
    uint16_t tileSkip;

    Position_.Z--;
    tileSkip = 0;

    if (Position_.Z == 7) {
        for (int zIdx = 5; zIdx >= 0; zIdx--) {
            tileSkip = ParseFloorDescription(reader,
                                             events,
                                             Position_.X - 8,
                                             Position_.Y - 6,
                                             zIdx,
                                             Map::TileBufferWidth,
                                             Map::TileBufferHeight,
                                             Map::TileBufferDepth - zIdx,
                                             tileSkip);
        }
    } else if (Position_.Z > 7) {
        tileSkip = ParseFloorDescription(reader,
                                         events,
                                         Position_.X - 8,
                                         Position_.Y - 6,
                                         Position_.Z - 2,
                                         Map::TileBufferWidth,
                                         Map::TileBufferHeight,
                                         3,
                                         tileSkip);
        ParseAssert(tileSkip == 0);
    }

    Position_.X++;
    Position_.Y++;

    auto &event = AddEvent<PlayerMoved>(events);
    event.Position = Position_;
}

void Parser::ParseFloorChangeDown(DataReader &reader, EventList &events) {
    uint16_t tileSkip;

    Position_.Z++;
    tileSkip = 0;

    if (Position_.Z == 8) {
        for (int zIdx = Position_.Z, offset = -1; zIdx <= Position_.Z + 2;
             zIdx++) {
            tileSkip = ParseFloorDescription(reader,
                                             events,
                                             Position_.X - 8,
                                             Position_.Y - 6,
                                             zIdx,
                                             Map::TileBufferWidth,
                                             Map::TileBufferHeight,
                                             offset,
                                             tileSkip);

            offset--;
        }
    } else if (Position_.Z > 7 && Position_.Z < 14) {
        tileSkip = ParseFloorDescription(reader,
                                         events,
                                         Position_.X - 8,
                                         Position_.Y - 6,
                                         Position_.Z + 2,
                                         Map::TileBufferWidth,
                                         Map::TileBufferHeight,
                                         -3,
                                         tileSkip);
        ParseAssert(tileSkip == 0);
    }

    Position_.X--;
    Position_.Y--;

    auto &event = AddEvent<PlayerMoved>(events);
    event.Position = Position_;
}

void Parser::ParseOutfitDialog(DataReader &reader,
                               [[maybe_unused]] EventList &events) {
    (void)ParseAppearance(reader);

    if (Version_.Protocol.OutfitAddons) {
        uint16_t outfitCount;

        if (Version_.Protocol.OutfitCountU16) {
            outfitCount = reader.ReadU16();
        } else {
            outfitCount = reader.ReadU8();
        }

        while (outfitCount--) {
            /* Appearance Id */
            reader.SkipU16();
            if (Version_.Protocol.OutfitNames) {
                reader.SkipString();
            }
            /* Appearance addon */
            reader.SkipU8();
        }
    } else if (Version_.Protocol.OutfitsU16) {
        /* Start outfit, end outfit */
        reader.Skip(4);
    } else {
        /* Start outfit, end outfit */
        reader.Skip(2);
    }

    if (Version_.Protocol.Mounts) {
        uint16_t mountCount;

        if (Version_.Protocol.OutfitCountU16) {
            mountCount = reader.ReadU16();
        } else {
            mountCount = reader.ReadU8();
        }

        while (mountCount--) {
            /* Mount Id */
            reader.SkipU16();
            /* Mount name */
            reader.SkipString();
        }
    }
}

void Parser::ParseVIPStatus(DataReader &reader,
                            [[maybe_unused]] EventList &events) {
    /* player id */
    reader.SkipU32();
    /* player name */
    reader.SkipString();

    if (Version_.Protocol.ExtendedVIPData) {
        reader.SkipString();
        reader.SkipU32();
        reader.SkipU8();
    }

    /* isOnline */
    reader.SkipU8();
}

void Parser::ParseVIPOnline(DataReader &reader,
                            [[maybe_unused]] EventList &events) {
    /* player id */
    reader.SkipU32();

    if (Version_.Protocol.ExtendedVIPData) {
        /* Online/offline. */
        reader.SkipU8();
    }
}

void Parser::ParseVIPOffline(DataReader &reader,
                             [[maybe_unused]] EventList &events) {
    /* This whole packet type is replaced by a boolean field in
     * `ParseVIPOnline`. */
    ParseAssert(!Version_.Protocol.ExtendedVIPData);

    /* player id */
    reader.SkipU32();
}

void Parser::ParseTutorialShow(DataReader &reader,
                               [[maybe_unused]] EventList &events) {
    /* tutorial id */
    reader.SkipU8();
}

void Parser::ParseMinimapFlag(DataReader &reader,
                              [[maybe_unused]] EventList &events) {
    (void)ParsePosition(reader);

    /* flag id */
    reader.SkipU8();
    /* description */
    reader.SkipString();
}

void Parser::ParseQuestDialog(DataReader &reader,
                              [[maybe_unused]] EventList &events) {
    auto questCount = reader.ReadU16();

    while (questCount--) {
        /* quest id */
        reader.SkipU16();
        /* title */
        reader.SkipString();
        /* completion state */
        reader.SkipU8();
    }
}

void Parser::ParseQuestDialogMission(DataReader &reader,
                                     [[maybe_unused]] EventList &events) {
    /* quest id */
    reader.SkipU16();
    auto missionCount = reader.ReadU8();

    while (missionCount--) {
        reader.SkipString();
        reader.SkipString();
    }
}

void Parser::ParseOffenseReportResponse(DataReader &reader,
                                        [[maybe_unused]] EventList &events) {
    reader.SkipString();
}

void Parser::ParseChannelEvent(DataReader &reader,
                               [[maybe_unused]] EventList &events) {
    /* channelId */
    reader.SkipU16();
    /* ??? */
    reader.SkipString();
    /* eventType */
    reader.SkipU8();
}

void Parser::ParseMarketInitialization(DataReader &reader,
                                       [[maybe_unused]] EventList &events) {
    if (Version_.Protocol.PlayerMoneyU64) {
        reader.SkipU64();
    } else {
        reader.SkipU32();
    }

    auto itemTypeCount = reader.ReadU16();
    /* vocationId */
    reader.SkipU8();

    while (itemTypeCount--) {
        /* itemId */
        reader.SkipU16();
        /* itemCount */
        reader.SkipU16();
    }
}

void Parser::ParseMarketItemDetails(DataReader &reader,
                                    [[maybe_unused]] EventList &events) {
    uint8_t buyOfferIterator, sellOfferIterator;
    int propertyIterator;

    /* itemId */
    reader.SkipU16();

    propertyIterator = 15;

    while (propertyIterator--) {
        reader.SkipString();
    }

    buyOfferIterator = reader.ReadU8();

    while (buyOfferIterator--) {
        /* offerCount */
        reader.SkipU32();
        /* lowestBid */
        reader.SkipU32();
        /* averageBid */
        reader.SkipU32();
        /* highestBid */
        reader.SkipU32();
    }

    sellOfferIterator = reader.ReadU8();

    while (sellOfferIterator--) {
        /* offerCount */
        reader.SkipU32();
        /* lowestBid */
        reader.SkipU32();
        /* averageBid */
        reader.SkipU32();
        /* highestBid */
        reader.SkipU32();
    }
}

void Parser::ParseMarketBrowse(DataReader &reader,
                               [[maybe_unused]] EventList &events) {
    uint16_t browseType;

    browseType = reader.ReadU16();

    for (int blockIdx = 0; blockIdx < 3; blockIdx++) {
        uint32_t offerCount;

        offerCount = reader.ReadU32();

        while (offerCount--) {
            /* offerEndTime */
            reader.SkipU32();
            /* unknown */
            reader.SkipU16();

            switch (browseType) {
            case 0xFFFF: /* Own offers */
            case 0xFFFE: /* Own history */
                /* item id */
                reader.SkipU16();
            }

            /* offerSize */
            reader.SkipU16();
            /* offerPrice */
            reader.SkipU32();

            switch (browseType) {
            case 0xFFFF: /* Own offers */
                break;
            case 0xFFFE: /* Own history */
                /* Offer state*/
                reader.SkipU8();
                [[fallthrough]];
            default:
                reader.SkipString();
            }
        }
    }
}

void Parser::ParseDeathDialog(DataReader &reader,
                              [[maybe_unused]] EventList &events) {
    if (Version_.Protocol.ExtendedDeathDialog) {
        uint8_t dialogType = reader.ReadU8();

        if (Version_.Protocol.UnfairFightReduction) {
            if (dialogType == 0) {
                /* Reduction in percent? */
                reader.SkipU8();
            }
        }
    }
}

void Parser::ParsePlayerInventory(DataReader &reader,
                                  [[maybe_unused]] EventList &events) {
    auto count = reader.ReadU16();

    for (int i = 0; i < count; i++) {
        /* itemId */
        reader.SkipU16();
        /* itemData */
        reader.SkipU8();
        /* itemCount */
        reader.SkipU16();
    }
}

void Parser::ParseRuleViolation(DataReader &reader,
                                [[maybe_unused]] EventList &events) {
    reader.Skip(2);
}

void Parser::ParseMoveEast(DataReader &reader, EventList &events) {
    Position_.X++;
    ParseAssert(Position_.X <
                (std::numeric_limits<uint16_t>::max() - Map::TileBufferWidth));

    auto &event = AddEvent<PlayerMoved>(events);
    event.Position = Position_;

    ParseMapDescription(reader, events, +9, -6, 1, Map::TileBufferHeight);
}

void Parser::ParseMoveNorth(DataReader &reader, EventList &events) {
    Position_.Y--;
    ParseAssert(Position_.X > Map::TileBufferHeight);

    auto &event = AddEvent<PlayerMoved>(events);
    event.Position = Position_;

    ParseMapDescription(reader, events, -8, -6, Map::TileBufferWidth, 1);
}

void Parser::ParseMoveSouth(DataReader &reader, EventList &events) {
    Position_.Y++;
    ParseAssert(Position_.Y <
                (std::numeric_limits<uint16_t>::max() - Map::TileBufferHeight));

    auto &event = AddEvent<PlayerMoved>(events);
    event.Position = Position_;

    ParseMapDescription(reader, events, -8, +7, Map::TileBufferWidth, 1);
}

void Parser::ParseMoveWest(DataReader &reader, EventList &events) {
    /* Move west */
    Position_.X--;
    ParseAssert(Position_.X > Map::TileBufferWidth);

    auto &event = AddEvent<PlayerMoved>(events);
    event.Position = Position_;

    ParseMapDescription(reader, events, -8, -6, 1, Map::TileBufferHeight);
}

Parser::EventList Parser::Parse(DataReader &reader) {
    Parser::EventList events;
    Parser::Repair repair;

    /* FIXME: Store significant branches (e.g item stackable or not) in
     * `Repair`, with their associated reader position. Upon errors, revert
     * back to the start of the packet and progressively flip the direction of
     * the branches until we've successfully parsed the entire payload.
     *
     * Needless to say, this is combinatorial. */
    while (reader.Remaining() > 0) {
        ParseNext(reader, repair, events);
    }

    return events;
}

void Parser::ParseNext(DataReader &reader,
                       [[maybe_unused]] Parser::Repair &repair,
                       Parser::EventList &events) {
    switch (reader.ReadU8()) {
    case 0x0A:
        /* HACK: This got re-used as a ping packet in 9.72, perhaps we should
         * translate packet types to canonical constants as well? */
        if (!Version_.AtLeast(9, 72)) {
            ParseInitialization(reader, events);
        }
        break;
    case 0x0B:
        ParseGMActions(reader, events);
        break;
    case 0x0F:
        break;
    case 0x17:
        ParseAssert(Version_.AtLeast(9, 72));
        ParseInitialization(reader, events);
        break;
    case 0x1D:
        break;
    case 0x1E:
        /* Single-byte ping packets, may overlap with patching in which case
         * we'll crash. Versioning will probably straighten that out but it's
         * difficult to map that out. */
        break;
    case 0x28:
        ParseDeathDialog(reader, events);
        break;
    case 0x64:
        ParseFullMapDescription(reader, events);
        break;
    case 0x65:
        ParseMoveNorth(reader, events);
        break;
    case 0x66:
        ParseMoveEast(reader, events);
        break;
    case 0x67:
        ParseMoveSouth(reader, events);
        break;
    case 0x68:
        ParseMoveWest(reader, events);
        break;
    case 0x69:
        ParseTileUpdate(reader, events);
        break;
    case 0x6A:
        ParseTileAddObject(reader, events);
        break;
    case 0x6B:
        ParseTileSetObject(reader, events);
        break;
    case 0x6C:
        ParseTileRemoveObject(reader, events);
        break;
    case 0x6D:
        ParseTileMoveCreature(reader, events);
        break;
    case 0x6E:
        ParseContainerOpen(reader, events);
        break;
    case 0x6F:
        ParseContainerClose(reader, events);
        break;
    case 0x70:
        ParseContainerAddItem(reader, events);
        break;
    case 0x71:
        ParseContainerTransformItem(reader, events);
        break;
    case 0x72:
        ParseContainerRemoveItem(reader, events);
        break;
    case 0x78:
        ParseInventorySetSlot(reader, events);
        break;
    case 0x79:
        ParseInventoryClearSlot(reader, events);
        break;
    case 0x7A:
        ParseNPCVendorBegin(reader, events);
        break;
    case 0x7B:
        ParseNPCVendorPlayerGoods(reader, events);
        break;
    case 0x7C:
        /* Single-byte NPC vendor abort */
        break;
    case 0x7D:
    case 0x7E:
        ParsePlayerTradeItems(reader, events);
        break;
    case 0x7F:
        /* Single-byte player trade abort */
        break;
    case 0x82:
        ParseAmbientLight(reader, events);
        break;
    case 0x83:
        ParseGraphicalEffect(reader, events);
        break;
    case 0x84:
        ParseTextEffect(reader, events);
        break;
    case 0x85:
        ParseMissileEffect(reader, events);
        break;
    case 0x86:
        ParseMarkCreature(reader, events);
        break;
    case 0x87:
        ParseTrappers(reader, events);
        break;
    case 0x8C:
        ParseCreatureHealth(reader, events);
        break;
    case 0x8D:
        ParseCreatureLight(reader, events);
        break;
    case 0x8E:
        ParseCreatureOutfit(reader, events);
        break;
    case 0x8F:
        ParseCreatureSpeed(reader, events);
        break;
    case 0x90:
        ParseCreatureSkull(reader, events);
        break;
    case 0x91:
        ParseCreatureShield(reader, events);
        break;
    case 0x92:
        ParseCreatureImpassable(reader, events);
        break;
    case 0x93:
        ParseCreaturePvPHelpers(reader, events);
        break;
    case 0x94:
        ParseCreatureGuildMembersOnline(reader, events);
        break;
    case 0x95:
        ParseCreatureType(reader, events);
        break;
    case 0x96:
        ParseOpenEditText(reader, events);
        break;
    case 0x97:
        ParseOpenHouseWindow(reader, events);
        break;
    case 0x9C:
        ParseBlessings(reader, events);
        break;
    case 0x9D:
        /* FIXME: This is also ParseOpenEditList, version this! */
        ParseHotkeyPresets(reader, events);
        break;
    case 0x9E:
        ParsePremiumTrigger(reader, events);
        break;
    case 0x9F:
        ParsePlayerDataBasic(reader, events);
        break;
    case 0xA0:
        ParsePlayerDataCurrent(reader, events);
        break;
    case 0xA1:
        ParsePlayerSkills(reader, events);
        break;
    case 0xA2:
        ParsePlayerIcons(reader, events);
        break;
    case 0xA3:
        ParseCancelAttack(reader, events);
        break;
    case 0xA4:
    case 0xA5:
        ParseSpellCooldown(reader, events);
        break;
    case 0xA6:
        ParseUseCooldown(reader, events);
        break;
    case 0xA7:
        ParsePlayerTactics(reader, events);
        break;
    case 0xAA:
        ParseCreatureSpeak(reader, events);
        break;
    case 0xAB:
        ParseChannelList(reader, events);
        break;
    case 0xAC:
        /* Public channel */
        ParseChannelOpen(reader, events);
        break;
    case 0xAD:
        ParseOpenPrivateConversation(reader, events);
        break;
    case 0xAE:
        break;
    case 0xAF:
        break;
    case 0xB0:
        /* Rule-violation-related packet with two-byte payload. */
        ParseRuleViolation(reader, events);
        break;
    case 0xB1:
        /* Single-byte rule-violation-related packet. */
        break;
    case 0xB2:
        /* Private channel, identical to 0xAC */
        ParseChannelOpen(reader, events);
        break;
    case 0xB3:
        ParseChannelClose(reader, events);
        break;
    case 0xB4:
        ParseTextMessage(reader, events);
        break;
    case 0xB5:
        ParseMoveDenied(reader, events);
        break;
    case 0xB6:
        ParseMoveDelay(reader, events);
        break;
    case 0xB7:
        ParseUnjustifiedPoints(reader, events);
        break;
    case 0xB8:
        ParseOpenPvPSituations(reader, events);
        break;
    case 0xBE:
        ParseFloorChangeUp(reader, events);
        break;
    case 0xBF:
        ParseFloorChangeDown(reader, events);
        break;
    case 0xC8:
        ParseOutfitDialog(reader, events);
        break;
    case 0xD2:
        ParseVIPStatus(reader, events);
        break;
    case 0xD3:
        ParseVIPOnline(reader, events);
        break;
    case 0xD4:
        ParseVIPOffline(reader, events);
        break;
    case 0xDC:
        ParseTutorialShow(reader, events);
        break;
    case 0xDD:
        ParseMinimapFlag(reader, events);
        break;
    case 0xF0:
        ParseQuestDialog(reader, events);
        break;
    case 0xF1:
        ParseQuestDialogMission(reader, events);
        break;
    case 0xF2:
        ParseOffenseReportResponse(reader, events);
        break;
    case 0xF3:
        ParseChannelEvent(reader, events);
        break;
    case 0xF5:
        ParsePlayerInventory(reader, events);
        break;
    case 0xF6:
        ParseMarketInitialization(reader, events);
        break;
    case 0xF7:
        /* empty packet! */
        break;
    case 0xF8:
        ParseMarketItemDetails(reader, events);
        break;
    case 0xF9:
        ParseMarketBrowse(reader, events);
        break;
    default:
        throw InvalidDataError();
    }
}

} // namespace trc