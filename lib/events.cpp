/*
 * Copyright 2024-2025 "John Högberg"
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

#include "events.hpp"

#include "gamestate.hpp"
#include "versions.hpp"

namespace trc {
namespace Events {

void WorldInitialized::Update(Gamestate &gamestate) {
    /* Clear out all containers, messages, and so on in case we've
     * relogged. */
    gamestate.Reset();

    gamestate.Player.Id = PlayerId;
    gamestate.Player.BeatDuration = BeatDuration;

    gamestate.Player.AllowBugReports = AllowBugReports;

    gamestate.SpeedA = SpeedA;
    gamestate.SpeedB = SpeedB;
    gamestate.SpeedC = SpeedC;
}

void AmbientLightChanged::Update(Gamestate &gamestate) {
    gamestate.Map.LightIntensity = Intensity;
    gamestate.Map.LightColor = Color;
}

void PlayerMoved::Update(Gamestate &gamestate) {
    gamestate.Map.Position = Position;
}

void TileUpdated::Update(Gamestate &gamestate) {
    auto &tile = gamestate.Map.Tile(Position);

    tile.Clear();

    tile.ObjectCount = std::min<size_t>(Objects.size(), Tile::MaxObjects);

    for (int i = 0; i < tile.ObjectCount; i++) {
        tile.Objects[i] = Objects[i];
    }
}

void TileObjectAdded::Update(Gamestate &gamestate) {
    auto &tile = gamestate.Map.Tile(TilePosition);

    tile.InsertObject(gamestate.Version, Object, StackPosition);
}

void TileObjectTransformed::Update(Gamestate &gamestate) {
    auto &tile = gamestate.Map.Tile(TilePosition);

    tile.SetObject(gamestate.Version, Object, StackPosition);
}

void TileObjectRemoved::Update(Gamestate &gamestate) {
    auto &tile = gamestate.Map.Tile(TilePosition);

    tile.RemoveObject(gamestate.Version, StackPosition);
}

void CreatureMoved::Update(Gamestate &gamestate) {
    const Version &version = gamestate.Version;

    uint32_t creatureId;

    if (StackPosition != Tile::StackPositionTop) {
        auto &fromTile = gamestate.Map.Tile(From);
        auto &movedObject = fromTile.GetObject(version, StackPosition);

        if (!movedObject.IsCreature()) {
            throw InvalidDataError();
        }

        creatureId = movedObject.CreatureId;

        fromTile.RemoveObject(version, StackPosition);
    } else {
        creatureId = CreatureId;
    }

    int xDifference, yDifference, zDifference;

    auto &toTile = gamestate.Map.Tile(To);
    auto &creature = gamestate.GetCreature(creatureId);

    creature.MovementInformation.Origin = From;
    creature.MovementInformation.Target = To;

    xDifference = creature.MovementInformation.Target.X -
                  creature.MovementInformation.Origin.X;
    yDifference = creature.MovementInformation.Target.Y -
                  creature.MovementInformation.Origin.Y;
    zDifference = creature.MovementInformation.Target.Z -
                  creature.MovementInformation.Origin.Z;

    if (yDifference < 0) {
        creature.Heading = Creature::Direction::North;
    } else if (yDifference > 0) {
        creature.Heading = Creature::Direction::South;
    }

    if (xDifference < 0) {
        creature.Heading = Creature::Direction::West;
    } else if (xDifference > 0) {
        creature.Heading = Creature::Direction::East;
    }

    if (zDifference == 0 &&
        (std::abs(xDifference) <= 1 && std::abs(yDifference) <= 1)) {
        auto &groundObject = toTile.GetObject(gamestate.Version, 0);
        uint32_t movementSpeed;

        const auto &groundType = version.GetItem(groundObject.Id);
        if (groundType.Properties.StackPriority != 0) {
            throw InvalidDataError();
        }

        if (version.Protocol.SpeedAdjustment) {
            if ((double)creature.Speed >= -gamestate.SpeedB) {
                movementSpeed = std::max<uint32_t>(
                        1,
                        (uint32_t)(gamestate.SpeedA *
                                           log((double)creature.Speed +
                                               gamestate.SpeedB) +
                                   gamestate.SpeedC));
            } else {
                movementSpeed = 1;
            }
        } else {
            movementSpeed = std::max<int>(1, creature.Speed);
        }

        creature.MovementInformation.WalkEndTick =
                gamestate.CurrentTick +
                (groundType.Properties.Speed * 1000) / movementSpeed;
        creature.MovementInformation.WalkStartTick = gamestate.CurrentTick;
    } else {
        /* Moves between floors, as well as teleportations, are instant. */
        creature.MovementInformation.WalkStartTick = 0;
        creature.MovementInformation.WalkEndTick = 0;
    }

    Object movedObject;

    movedObject.Id = Object::CreatureMarker;
    movedObject.CreatureId = creatureId;

    toTile.InsertObject(gamestate.Version, movedObject, Tile::StackPositionTop);
}

