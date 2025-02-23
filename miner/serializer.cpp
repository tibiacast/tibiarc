/*
 * Copyright 2025 "John Högberg"
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

#include "serializer.hpp"

#include "characterset.hpp"
#include "memoryfile.hpp"
#include "versions.hpp"

#include "deps/json.hpp"

#include <algorithm>
#include <iterator>

#ifdef _WIN32
#    define DIR_SEP "\\"
#else
#    define DIR_SEP "/"
#endif

using json = nlohmann::json;

namespace trc {
NLOHMANN_JSON_SERIALIZE_ENUM(
        MessageMode,
        {{MessageMode::PrivateIn, "PrivateIn"},
         {MessageMode::PrivateOut, "PrivateOut"},
         {MessageMode::Say, "Say"},
         {MessageMode::Whisper, "Whisper"},
         {MessageMode::Yell, "Yell"},
         {MessageMode::ChannelWhite, "ChannelWhite"},
         {MessageMode::ChannelYellow, "ChannelYellow"},
         {MessageMode::ChannelOrange, "ChannelOrange"},
         {MessageMode::ChannelRed, "ChannelRed"},
         {MessageMode::ChannelAnonymousRed, "ChannelAnonymousRed"},
         {MessageMode::ConsoleBlue, "ConsoleBlue"},
         {MessageMode::ConsoleOrange, "ConsoleOrange"},
         {MessageMode::ConsoleRed, "ConsoleRed"},
         {MessageMode::Spell, "Spell"},
         {MessageMode::NPCStart, "NPCStart"},
         {MessageMode::NPCContinued, "NPCContinued"},
         {MessageMode::PlayerToNPC, "PlayerToNPC"},
         {MessageMode::Broadcast, "Broadcast"},
         {MessageMode::GMToPlayer, "GMToPlayer"},
         {MessageMode::PlayerToGM, "PlayerToGM"},
         {MessageMode::Login, "Login"},
         {MessageMode::Admin, "Admin"},
         {MessageMode::Game, "Game"},
         {MessageMode::Failure, "Failure"},
         {MessageMode::Look, "Look"},
         {MessageMode::DamageDealt, "DamageDealt"},
         {MessageMode::DamageReceived, "DamageReceived"},
         {MessageMode::Healing, "Healing"},
         {MessageMode::Experience, "Experience"},
         {MessageMode::DamageReceivedOthers, "DamageReceivedOthers"},
         {MessageMode::HealingOthers, "HealingOthers"},
         {MessageMode::ExperienceOthers, "ExperienceOthers"},
         {MessageMode::Status, "Status"},
         {MessageMode::Loot, "Loot"},
         {MessageMode::NPCTrade, "NPCTrade"},
         {MessageMode::Guild, "Guild"},
         {MessageMode::PartyWhite, "PartyWhite"},
         {MessageMode::Party, "Party"},
         {MessageMode::MonsterSay, "MonsterSay"},
         {MessageMode::MonsterYell, "MonsterYell"},
         {MessageMode::Report, "Report"},
         {MessageMode::Hotkey, "Hotkey"},
         {MessageMode::Tutorial, "Tutorial"},
         {MessageMode::ThankYou, "ThankYou"},
         {MessageMode::Market, "Market"},
         {MessageMode::Mana, "Mana"},
         {MessageMode::Warning, "Warning"},
         {MessageMode::RuleViolationChannel, "RuleViolationChannel"},
         {MessageMode::RuleViolationAnswer, "RuleViolationAnswer"},
         {MessageMode::RuleViolationContinue, "RuleViolationContinue"}});

NLOHMANN_JSON_SERIALIZE_ENUM(CharacterSkull,
                             {{CharacterSkull::None, "None"},
                              {CharacterSkull::Yellow, "Yellow"},
                              {CharacterSkull::Green, "Green"},
                              {CharacterSkull::White, "White"},
                              {CharacterSkull::Red, "Red"},
                              {CharacterSkull::Black, "Black"},
                              {CharacterSkull::Orange, "Orange"}});

NLOHMANN_JSON_SERIALIZE_ENUM(CreatureType,
                             {
                                     {CreatureType::Player, "Player"},
                                     {CreatureType::Monster, "Monster"},
                                     {CreatureType::NPC, "NPC"},
                                     {CreatureType::SummonOwn, "SummonOwn"},
                                     {CreatureType::SummonOthers,
                                      "SummonOthers"},
                             });

NLOHMANN_JSON_SERIALIZE_ENUM(NPCCategory,
                             {
                                     {NPCCategory::None, "None"},
                                     {NPCCategory::Normal, "Normal"},
                                     {NPCCategory::Trader, "Trader"},
                                     {NPCCategory::Quest, "Quest"},
                                     {NPCCategory::TraderQuest, "TraderQuest"},
                             });

NLOHMANN_JSON_SERIALIZE_ENUM(InventorySlot,
                             {
                                     {InventorySlot::Head, "Head"},
                                     {InventorySlot::Amulet, "Amulet"},
                                     {InventorySlot::Backpack, "Backpack"},
                                     {InventorySlot::Chest, "Chest"},
                                     {InventorySlot::RightArm, "RightArm"},
                                     {InventorySlot::LeftArm, "LeftArm"},
                                     {InventorySlot::Legs, "Legs"},
                                     {InventorySlot::Boots, "Boots"},
                                     {InventorySlot::Ring, "Ring"},
                                     {InventorySlot::Quiver, "Quiver"},
                                     {InventorySlot::Purse, "Purse"},
                             });

NLOHMANN_JSON_SERIALIZE_ENUM(
        PartyShield,
        {
                {PartyShield::None, "None"},
                {PartyShield::WhiteYellow, "WhiteYellow"},
                {PartyShield::WhiteBlue, "WhiteBlue"},
                {PartyShield::Blue, "Blue"},
                {PartyShield::Yellow, "Yellow"},
                {PartyShield::BlueSharedExp, "BlueSharedExp"},
                {PartyShield::YellowSharedExp, "YellowSharedExp"},
                {PartyShield::BlueNoSharedExpBlink, "BlueNoSharedExpBlink"},
                {PartyShield::YellowNoSharedExpBlink, "YellowNoSharedExpBlink"},
                {PartyShield::BlueNoSharedExp, "BlueNoSharedExp"},
                {PartyShield::YellowNoSharedExp, "YellowNoSharedExp"},
                {PartyShield::Gray, "Gray"},
        });

NLOHMANN_JSON_SERIALIZE_ENUM(WarIcon,
                             {
                                     {WarIcon::None, "None"},
                                     {WarIcon::Ally, "Ally"},
                                     {WarIcon::Enemy, "Enemy"},
                                     {WarIcon::Neutral, "Neutral"},
                                     {WarIcon::Member, "Member"},
                                     {WarIcon::Other, "Other"},
                             });

NLOHMANN_JSON_SERIALIZE_ENUM(Creature::Direction,
                             {
                                     {Creature::Direction::North, "North"},
                                     {Creature::Direction::East, "East"},
                                     {Creature::Direction::South, "South"},
                                     {Creature::Direction::West, "West"},
                             });

template <typename T>
static json ToJSON(const Version &version, const std::vector<T> &objects) {
    std::vector<json> result;

    std::transform(
            objects.begin(),
            objects.end(),
            std::back_insert_iterator(result),
            [&version](const T &object) { return ToJSON(version, object); });

    return json{result};
}

static json ToJSON(const Version &version, const Position &position) {
    return json{{"X", position.X}, {"Y", position.Y}, {"Z", position.Z}};
}

static json ToJSON(const Version &version, const Object &object) {
    json result;

    if (object.IsCreature()) {
        result = json{{"CreatureId", object.CreatureId}};

        if (version.Protocol.CreatureMarks) {
            result["Mark"] = object.Mark;
        }
    } else {
        result = json{{"ItemId", object.Id}};

        if (object.Id != 0) {
            const auto &type = version.GetItem(object.Id);

            if (version.Protocol.ItemMarks) {
                result["Mark"] = object.Mark;
            }

            if (type.Properties.LiquidContainer || type.Properties.LiquidPool) {
                result["Fluid"] = object.ExtraByte;
            } else if (type.Properties.Stackable ||
                       (type.Properties.Rune &&
                        version.Protocol.RuneChargeCount)) {
                result["Count"] = object.ExtraByte;
            }

            if (type.Properties.Animated && version.Protocol.ItemAnimation) {
                result["Animation"] = object.Animation;
            }
        }
    }

    return result;
}

static json ToJSON(const Version &version, const Appearance &appearance) {
    json result = json{{"Id", appearance.Id},
                       {"HeadColor", appearance.HeadColor},
                       {"PrimaryColor", appearance.PrimaryColor},
                       {"SecondaryColor", appearance.SecondaryColor},
                       {"DetailColor", appearance.DetailColor},
                       {"Item", ToJSON(version, appearance.Item)}};

    if (version.Protocol.OutfitAddons) {
        result["Addons"] = appearance.Addons;
    }

    if (version.Protocol.Mounts) {
        result["MountId"] = appearance.MountId;
    }

    return result;
}

namespace Events {
NLOHMANN_JSON_SERIALIZE_ENUM(
        Type,
        {{Type::WorldInitialized, "WorldInitialized"},
         {Type::AmbientLightChanged, "AmbientLightChanged"},
         {Type::TileUpdated, "TileUpdated"},
         {Type::TileObjectAdded, "TileObjectAdded"},
         {Type::TileObjectTransformed, "TileObjectTransformed"},
         {Type::TileObjectRemoved, "TileObjectRemoved"},
         {Type::CreatureMoved, "CreatureMoved"},
         {Type::CreatureRemoved, "CreatureRemoved"},
         {Type::CreatureSeen, "CreatureSeen"},
         {Type::CreatureHealthUpdated, "CreatureHealthUpdated"},
         {Type::CreatureHeadingUpdated, "CreatureHeadingUpdated"},
         {Type::CreatureLightUpdated, "CreatureLightUpdated"},
         {Type::CreatureOutfitUpdated, "CreatureOutfitUpdated"},
         {Type::CreatureSpeedUpdated, "CreatureSpeedUpdated"},
         {Type::CreatureSkullUpdated, "CreatureSkullUpdated"},
         {Type::CreatureShieldUpdated, "CreatureShieldUpdated"},
         {Type::CreatureImpassableUpdated, "CreatureImpassableUpdated"},
         {Type::CreaturePvPHelpersUpdated, "CreaturePvPHelpersUpdated"},
         {Type::CreatureGuildMembersUpdated, "CreatureGuildMembersUpdated"},
         {Type::CreatureTypeUpdated, "CreatureTypeUpdated"},
         {Type::CreatureNPCCategoryUpdated, "CreatureNPCCategoryUpdated"},
         {Type::PlayerMoved, "PlayerMoved"},
         {Type::PlayerInventoryUpdated, "PlayerInventoryUpdated"},
         {Type::PlayerBlessingsUpdated, "PlayerBlessingsUpdated"},
         {Type::PlayerHotkeyPresetUpdated, "PlayerHotkeyPresetUpdated"},
         {Type::PlayerDataBasicUpdated, "PlayerDataBasicUpdated"},
         {Type::PlayerDataUpdated, "PlayerDataUpdated"},
         {Type::PlayerSkillsUpdated, "PlayerSkillsUpdated"},
         {Type::PlayerIconsUpdated, "PlayerIconsUpdated"},
         {Type::PlayerTacticsUpdated, "PlayerTacticsUpdated"},
         {Type::PvPSituationsChanged, "PvPSituationsChanged"},
         {Type::CreatureSpoke, "CreatureSpoke"},
         {Type::CreatureSpokeOnMap, "CreatureSpokeOnMap"},
         {Type::CreatureSpokeInChannel, "CreatureSpokeInChannel"},
         {Type::ChannelListUpdated, "ChannelListUpdated"},
         {Type::ChannelOpened, "ChannelOpened"},
         {Type::ChannelClosed, "ChannelClosed"},
         {Type::PrivateConversationOpened, "PrivateConversationOpened"},
         {Type::ContainerOpened, "ContainerOpened"},
         {Type::ContainerClosed, "ContainerClosed"},
         {Type::ContainerAddedItem, "ContainerAddedItem"},
         {Type::ContainerTransformedItem, "ContainerTransformedItem"},
         {Type::ContainerRemovedItem, "ContainerRemovedItem"},
         {Type::NumberEffectPopped, "NumberEffectPopped"},
         {Type::GraphicalEffectPopped, "GraphicalEffectPopped"},
         {Type::MissileFired, "MissileFired"},
         {Type::StatusMessageReceived, "StatusMessageReceived"},
         {Type::StatusMessageReceivedInChannel,
          "StatusMessageReceivedInChannel"}});

static json ToJSON(const Version &version, const WorldInitialized &event) {
    return json{{"PlayerId", event.PlayerId}};
}

static json ToJSON(const Version &version, const AmbientLightChanged &event) {
    return json{{"Color", event.Color}, {"Intensity", event.Intensity}};
}

static json ToJSON(const Version &version, const TileUpdated &event) {
    return json{{"Position", ToJSON(version, event.Position)},
                {"Objects", ToJSON(version, event.Objects)}};
}

static json ToJSON(const Version &version, const TileObjectAdded &event) {
    return json{{"TilePosition", ToJSON(version, event.TilePosition)},
                {"StackPosition", event.StackPosition},
                {"Object", ToJSON(version, event.Object)}};
}

static json ToJSON(const Version &version, const TileObjectTransformed &event) {
    return json{{"TilePosition", ToJSON(version, event.TilePosition)},
                {"StackPosition", event.StackPosition},
                {"Object", ToJSON(version, event.Object)}};
}

static json ToJSON(const Version &version, const TileObjectRemoved &event) {
    return json{{"TilePosition", ToJSON(version, event.TilePosition)},
                {"StackPosition", event.StackPosition}};
}

static json ToJSON(const Version &version, const CreatureMoved &event) {
    return json{{"CreatureId", event.CreatureId},
                {"From", ToJSON(version, event.From)},
                {"StackPosition", event.StackPosition},
                {"To", ToJSON(version, event.To)}};
}

static json ToJSON(const Version &version, const CreatureRemoved &event) {
    return json{{"CreatureId", event.CreatureId}};
}

static json ToJSON(const Version &version, const CreatureSeen &event) {
    json result{{"CreatureId", event.CreatureId},
                {"Type", event.Type},
                {"Name", CharacterSet::ToUtf8(event.Name)},
                {"Heading", event.Heading},
                {"LightColor", event.LightColor},
                {"LightIntensity", event.LightIntensity},
                {"Outfit", ToJSON(version, event.Outfit)}};

    if (version.Protocol.SkullIcon) {
        result["Skull"] = event.Skull;
    }

    if (version.Protocol.ShieldIcon) {
        result["Shield"] = event.Shield;
    }

    if (version.Protocol.WarIcon) {
        result["War"] = event.War;
    }

    if (version.Protocol.NPCCategory) {
        result["NPCCategory"] = event.NPCCategory;
    }

    if (version.Protocol.CreatureMarks) {
        result["Mark"] = event.Mark;
        result["GuildMembersOnline"] = event.GuildMembersOnline;
        result["MarkIsPermanent"] = event.MarkIsPermanent;
    }

    if (version.Protocol.PassableCreatures) {
        result["Impassable"] = event.Impassable;
    }

    return result;
}

static json ToJSON(const Version &version, const CreatureHealthUpdated &event) {
    return json{{"CreatureId", event.CreatureId}, {"Health", event.Health}};
}

static json ToJSON(const Version &version,
                   const CreatureHeadingUpdated &event) {
    return json{{"CreatureId", event.CreatureId}, {"Heading", event.Heading}};
}

static json ToJSON(const Version &version, const CreatureLightUpdated &event) {
    return json{{"Id", event.CreatureId},
                {"Color", event.Color},
                {"Intensity", event.Intensity}};
}

static json ToJSON(const Version &version, const CreatureOutfitUpdated &event) {
    return json{{"CreatureId", event.CreatureId},
                {"Outfit", ToJSON(version, event.Outfit)}};
}

static json ToJSON(const Version &version, const CreatureSpeedUpdated &event) {
    return json{{"CreatureId", event.CreatureId}, {"Speed", event.Speed}};
}

static json ToJSON(const Version &version, const CreatureSkullUpdated &event) {
    return json{{"CreatureId", event.CreatureId}, {"Skull", event.Skull}};
}

static json ToJSON(const Version &version, const CreatureShieldUpdated &event) {
    return json{{"CreatureId", event.CreatureId}, {"Shield", event.Shield}};
}

static json ToJSON(const Version &version,
                   const CreatureImpassableUpdated &event) {
    return json{{"CreatureId", event.CreatureId},
                {"Impassable", event.Impassable}};
}

static json ToJSON(const Version &version,
                   const CreaturePvPHelpersUpdated &event) {
    return json{{"CreatureId", event.CreatureId},
                {"Mark", event.Mark},
                {"MarkIsPermanent", event.MarkIsPermanent}};
}

static json ToJSON(const Version &version,
                   const CreatureGuildMembersUpdated &event) {
    return json{{"CreatureId", event.CreatureId},
                {"GuildMembersOnline", event.GuildMembersOnline}};
}

static json ToJSON(const Version &version, const CreatureTypeUpdated &event) {
    return json{{"CreatureId", event.CreatureId}, {"Type", event.Type}};
}

static json ToJSON(const Version &version,
                   const CreatureNPCCategoryUpdated &event) {
    return json{{"CreatureId", event.CreatureId},
                {"NPCCategory", event.Category}};
}

static json ToJSON(const Version &version, const PlayerMoved &event) {
    return json{{"Position", ToJSON(version, event.Position)}};
}

static json ToJSON(const Version &version,
                   const PlayerInventoryUpdated &event) {
    return json{{"Slot", event.Slot}, {"Item", ToJSON(version, event.Item)}};
}

static json ToJSON(const Version &version,
                   const PlayerBlessingsUpdated &event) {
    return json{{"Blessings", event.Blessings}};
}

static json ToJSON(const Version &version,
                   const PlayerHotkeyPresetUpdated &event) {
    return json{{"CreatureId", event.CreatureId},
                {"HotkeyPreset", event.HotkeyPreset}};
}

static json ToJSON(const Version &version,
                   const PlayerDataBasicUpdated &event) {
    json result{{"Vocation", event.Vocation},
                {"IsPremium", event.IsPremium},
                {"Spells", event.Spells}};

    if (version.Protocol.PremiumUntil) {
        result["PremiumUntil"] = event.PremiumUntil;
    }

    return result;
}

static json ToJSON(const Version &version, const PlayerDataUpdated &event) {
    json result{{"Health", event.Health},
                {"MaxHealth", event.MaxHealth},
                {"Mana", event.Mana},
                {"MaxMana", event.MaxMana},
                {"Level", event.Level},
                {"Capacity", event.Capacity},
                {"Experience", event.Experience},
                {"MagicLevel", event.MagicLevel}};

    if (version.Protocol.MaxCapacity) {
        result["MaxCapacity"] = event.MaxCapacity;
    }

    if (version.Protocol.SkillPercentages) {
        result["LevelPercent"] = event.LevelPercent;
    }

    if (version.Protocol.ExperienceBonus) {
        result["ExperienceBonus"] = event.ExperienceBonus;
    }

    if (version.Protocol.SkillBonuses) {
        result["MagicLevelBase"] = event.MagicLevelBase;
    }

    if (version.Protocol.SkillPercentages) {
        result["MagicLevelPercent"] = event.MagicLevelPercent;
    }

    if (version.Protocol.SoulPoints) {
        result["SoulPoints"] = event.SoulPoints;
    }

    if (version.Protocol.Stamina) {
        result["Stamina"] = event.Stamina;
    }

    if (version.Protocol.PlayerSpeed) {
        result["Speed"] = event.Speed;
    }

    if (version.Protocol.PlayerHunger) {
        result["Fed"] = event.Fed;
    }

    if (version.Protocol.OfflineStamina) {
        result["OfflineStamina"] = event.OfflineStamina;
    }

    return result;
}

static json ToJSON(const Version &version, const PlayerSkillsUpdated &event) {
    json result;

    using Spec = std::initializer_list<std::pair<std::string, int>>;
    for (const auto &[name, index] : Spec{{"Fist", 0},
                                          {"Club", 1},
                                          {"Sword", 2},
                                          {"Axe", 3},
                                          {"Distance", 4},
                                          {"Shield", 5},
                                          {"Fishing", 6}}) {
        auto &skill = event.Skills[index];
        json object{{"Actual", skill.Actual}};

        if (version.Protocol.SkillBonuses) {
            object["Effective"] = skill.Effective;
        }

        if (version.Protocol.SkillPercentages) {
            object["Percent"] = skill.Percent;
        }

        result[name] = object;
    }

    return result;
}

static json ToJSON(const Version &version, const PlayerIconsUpdated &event) {
    return json{{"Icons", event.Icons}};
}

static json ToJSON(const Version &version, const PlayerTacticsUpdated &event) {
    return json{{"AttackMode", event.AttackMode},
                {"ChaseMode", event.ChaseMode},
                {"PvPMode", event.PvPMode},
                {"SecureMode", event.SecureMode}};
}

static json ToJSON(const Version &version, const PvPSituationsChanged &event) {
    return json{{"OpenSituations", event.OpenSituations}};
}

static json ToJSON(const Version &version, const CreatureSpoke &event) {
    json result{
            {"Mode", event.Mode},
            {"AuthorName", CharacterSet::ToUtf8(event.AuthorName)},
            {"Message", CharacterSet::ToUtf8(event.Message)},
    };

    if (version.Protocol.SpeakerLevel) {
        result["AuthorLevel"] = event.AuthorLevel;
    }

    if (version.Protocol.ReportMessages) {
        result["MessageId"] = event.MessageId;
    }

    return result;
}

static json ToJSON(const Version &version, const CreatureSpokeOnMap &event) {
    json result{{"Mode", event.Mode},
                {"AuthorName", CharacterSet::ToUtf8(event.AuthorName)},
                {"Message", CharacterSet::ToUtf8(event.Message)},
                {"Position", ToJSON(version, event.Position)}};

    if (version.Protocol.SpeakerLevel) {
        result["AuthorLevel"] = event.AuthorLevel;
    }

    if (version.Protocol.ReportMessages) {
        result["MessageId"] = event.MessageId;
    }

    return result;
}

static json ToJSON(const Version &version,
                   const CreatureSpokeInChannel &event) {
    json result{{"Mode", event.Mode},
                {"AuthorName", CharacterSet::ToUtf8(event.AuthorName)},
                {"Message", CharacterSet::ToUtf8(event.Message)},
                {"ChannelId", event.ChannelId}};

    if (version.Protocol.SpeakerLevel) {
        result["AuthorLevel"] = event.AuthorLevel;
    }

    if (version.Protocol.ReportMessages) {
        result["MessageId"] = event.MessageId;
    }

    return result;
}

static json ToJSON(const Version &version, const ChannelListUpdated &event) {
    return json{{"Channels", event.Channels}};
}

static json ToJSON(const Version &version, const ChannelOpened &event) {
    json result{{"ChannelId", event.Id},
                {"ChannelName", CharacterSet::ToUtf8(event.Name)}};

    if (version.Protocol.ChannelParticipants) {
        result["Invitees"] = event.Invitees;
        result["Participants"] = event.Participants;
    }

    return result;
}

static json ToJSON(const Version &version, const ChannelClosed &event) {
    return json{{"ChannelId", event.Id}};
}

static json ToJSON(const Version &version,
                   const PrivateConversationOpened &event) {
    return json{{"PlayerName", event.Name}};
}

static json ToJSON(const Version &version, const ContainerOpened &event) {
    return json{{"ContainerId", event.ContainerId},
                {"ContainerItem", ToJSON(version, Object(event.ItemId))},
                {"ContainerName", CharacterSet::ToUtf8(event.Name)},
                {"Items", ToJSON(version, event.Items)}};
}

static json ToJSON(const Version &version, const ContainerClosed &event) {
    return json{{"ContainerId", event.ContainerId}};
}

static json ToJSON(const Version &version, const ContainerAddedItem &event) {
    return json{{"ContainerId", event.ContainerId},
                {"Item", ToJSON(version, event.Item)}};
}

static json ToJSON(const Version &version,
                   const ContainerTransformedItem &event) {
    return json{{"ContainerId", event.ContainerId},
                {"ContainerIndex", event.ContainerIndex},
                {"Item", ToJSON(version, event.Item)}};
}

static json ToJSON(const Version &version, const ContainerRemovedItem &event) {
    return json{{"ContainerId", event.ContainerId},
                {"ContainerIndex", event.ContainerIndex}};
}

static json ToJSON(const Version &version, const NumberEffectPopped &event) {
    return json{{"Position", ToJSON(version, event.Position)},
                {"Color", event.Color},
                {"Value", event.Value}};
}

static json ToJSON(const Version &version, const GraphicalEffectPopped &event) {
    return json{{"Position", ToJSON(version, event.Position)},
                {"Id", event.Id}};
}

static json ToJSON(const Version &version, const MissileFired &event) {
    return json{{"Origin", ToJSON(version, event.Origin)},
                {"Target", ToJSON(version, event.Target)},
                {"Id", event.Id}};
}

static json ToJSON(const Version &version, const StatusMessageReceived &event) {
    return json{{"Message", CharacterSet::ToUtf8(event.Message)},
                {"Mode", event.Mode}};
}

static json ToJSON(const Version &version,
                   const StatusMessageReceivedInChannel &event) {
    return json{{"Message", CharacterSet::ToUtf8(event.Message)},
                {"Mode", event.Mode},
                {"ChannelId", event.ChannelId}};
}

static json ToJSON(const Version &version, const Events::Base &base) {
    json j;

    switch (base.Kind()) {
    case Events::Type::WorldInitialized:
        j = ToJSON(version,
                   static_cast<const Events::WorldInitialized &>(base));
        break;
    case Events::Type::AmbientLightChanged:
        j = ToJSON(version,
                   static_cast<const Events::AmbientLightChanged &>(base));
        break;
    case Events::Type::TileUpdated:
        j = ToJSON(version, static_cast<const Events::TileUpdated &>(base));
        break;
    case Events::Type::TileObjectAdded:
        j = ToJSON(version, static_cast<const Events::TileObjectAdded &>(base));
        break;
    case Events::Type::TileObjectTransformed:
        j = ToJSON(version,
                   static_cast<const Events::TileObjectTransformed &>(base));
        break;
    case Events::Type::TileObjectRemoved:
        j = ToJSON(version,
                   static_cast<const Events::TileObjectRemoved &>(base));
        break;
    case Events::Type::CreatureMoved:
        j = ToJSON(version, static_cast<const Events::CreatureMoved &>(base));
        break;
    case Events::Type::CreatureRemoved:
        j = ToJSON(version, static_cast<const Events::CreatureRemoved &>(base));
        break;
    case Events::Type::CreatureSeen:
        j = ToJSON(version, static_cast<const Events::CreatureSeen &>(base));
        break;
    case Events::Type::CreatureHealthUpdated:
        j = ToJSON(version,
                   static_cast<const Events::CreatureHealthUpdated &>(base));
        break;
    case Events::Type::CreatureHeadingUpdated:
        j = ToJSON(version,
                   static_cast<const Events::CreatureHeadingUpdated &>(base));
        break;
    case Events::Type::CreatureLightUpdated:
        j = ToJSON(version,
                   static_cast<const Events::CreatureLightUpdated &>(base));
        break;
    case Events::Type::CreatureOutfitUpdated:
        j = ToJSON(version,
                   static_cast<const Events::CreatureOutfitUpdated &>(base));
        break;
    case Events::Type::CreatureSpeedUpdated:
        j = ToJSON(version,
                   static_cast<const Events::CreatureSpeedUpdated &>(base));
        break;
    case Events::Type::CreatureSkullUpdated:
        j = ToJSON(version,
                   static_cast<const Events::CreatureSkullUpdated &>(base));
        break;
    case Events::Type::CreatureShieldUpdated:
        j = ToJSON(version,
                   static_cast<const Events::CreatureShieldUpdated &>(base));
        break;
    case Events::Type::CreatureImpassableUpdated:
        j = ToJSON(
                version,
                static_cast<const Events::CreatureImpassableUpdated &>(base));
        break;
    case Events::Type::CreaturePvPHelpersUpdated:
        j = ToJSON(
                version,
                static_cast<const Events::CreaturePvPHelpersUpdated &>(base));
        break;
    case Events::Type::CreatureGuildMembersUpdated:
        j = ToJSON(
                version,
                static_cast<const Events::CreatureGuildMembersUpdated &>(base));
        break;
    case Events::Type::CreatureTypeUpdated:
        j = ToJSON(version,
                   static_cast<const Events::CreatureTypeUpdated &>(base));
        break;
    case Events::Type::CreatureNPCCategoryUpdated:
        j = ToJSON(
                version,
                static_cast<const Events::CreatureNPCCategoryUpdated &>(base));
        break;
    case Events::Type::PlayerMoved:
        j = ToJSON(version, static_cast<const Events::PlayerMoved &>(base));
        break;
    case Events::Type::PlayerInventoryUpdated:
        j = ToJSON(version,
                   static_cast<const Events::PlayerInventoryUpdated &>(base));
        break;
    case Events::Type::PlayerBlessingsUpdated:
        j = ToJSON(version,
                   static_cast<const Events::PlayerBlessingsUpdated &>(base));
        break;
    case Events::Type::PlayerHotkeyPresetUpdated:
        j = ToJSON(
                version,
                static_cast<const Events::PlayerHotkeyPresetUpdated &>(base));
        break;
    case Events::Type::PlayerDataBasicUpdated:
        j = ToJSON(version,
                   static_cast<const Events::PlayerDataBasicUpdated &>(base));
        break;
    case Events::Type::PlayerDataUpdated:
        j = ToJSON(version,
                   static_cast<const Events::PlayerDataUpdated &>(base));
        break;
    case Events::Type::PlayerSkillsUpdated:
        j = ToJSON(version,
                   static_cast<const Events::PlayerSkillsUpdated &>(base));
        break;
    case Events::Type::PlayerIconsUpdated:
        j = ToJSON(version,
                   static_cast<const Events::PlayerIconsUpdated &>(base));
        break;
    case Events::Type::PlayerTacticsUpdated:
        j = ToJSON(version,
                   static_cast<const Events::PlayerTacticsUpdated &>(base));
        break;
    case Events::Type::PvPSituationsChanged:
        j = ToJSON(version,
                   static_cast<const Events::PvPSituationsChanged &>(base));
        break;
    case Events::Type::CreatureSpoke:
        j = ToJSON(version, static_cast<const Events::CreatureSpoke &>(base));
        break;
    case Events::Type::CreatureSpokeOnMap:
        j = ToJSON(version,
                   static_cast<const Events::CreatureSpokeOnMap &>(base));
        break;
    case Events::Type::CreatureSpokeInChannel:
        j = ToJSON(version,
                   static_cast<const Events::CreatureSpokeInChannel &>(base));
        break;
    case Events::Type::ChannelListUpdated:
        j = ToJSON(version,
                   static_cast<const Events::ChannelListUpdated &>(base));
        break;
    case Events::Type::ChannelOpened:
        j = ToJSON(version, static_cast<const Events::ChannelOpened &>(base));
        break;
    case Events::Type::ChannelClosed:
        j = ToJSON(version, static_cast<const Events::ChannelClosed &>(base));
        break;
    case Events::Type::PrivateConversationOpened:
        j = ToJSON(
                version,
                static_cast<const Events::PrivateConversationOpened &>(base));
        break;
    case Events::Type::ContainerOpened:
        j = ToJSON(version, static_cast<const Events::ContainerOpened &>(base));
        break;
    case Events::Type::ContainerClosed:
        j = ToJSON(version, static_cast<const Events::ContainerClosed &>(base));
        break;
    case Events::Type::ContainerAddedItem:
        j = ToJSON(version,
                   static_cast<const Events::ContainerAddedItem &>(base));
        break;
    case Events::Type::ContainerTransformedItem:
        j = ToJSON(version,
                   static_cast<const Events::ContainerTransformedItem &>(base));
        break;
    case Events::Type::ContainerRemovedItem:
        j = ToJSON(version,
                   static_cast<const Events::ContainerRemovedItem &>(base));
        break;
    case Events::Type::NumberEffectPopped:
        j = ToJSON(version,
                   static_cast<const Events::NumberEffectPopped &>(base));
        break;
    case Events::Type::GraphicalEffectPopped:
        j = ToJSON(version,
                   static_cast<const Events::GraphicalEffectPopped &>(base));
        break;
    case Events::Type::MissileFired:
        j = ToJSON(version, static_cast<const Events::MissileFired &>(base));
        break;
    case Events::Type::StatusMessageReceived:
        j = ToJSON(version,
                   static_cast<const Events::StatusMessageReceived &>(base));
        break;
    case Events::Type::StatusMessageReceivedInChannel:
        j = ToJSON(version,
                   static_cast<const Events::StatusMessageReceivedInChannel &>(
                           base));
        break;
    default:
        throw InvalidDataError();
    }

    j["Event"] = base.Kind();

    return j;
}
} // namespace Events

namespace Serializer {

static auto Open(const Settings &settings,
                 const std::string &dataFolder,
                 const std::string &path,
                 const DataReader &reader) {
    auto inputFormat = settings.InputFormat;
    int major, minor, preview;

    if (inputFormat == Recordings::Format::Unknown) {
        inputFormat = Recordings::GuessFormat(path, reader);
        fprintf(stderr,
                "warning: Unknown recording format, guessing %s\n",
                Recordings::FormatName(inputFormat).c_str());
        fflush(stderr);
    }

    major = settings.DesiredTibiaVersion.Major;
    minor = settings.DesiredTibiaVersion.Minor;
    preview = settings.DesiredTibiaVersion.Preview;

    /* Ask the file for the version if none was provided. */
    if ((major | minor | preview) == 0) {
        if (!Recordings::QueryTibiaVersion(inputFormat,
                                           reader,
                                           major,
                                           minor,
                                           preview)) {
            throw InvalidDataError();
        }
        fprintf(stderr,
                "warning: Unknown recording version, guessing %i.%i (%i)\n",
                major,
                minor,
                preview);
        fflush(stderr);
    }

    const MemoryFile pictures(dataFolder + DIR_SEP "Tibia.pic");
    const MemoryFile sprites(dataFolder + DIR_SEP "Tibia.spr");
    const MemoryFile types(dataFolder + DIR_SEP "Tibia.dat");

    auto version = std::make_unique<Version>(major,
                                             minor,
                                             preview,
                                             pictures.Reader(),
                                             sprites.Reader(),
                                             types.Reader());
    auto recording = Recordings::Read(inputFormat, reader, *version);

    return std::make_tuple(std::move(recording), std::move(version));
}

void Serialize(const Settings &settings,
               const std::string &dataFolder,
               const std::string &inputPath,
               std::ostream &output) {
    const MemoryFile file(inputPath);

    auto [recording, version] =
            Open(settings, dataFolder, inputPath, file.Reader());

    auto frames = std::vector<json>();

    for (const auto &frame : recording->Frames) {
        auto events = std::vector<json>();

        for (const auto &event : frame.Events) {
            if (settings.SkippedEvents.contains(event->Kind())) {
                continue;
            }

            events.push_back(ToJSON(*version, *event));
        }

        if (!events.empty()) {
            frames.emplace_back<json>(
                    {{"Timestamp", frame.Timestamp}, {"Events", events}});
        }
    }

    output << frames << std::flush;
}
} // namespace Serializer
} // namespace trc