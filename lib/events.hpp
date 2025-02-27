/*
 * Copyright 2024 "John Högberg"
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

#ifndef __TRC_EVENTS_HPP__
#define __TRC_EVENTS_HPP__

#include "gamestate.hpp"

#include <vector>

namespace trc {

/* Forward-declare gamestate? */
struct Gamestate;

namespace Events {

enum class Type {
    WorldInitialized,
    AmbientLightChanged,
    TileUpdated,
    TileObjectAdded,
    TileObjectTransformed,
    TileObjectRemoved,
    CreatureMoved,
    CreatureRemoved,
    CreatureSeen,
    CreatureHealthUpdated,
    CreatureHeadingUpdated,
    CreatureLightUpdated,
    CreatureOutfitUpdated,
    CreatureSpeedUpdated,
    CreatureSkullUpdated,
    CreatureShieldUpdated,
    CreatureImpassableUpdated,
    CreaturePvPHelpersUpdated,
    CreatureGuildMembersUpdated,
    CreatureTypeUpdated,
    CreatureNPCCategoryUpdated,
    PlayerMoved,
    PlayerInventoryUpdated,
    PlayerBlessingsUpdated,
    PlayerHotkeyPresetUpdated,
    PlayerDataBasicUpdated,
    PlayerDataUpdated,
    PlayerSkillsUpdated,
    PlayerIconsUpdated,
    PlayerTacticsUpdated,
    PvPSituationsChanged,
    CreatureSpoke,
    CreatureSpokeOnMap,
    CreatureSpokeInChannel,
    ChannelListUpdated,
    ChannelOpened,
    ChannelClosed,
    PrivateConversationOpened,
    ContainerOpened,
    ContainerClosed,
    ContainerAddedItem,
    ContainerTransformedItem,
    ContainerRemovedItem,
    NumberEffectPopped,
    GraphicalEffectPopped,
    MissileFired,
    StatusMessageReceived,
    StatusMessageReceivedInChannel
};

struct Base {
    virtual void Update(trc::Gamestate &gamestate) = 0;
    virtual Events::Type Kind() const = 0;

    virtual ~Base() = default;
};

struct WorldInitialized : public Base {
    uint32_t PlayerId;
    uint16_t BeatDuration;

    double SpeedA = 0.0, SpeedB = 0.0, SpeedC = 0.0;
    bool AllowBugReports = false;

    uint8_t PvPFraming;
    bool ExpertMode;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::WorldInitialized;
    }
};

struct AmbientLightChanged : public Base {
    uint8_t Intensity;
    uint8_t Color;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::AmbientLightChanged;
    }
};

struct TileUpdated : public Base {
    trc::Position Position;

    std::vector<Object> Objects;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::TileUpdated;
    }
};

struct TileObjectAdded : public Base {
    Position TilePosition;
    uint8_t StackPosition;

    trc::Object Object;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::TileObjectAdded;
    }
};

struct TileObjectTransformed : public Base {
    Position TilePosition;
    uint8_t StackPosition;

    trc::Object Object;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::TileObjectTransformed;
    }
};

struct TileObjectRemoved : public Base {
    Position TilePosition;
    uint8_t StackPosition;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::TileObjectRemoved;
    }
};

struct CreatureMoved : public Base {
    Position From;
    Position To;

    uint8_t StackPosition;
    uint32_t CreatureId;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::CreatureMoved;
    }
};

struct CreatureRemoved : public Base {
    uint32_t CreatureId;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::CreatureRemoved;
    }
};

struct CreatureSeen : public Base {
    uint32_t CreatureId;

    CreatureType Type;
    std::string Name;
    uint8_t Health;
    Creature::Direction Heading;
    Appearance Outfit;
    uint8_t LightIntensity;
    uint8_t LightColor;
    uint16_t Speed;