void CreatureRemoved::Update(Gamestate &gamestate) {
    gamestate.Creatures.erase(CreatureId);
}

void CreatureSeen::Update(Gamestate &gamestate) {
    /* It's okay for this to point at the old one, in which case this is
     * just a really big property update. */
    [[maybe_unused]] auto [it, added] =
            gamestate.Creatures.try_emplace(CreatureId);
    auto &creature = it->second;

    /* FIXME: C++ migration, zero-init. */
    creature.MovementInformation = {};

    creature.Id = CreatureId;
    creature.Type = Type;
    creature.Name = Name;
    creature.Health = std::max<uint8_t>(0, std::min<uint8_t>(Health, 100));
    creature.Heading = Heading;
    creature.Outfit = Outfit;
    creature.LightIntensity = LightIntensity;
    creature.LightColor = LightColor;
    creature.Speed = Speed;

    creature.Skull = Skull;
    creature.Shield = Shield;
    creature.War = War;
    creature.NPCCategory = NPCCategory;

    creature.Mark = Mark;
    creature.MarkIsPermanent = MarkIsPermanent;
    creature.GuildMembersOnline = GuildMembersOnline;
}

void CreatureHealthUpdated::Update(Gamestate &gamestate) {
    auto &creature = gamestate.GetCreature(CreatureId);

    creature.Health = std::max<uint8_t>(0, std::min<uint8_t>(Health, 100));
}

void CreatureHeadingUpdated::Update(Gamestate &gamestate) {
    auto &creature = gamestate.GetCreature(CreatureId);

    creature.Heading = Heading;
}

void CreatureLightUpdated::Update(Gamestate &gamestate) {
    auto &creature = gamestate.GetCreature(CreatureId);

    creature.LightIntensity = Intensity;
    creature.LightColor = Color;
}

void CreatureOutfitUpdated::Update(Gamestate &gamestate) {
    auto &creature = gamestate.GetCreature(CreatureId);

    creature.Outfit = Outfit;
}

void CreatureSpeedUpdated::Update(Gamestate &gamestate) {
    auto &creature = gamestate.GetCreature(CreatureId);

    creature.Speed = Speed;
}

void CreatureSkullUpdated::Update(Gamestate &gamestate) {
    auto &creature = gamestate.GetCreature(CreatureId);

    creature.Skull = Skull;
}

void CreatureShieldUpdated::Update(Gamestate &gamestate) {
    auto &creature = gamestate.GetCreature(CreatureId);

    creature.Shield = Shield;
}

void CreatureImpassableUpdated::Update(Gamestate &gamestate) {
    auto &creature = gamestate.GetCreature(CreatureId);

    creature.Impassable = Impassable;
}

void CreaturePvPHelpersUpdated::Update(Gamestate &gamestate) {
    auto &creature = gamestate.GetCreature(CreatureId);

    creature.MarkIsPermanent = MarkIsPermanent;
    creature.Mark = Mark;
}

void CreatureGuildMembersUpdated::Update(Gamestate &gamestate) {
    auto &creature = gamestate.GetCreature(CreatureId);

    creature.GuildMembersOnline = GuildMembersOnline;
}

void CreatureTypeUpdated::Update(Gamestate &gamestate) {
    auto &creature = gamestate.GetCreature(CreatureId);

    creature.Type = Type;
}

void CreatureNPCCategoryUpdated::Update(Gamestate &gamestate) {
    auto &creature = gamestate.GetCreature(CreatureId);

    creature.NPCCategory = Category;
}

void PlayerInventoryUpdated::Update(Gamestate &gamestate) {
    gamestate.Player.Inventory(Slot) = Item;
}

void PlayerBlessingsUpdated::Update(Gamestate &gamestate) {
    gamestate.Player.Blessings = Blessings;
}

void PlayerHotkeyPresetUpdated::Update(Gamestate &gamestate) {
    gamestate.Player.HotkeyPreset = HotkeyPreset;
}

void PlayerDataBasicUpdated::Update(Gamestate &gamestate) {
    gamestate.Player.IsPremium = IsPremium;
    gamestate.Player.PremiumUntil = PremiumUntil;
    gamestate.Player.Vocation = Vocation;
}

void PlayerDataUpdated::Update(Gamestate &gamestate) {
    auto &stats = gamestate.Player.Stats;

    stats.Capacity = Capacity;
    stats.Experience = Experience;
    stats.ExperienceBonus = ExperienceBonus;
    stats.Fed = Fed;
    stats.Health = std::max<int16_t>(0, std::min(Health, MaxHealth));
    stats.Level = Level;
    stats.MagicLevel = MagicLevel;
    stats.MagicLevelBase = MagicLevelBase;
    stats.MagicLevelPercent = MagicLevelPercent;
    stats.Mana = Mana;
    stats.MaxCapacity = MaxCapacity;
    stats.MaxHealth = MaxHealth;
    stats.MaxMana = MaxMana;
    stats.OfflineStamina = OfflineStamina;
    stats.SoulPoints = SoulPoints;
    stats.Speed = Speed;
    stats.Stamina = Stamina;
}

void PlayerSkillsUpdated::Update(Gamestate &gamestate) {
    auto &player = gamestate.Player;

    for (int i = 0; i < PLAYER_SKILL_COUNT; i++) {
        player.Skills[i].Effective = Skills[i].Effective;
        player.Skills[i].Actual = Skills[i].Actual;
        player.Skills[i].Percent = Skills[i].Percent;
    }
}

void PlayerIconsUpdated::Update(Gamestate &gamestate) {
    gamestate.Player.Icons = Icons;
}

void PlayerTacticsUpdated::Update(Gamestate &gamestate) {
    gamestate.Player.AttackMode = AttackMode;
    gamestate.Player.ChaseMode = ChaseMode;
    gamestate.Player.SecureMode = SecureMode;
    gamestate.Player.PvPMode = PvPMode;
}

void PvPSituationsChanged::Update(Gamestate &gamestate) {
    gamestate.Player.OpenPvPSituations = OpenSituations;
}

void CreatureSpoke::Update(Gamestate &gamestate) {
    gamestate.AddTextMessage(Mode, Message, AuthorName);
}

void CreatureSpokeOnMap::Update(Gamestate &gamestate) {
    gamestate.AddTextMessage(Mode, Message, AuthorName, Position);
}

void CreatureSpokeInChannel::Update([[maybe_unused]] Gamestate &gamestate) {
}

void ChannelListUpdated::Update([[maybe_unused]] Gamestate &gamestate) {
}

void ChannelOpened::Update([[maybe_unused]] Gamestate &gamestate) {
}