    CharacterSkull Skull = CharacterSkull::None;
    PartyShield Shield = PartyShield::None;
    WarIcon War = WarIcon::None;
    trc::NPCCategory NPCCategory = NPCCategory::None;

    uint8_t Mark = 0;
    bool MarkIsPermanent = true;
    uint16_t GuildMembersOnline = 0;

    bool Impassable = true;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::CreatureSeen;
    }
};

struct CreatureHealthUpdated : public Base {
    uint32_t CreatureId;
    uint8_t Health;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::CreatureHealthUpdated;
    }
};

struct CreatureHeadingUpdated : public Base {
    uint32_t CreatureId;
    Creature::Direction Heading;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::CreatureHeadingUpdated;
    }
};

struct CreatureLightUpdated : public Base {
    uint32_t CreatureId;
    uint8_t Intensity;
    uint8_t Color;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::CreatureLightUpdated;
    }
};

struct CreatureOutfitUpdated : public Base {
    uint32_t CreatureId;

    trc::Appearance Outfit;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::CreatureOutfitUpdated;
    }
};

struct CreatureSpeedUpdated : public Base {
    uint32_t CreatureId;

    uint16_t Speed;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::CreatureSpeedUpdated;
    }
};

struct CreatureSkullUpdated : public Base {
    uint32_t CreatureId;

    CharacterSkull Skull;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::CreatureSkullUpdated;
    }
};

struct CreatureShieldUpdated : public Base {
    uint32_t CreatureId;

    PartyShield Shield;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::CreatureShieldUpdated;
    }
};

struct CreatureImpassableUpdated : public Base {
    uint32_t CreatureId;

    bool Impassable;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::CreatureImpassableUpdated;
    }
};

struct CreaturePvPHelpersUpdated : public Base {
    uint32_t CreatureId;

    bool MarkIsPermanent;
    uint8_t Mark;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::CreaturePvPHelpersUpdated;
    }
};

struct CreatureGuildMembersUpdated : public Base {
    uint32_t CreatureId;

    uint16_t GuildMembersOnline;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::CreatureGuildMembersUpdated;
    }
};

struct CreatureTypeUpdated : public Base {
    uint32_t CreatureId;

    CreatureType Type;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::CreatureTypeUpdated;
    }
};

struct CreatureNPCCategoryUpdated : public Base {
    uint32_t CreatureId;

    NPCCategory Category;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::CreatureNPCCategoryUpdated;
    }
};

struct PlayerMoved : public Base {
    trc::Position Position;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::PlayerMoved;
    }
};

struct PlayerInventoryUpdated : public Base {
    InventorySlot Slot;
    Object Item;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::PlayerInventoryUpdated;
    }
};

struct PlayerBlessingsUpdated : public Base {
    uint16_t Blessings;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::PlayerBlessingsUpdated;
    }
};

struct PlayerHotkeyPresetUpdated : public Base {
    uint32_t CreatureId;
    uint32_t HotkeyPreset;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::PlayerHotkeyPresetUpdated;
    }
};

struct PlayerDataBasicUpdated : public Base {
    bool IsPremium;

    uint32_t PremiumUntil = 0;
    uint8_t Vocation;
    std::vector<uint16_t> Spells;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::PlayerDataBasicUpdated;
    }
};

struct PlayerDataUpdated : public Base {
    double ExperienceBonus = 1.0;
    int16_t Health;
    int16_t Mana;
    int16_t MaxHealth;
    int16_t MaxMana;
    uint16_t Fed = 0;
    uint16_t Level;
    uint16_t OfflineStamina = 0;
    uint16_t Speed = 0;
    uint16_t Stamina = 0;
    uint32_t Capacity;
    uint32_t MaxCapacity = std::numeric_limits<uint32_t>::max();
    uint64_t Experience;
    uint8_t LevelPercent = 0;
    uint8_t MagicLevel;
    uint8_t MagicLevelBase;
    uint8_t MagicLevelPercent = 0;
    uint8_t SoulPoints = 0;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::PlayerDataUpdated;
    }
};