void ChannelClosed::Update([[maybe_unused]] Gamestate &gamestate) {
}

void PrivateConversationOpened::Update([[maybe_unused]] Gamestate &gamestate) {
}

void ContainerOpened::Update(Gamestate &gamestate) {
    auto [it, added] = gamestate.Containers.try_emplace(ContainerId);

    auto &container = it->second;

    /* Reusing a container is not a protocol violation, however, we
     * to clear its items as this is effectively a new container. */
    if (!added) {
        container.Items.clear();
    }

    container.ItemId = ItemId;
    container.Mark = Mark;
    container.Animation = Animation;

    container.Name = Name;
    container.SlotsPerPage = SlotsPerPage;
    container.HasParent = HasParent;
    container.DragAndDrop = DragAndDrop;
    container.Pagination = Pagination;
    container.TotalObjects = TotalObjects;
    container.StartIndex = StartIndex;
    container.Items = Items;
}

void ContainerClosed::Update(Gamestate &gamestate) {
    /* It's fine to close a non-existing container. */
    (void)gamestate.Containers.erase(ContainerId);
}

void ContainerAddedItem::Update(Gamestate &gamestate) {
    auto it = gamestate.Containers.find(ContainerId);

    if (it != gamestate.Containers.end()) {
        auto &container = it->second;

        if (ContainerIndex >= container.StartIndex) {
            auto insertAt = (ContainerIndex - container.StartIndex);

            container.Items.insert(container.Items.begin() + insertAt, Item);
        }

        container.TotalObjects += 1;
    }
}

void ContainerTransformedItem::Update(Gamestate &gamestate) {
    auto it = gamestate.Containers.find(ContainerId);

    if (it != gamestate.Containers.end()) {
        auto &container = it->second;

        if (ContainerIndex >= container.StartIndex) {
            auto transformAt = ContainerIndex - container.StartIndex;

            if (transformAt < container.Items.size()) {
                container.Items[transformAt] = Item;
            }
        }
    }
}

void ContainerRemovedItem::Update(Gamestate &gamestate) {
    auto it = gamestate.Containers.find(ContainerId);

    if (it != gamestate.Containers.end()) {
        auto &container = it->second;

        if (ContainerIndex >= container.StartIndex) {
            auto removeAt = ContainerIndex - container.StartIndex;

            if (removeAt < container.Items.size()) {
                container.Items.erase(container.Items.begin() + removeAt);

                /* Backfill id is only != 0 when the container is full. */
                if (Backfill.Id != 0) {
                    container.Items.push_back(Backfill);
                }
            }
        }

        container.TotalObjects--;
    }
}

void NumberEffectPopped::Update(Gamestate &gamestate) {
    auto &tile = gamestate.Map.Tile(Position);

    tile.AddNumericalEffect(Color, Value, gamestate.CurrentTick);
}

void GraphicalEffectPopped::Update(Gamestate &gamestate) {
    auto &tile = gamestate.Map.Tile(Position);

    tile.AddGraphicalEffect(Id, gamestate.CurrentTick);
}

void MissileFired::Update(Gamestate &gamestate) {
    gamestate.AddMissileEffect(Origin, Target, Id);
}

void StatusMessageReceived::Update(Gamestate &gamestate) {
    switch (Mode) {
    case MessageMode::DamageDealt:
    case MessageMode::DamageReceived:
    case MessageMode::DamageReceivedOthers:
    case MessageMode::Healing:
    case MessageMode::HealingOthers:
    case MessageMode::Experience:
    case MessageMode::ExperienceOthers:
    case MessageMode::Mana:
        /* These are not shown in the viewport. */
        break;
    default:
        gamestate.AddTextMessage(Mode, Message);
        break;
    }
}

void StatusMessageReceivedInChannel::Update(
        [[maybe_unused]] Gamestate &gamestate) {
}

} // namespace Events
} // namespace trc