struct PlayerSkillsUpdated : public Base {
    struct {
        uint16_t Effective;
        uint16_t Actual;
        uint8_t Percent;
    } Skills[PLAYER_SKILL_COUNT];

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::PlayerSkillsUpdated;
    }
};

struct PlayerIconsUpdated : public Base {
    StatusIcon Icons;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::PlayerIconsUpdated;
    }
};

struct PlayerTacticsUpdated : public Base {
    bool AttackMode = false;
    bool ChaseMode = false;
    bool SecureMode = false;
    bool PvPMode = false;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::PlayerTacticsUpdated;
    }
};

struct PvPSituationsChanged : public Base {
    uint8_t OpenSituations;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::PvPSituationsChanged;
    }
};

struct CreatureSpoke : public Base {
    uint32_t MessageId;
    MessageMode Mode;

    std::string AuthorName;
    uint16_t AuthorLevel;

    std::string Message;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::CreatureSpoke;
    }
};

struct CreatureSpokeOnMap : public CreatureSpoke {
    trc::Position Position;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::CreatureSpokeOnMap;
    }
};

struct CreatureSpokeInChannel : public CreatureSpoke {
    uint16_t ChannelId;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::CreatureSpokeInChannel;
    }
};

struct ChannelListUpdated : public Base {
    std::vector<std::pair<uint16_t, std::string>> Channels;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::ChannelListUpdated;
    }
};

struct ChannelOpened : public Base {
    uint16_t Id;
    std::string Name;

    std::vector<std::string> Participants;
    std::vector<std::string> Invitees;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::ChannelOpened;
    }
};

struct ChannelClosed : public Base {
    uint16_t Id;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::ChannelClosed;
    }
};

struct PrivateConversationOpened : public Base {
    std::string Name;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::PrivateConversationOpened;
    }
};

struct ContainerOpened : public Base {
    uint32_t ContainerId;

    uint16_t ItemId;
    uint8_t Mark = 255;
    uint8_t Animation = 0;

    std::string Name;
    uint8_t SlotsPerPage;
    uint8_t HasParent;

    uint8_t DragAndDrop = 0;
    uint8_t Pagination = 0;
    uint16_t TotalObjects;
    uint16_t StartIndex = 0;

    std::vector<Object> Items;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::ContainerOpened;
    }
};

struct ContainerClosed : public Base {
    uint32_t ContainerId;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::ContainerClosed;
    }
};

struct ContainerAddedItem : public Base {
    uint32_t ContainerId;

    uint32_t ContainerIndex;

    Object Item;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::ContainerAddedItem;
    }
};

struct ContainerTransformedItem : public Base {
    uint32_t ContainerId;

    uint32_t ContainerIndex;

    Object Item;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::ContainerTransformedItem;
    }
};

struct ContainerRemovedItem : public Base {
    uint32_t ContainerId;

    uint32_t ContainerIndex;

    Object Backfill;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::ContainerRemovedItem;
    }
};

struct NumberEffectPopped : public Base {
    trc::Position Position;
    uint8_t Color;
    uint32_t Value;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::NumberEffectPopped;
    }
};

struct GraphicalEffectPopped : public Base {
    trc::Position Position;
    uint8_t Id;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::GraphicalEffectPopped;
    }
};

struct MissileFired : public Base {
    Position Origin;
    Position Target;
    uint8_t Id;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::MissileFired;
    }
};

struct StatusMessageReceived : public Base {
    MessageMode Mode;

    std::string Message;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::StatusMessageReceived;
    }
};

struct StatusMessageReceivedInChannel : public StatusMessageReceived {
    uint16_t ChannelId;

    virtual void Update(Gamestate &gamestate);
    virtual Events::Type Kind() const {
        return Events::Type::StatusMessageReceivedInChannel;
    }
};

} // namespace Events
} // namespace trc

#endif
