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

/* FIXME: This is a pretty horrendous translation layer. While the protocol
 * stuff is mostly fine, we need a better way to deal with type properties,
 * message types, and so on.
 *
 * What we have right now "works" insofar as parsing goes, but can be subtly
 * incorrect and misinterpret e.g. login messages as look messages. Once we've
 * got a reasonably sized corpus of recordings, it would be nice to start over
 * from scratch and methodically map out all the constants used from 7.0
 * onwards. */

#include "versions.hpp"
#include "utils.hpp"

#ifdef DUMP_ITEMS
#    include "renderer.hpp"
#    include "textrenderer.hpp"
#endif

#include <bit>
#include <cstddef>
#include <cstdlib>
#include <format>

namespace trc {

void VersionBase::InitTypeProperties() {
    auto &table = TypeProperties;

    /* 7.00 - 7.30, serving as a baseline. */
    AbortUnless(AtLeast(7, 00));
    table.Insert(0, TypeProperty::Ground);
    table.Insert(1, TypeProperty::Clip);
    table.Insert(2, TypeProperty::Bottom);
    table.Insert(3, TypeProperty::Container);
    table.Insert(4, TypeProperty::Stackable);
    table.Insert(5, TypeProperty::Usable);
    table.Insert(6, TypeProperty::ForceUse);
    table.Insert(7, TypeProperty::Write);
    table.Insert(8, TypeProperty::WriteOnce);
    table.Insert(9, TypeProperty::LiquidContainer);
    table.Insert(10, TypeProperty::LiquidPool);
    table.Insert(11, TypeProperty::Blocking);
    table.Insert(12, TypeProperty::Unmovable);
    table.Insert(13, TypeProperty::Blocking);
    table.Insert(14, TypeProperty::Unpathable);
    table.Insert(15, TypeProperty::Takeable);
    table.Insert(16, TypeProperty::Light);
    table.Insert(17, TypeProperty::DontHide);
    table.Insert(18, TypeProperty::Blocking);
    table.Insert(19, TypeProperty::Height);
    table.Insert(20, TypeProperty::DisplacementLegacy);
    table.Gap(21);
    table.Insert(22, TypeProperty::Automap);
    table.Insert(23, TypeProperty::Rotate);
    table.Insert(24, TypeProperty::Corpse);
    table.Insert(25, TypeProperty::Hangable);
    table.Insert(26, TypeProperty::UnknownU16);
    table.Insert(27, TypeProperty::Horizontal);
    table.Insert(28, TypeProperty::AnimateIdle);
    table.Insert(29, TypeProperty::Lenshelp);

    if (AtLeast(7, 40)) {
        table.Replace(26, TypeProperty::Vertical, TypeProperty::UnknownU16);
    }

    if (AtLeast(7, 55)) {
        table.Insert(3, TypeProperty::Top, TypeProperty::Container);

        /* FORCE_USE and USABLE have changed places. */
        table.Replace(6, TypeProperty::ForceUse, TypeProperty::Usable);
        table.Replace(7, TypeProperty::Usable, TypeProperty::ForceUse);

        /* Lots of fields were reordered, seemingly for the heck of it. Did
         * they sort an enum or something? */
        table.Replace(17, TypeProperty::Hangable, TypeProperty::Light);
        table.Replace(18, TypeProperty::Vertical, TypeProperty::DontHide);
        table.Replace(19, TypeProperty::Horizontal, TypeProperty::Blocking);
        table.Replace(20, TypeProperty::Rotate, TypeProperty::Height);
        table.Replace(21,
                      TypeProperty::Light,
                      TypeProperty::DisplacementLegacy);
        table.Replace(22, TypeProperty::DontHide);
        table.Replace(23, TypeProperty::Translucent, TypeProperty::Automap);
        table.Replace(24, TypeProperty::Displacement, TypeProperty::Rotate);
        table.Replace(25, TypeProperty::Height, TypeProperty::Corpse);
        table.Replace(26,
                      TypeProperty::RedrawNearbyTop,
                      TypeProperty::Hangable);
        table.Replace(27, TypeProperty::AnimateIdle, TypeProperty::Vertical);
        table.Replace(28, TypeProperty::Automap, TypeProperty::Horizontal);
        table.Replace(29, TypeProperty::Lenshelp, TypeProperty::AnimateIdle);
        table.Insert(30, TypeProperty::Walkable, TypeProperty::Lenshelp);
    }

    if (AtLeast(7, 80)) {
        table.Insert(8, TypeProperty::Rune, TypeProperty::Write);
        table.Insert(32, TypeProperty::LookThrough, TypeProperty::Lenshelp);
    }

    if (AtLeast(8, 60)) {
        table.Remove(8, TypeProperty::Rune);
    }

    /* FIXME: These are completely taken out of thin air. I do not know the
     * exact versions in which they appear.
     *
     * TODO: It would be nice if we had a tool to test this alone. Perhaps we
     * should break out `DumpItems` into a simple utility as
     * additional justification for this. */
    if (AtLeast(9, 80)) {
        table.Insert(33, TypeProperty::MarketItem);
        table.Insert(34, TypeProperty::DefaultAction);
        table.Insert(35, TypeProperty::Wrappable);
        table.Insert(36, TypeProperty::TopEffect);
    }

    if (AtLeast(10, 10)) {
        table.Insert(16, TypeProperty::NoMoveAnimation, TypeProperty::Takeable);
    }

    /* TODO: Figure out which versions these belong to:
     *
     * 33:  TypeProperty::EquipmentSlot,
     * 34:  TypeProperty::MarketItem,
     * 35:  TypeProperty::DefaultAction,
     * 36:  TypeProperty::Wrappable,
     * 37:  TypeProperty::Unwrappable,
     * 38:  TypeProperty::TopEffect */
}

void VersionBase::InitUnifiedMessageTypes(
        TranslationTable<MessageMode> &table) {
    table.Insert(1, MessageMode::Say);
    table.Insert(2, MessageMode::Whisper);
    table.Insert(3, MessageMode::Yell);
    table.Insert(4, MessageMode::PrivateIn);
    table.Insert(5, MessageMode::PrivateOut);
    table.Insert(6, MessageMode::ChannelWhite);
    table.Insert(7, MessageMode::ChannelWhite);
    table.Insert(8, MessageMode::ChannelWhite);
    table.Insert(9, MessageMode::Spell);
    table.Insert(10, MessageMode::NPCStart);
    table.Insert(11, MessageMode::PlayerToNPC);
    table.Insert(12, MessageMode::Broadcast);
    table.Insert(13, MessageMode::ChannelRed);
    table.Insert(14, MessageMode::GMToPlayer);
    table.Insert(15, MessageMode::PlayerToGM);
    table.Insert(16, MessageMode::Login);
    table.Insert(17, MessageMode::Warning);
    table.Insert(18, MessageMode::Game);
    table.Insert(19, MessageMode::Failure);
    table.Insert(20, MessageMode::Look);
    table.Insert(21, MessageMode::DamageDealt);
    table.Insert(22, MessageMode::DamageReceived);
    table.Insert(23, MessageMode::Healing);
    table.Insert(24, MessageMode::Experience);
    table.Insert(25, MessageMode::DamageReceivedOthers);
    table.Insert(26, MessageMode::HealingOthers);
    table.Insert(27, MessageMode::ExperienceOthers);
    table.Insert(28, MessageMode::Status);
    table.Insert(29, MessageMode::Loot);
    table.Insert(30, MessageMode::NPCTrade);
    table.Insert(31, MessageMode::Guild);
    table.Insert(32, MessageMode::PartyWhite);
    table.Insert(33, MessageMode::Party);
    table.Insert(34, MessageMode::MonsterSay);
    table.Insert(35, MessageMode::MonsterYell);
    table.Insert(36, MessageMode::Report);
    table.Insert(37, MessageMode::Hotkey);
    table.Insert(38, MessageMode::Tutorial);
    table.Insert(39, MessageMode::ThankYou);
    table.Insert(40, MessageMode::Market);
    table.Insert(41, MessageMode::Mana);

    if (AtLeast(10, 36)) {
        table.Insert(11, MessageMode::PlayerToNPC, MessageMode::NPCContinued);
    }

    if (AtLeast(10, 54)) {
        table.Insert(29, MessageMode::Failure, MessageMode::Game);
    }
}

void VersionBase::InitMessageTypes() {
    auto &table = MessageModes;

    if (AtLeast(9, 00)) {
        InitUnifiedMessageTypes(table);
        return;
    }

    /* 7.11, serving as a baseline. */
    AbortUnless(AtLeast(7, 11));
    table.Insert(14, MessageMode::ConsoleOrange);
    table.Insert(15, MessageMode::Broadcast);
    table.Insert(16, MessageMode::Game);
    table.Insert(17, MessageMode::Login);
    table.Insert(18, MessageMode::Status);
    table.Insert(19, MessageMode::Look);
    table.Insert(20, MessageMode::Failure);

    if (AtLeast(7, 20)) {
        /* Dummy entry, no idea where this should be. */
        table.Gap(0);

        table.Insert(17, MessageMode::Warning, MessageMode::Game);
    }

    if (AtLeast(7, 24)) {
        /* Dummy entry, no idea where this should be. */
        table.Gap(0);
    }

    if (AtLeast(8, 20)) {
        table.Insert(17, MessageMode::ConsoleRed, MessageMode::Broadcast);
        table.Gap(18, MessageMode::Broadcast);
    }

    if (AtLeast(8, 40)) {
        table.Insert(20, MessageMode::ConsoleOrange, MessageMode::Warning);
    }

    /* TibiaCamTV decided to move their slogan to MessageMode::Warning
     * in 8.60, keep that in mind when adding new versions. */

    if (AtLeast(8, 61)) {
        table.Remove(0);
        table.Remove(0);
        table.Remove(0);
        table.Remove(0);
        table.Remove(0);
        table.Remove(0);
        table.Insert(22, MessageMode::Warning);
    }
}

void VersionBase::InitSpeakTypes() {
    auto &table = SpeakModes;

    if (AtLeast(9, 00)) {
        InitUnifiedMessageTypes(table);
        return;
    }

    /* 7.11, serving as a baseline. */
    AbortUnless(AtLeast(7, 11));
    table.Insert(1, MessageMode::Say);
    table.Insert(2, MessageMode::Whisper);
    table.Insert(3, MessageMode::Yell);
    table.Insert(4, MessageMode::PrivateIn);
    table.Insert(5, MessageMode::ChannelYellow);
    table.Insert(6, MessageMode::RuleViolationChannel);
    table.Insert(7, MessageMode::RuleViolationAnswer);
    table.Insert(8, MessageMode::RuleViolationContinue);
    table.Insert(9, MessageMode::Broadcast);
    table.Insert(10, MessageMode::ChannelRed);
    table.Insert(11, MessageMode::GMToPlayer);
    table.Insert(12, MessageMode::ChannelAnonymousRed);
    table.Insert(13, MessageMode::MonsterSay);
    table.Insert(14, MessageMode::MonsterYell);

    if (AtLeast(7, 20)) {
        table.Insert(12,
                     MessageMode::ChannelOrange,
                     MessageMode::ChannelAnonymousRed);
        table.Gap(13, MessageMode::ChannelAnonymousRed);
    }

    if (AtLeast(7, 23)) {
        table.Gap(15, MessageMode::MonsterSay);
    }

    if (AtLeast(8, 20)) {
        table.Insert(4, MessageMode::PlayerToNPC, MessageMode::PrivateIn);
        table.Insert(5, MessageMode::NPCStart, MessageMode::PrivateIn);
    }

    if (AtLeast(8, 40)) {
        table.Insert(8,
                     MessageMode::ChannelWhite,
                     MessageMode::RuleViolationChannel);
    }

    if (AtLeast(8, 61)) {
        table.Remove(9, MessageMode::RuleViolationChannel);
        table.Remove(9, MessageMode::RuleViolationAnswer);
        table.Remove(9, MessageMode::RuleViolationContinue);

        table.Remove(13);
        table.Remove(13, MessageMode::ChannelAnonymousRed);
        table.Remove(13);
    }
}

void VersionBase::InitFeatures() {
    Features.CapacityDivisor = 1;

    if (AtLeast(7, 50)) {
        Features.IconBar = true;
    }

    if (AtLeast(7, 55)) {
        Features.TypeZDiv = true;
    }

    if (AtLeast(8, 30)) {
        Features.CapacityDivisor = 100;
    }

    if (AtLeast(8, 53)) {
        Features.ModernStacking = true;
    }

    if (AtLeast(9, 6)) {
        Features.SpriteIndexU32 = true;
    }

    if (AtLeast(10, 50)) {
        Features.AnimationPhases = true;
    }

    if (AtLeast(10, 57)) {
        Features.FrameGroups = true;
    }
}

void VersionBase::InitProtocol() {
    if (AtLeast(7, 20)) {
        Protocol.BugReporting = true;
        Protocol.SkullIcon = true;
    }

    if (AtLeast(7, 24)) {
        Protocol.ShieldIcon = true;
    }

    if (AtLeast(7, 40)) {
        Protocol.MoveDeniedDirection = true;
        Protocol.SkillPercentages = true;
    }

    if (AtLeast(7, 50)) {
        Protocol.SoulPoints = true;
    }

    if (AtLeast(7, 55)) {
        Protocol.RawEffectIds = true;
    }

    if (AtLeast(7, 60)) {
        Protocol.TextEditAuthorName = true;
        Protocol.LevelU16 = true;
    }

    if (AtLeast(7, 70)) {
        Protocol.ReportMessages = true;
        Protocol.OutfitsU16 = true;
    }

    if (AtLeast(7, 80)) {
        Protocol.RuneChargeCount = true;
        Protocol.OutfitAddons = true;
        Protocol.Stamina = true;
        Protocol.SpeakerLevel = true;
        Protocol.IconsU16 = true;
    }

    if (AtLeast(7, 90)) {
        Protocol.TextEditDate = true;
        Protocol.OutfitNames = true;
    }

    if (AtLeast(8, 30)) {
        Protocol.NPCVendorWeight = true;
        Protocol.CapacityU32 = true;
    }

    if (AtLeast(8, 41)) {
        Protocol.AddObjectStackPosition = true;
    }

    if (AtLeast(8, 42)) {
        Protocol.TextEditObject = true;
    }

    if (AtLeast(8, 53)) {
        Protocol.PassableCreatures = true;
    }

    if (AtLeast(8, 54)) {
        Protocol.WarIcon = true;
    }

    if (AtLeast(8, 60)) {
        Protocol.CancelAttackId = true;
    }

    if (AtLeast(8, 70)) {
        Protocol.Mounts = true;
    }

    /* HAZY: Catch-all for properties of unknown versions to get 8.55 rolling.
     *
     * These may belong to any version between 8.55 and 9.32. */
    if (AtLeast(9, 0)) {
        Protocol.CancelAttackId = true;
        Protocol.EnvironmentalEffects = true;
        Protocol.MaxCapacity = true;
        Protocol.ExperienceU64 = true;
        Protocol.PlayerSpeed = true;
        Protocol.PlayerHunger = true;
        Protocol.ItemAnimation = true;
        Protocol.NPCVendorName = true;
        Protocol.MessageEffects = true;
        Protocol.ChannelParticipants = true;

        /* ?? */
        Protocol.SpeedAdjustment = true;
        Protocol.CreatureTypes = true;
        Protocol.SkillBonuses = true;
    }

    if (AtLeast(9, 32)) {
        Protocol.NPCVendorItemCountU16 = true;
    }

    if (AtLeast(9, 54)) {
        Protocol.OfflineStamina = true;
        Protocol.PassableCreatureUpdate = true;
    }

    if (AtLeast(9, 62)) {
        Protocol.ExtendedVIPData = true;
    }

    if (AtLeast(9, 72)) {
        Protocol.PlayerMoneyU64 = true;
        Protocol.ExtendedDeathDialog = true;
    }

    if (AtLeast(9, 83)) {
        Protocol.ContainerIndexU16 = true;
        Protocol.NullObjects = true;
    }

    if (AtLeast(9, 83, 1)) {
        Protocol.PreviewByte = true;
    }

    if (AtLeast(9, 84)) {
        Protocol.PreviewByte = true;
        Protocol.ContainerPagination = true;
    }

    if (AtLeast(9, 85, 1)) {
        Protocol.CreatureMarks = true;
        Protocol.ItemMarks = true;
    }

    if (AtLeast(10, 36)) {
        Protocol.NPCCategory = true;
        Protocol.SinglePvPHelper = true;
        Protocol.LoyaltyBonus = true;
    }

    if (AtLeast(10, 37)) {
        Protocol.PremiumUntil = true;
    }

    if (AtLeast(10, 52, 1)) {
        Protocol.PvPFraming = true;
    }

    if (AtLeast(10, 53, 1)) {
        Protocol.ExperienceBonus = true;
    }

    if (AtLeast(10, 55)) {
        Protocol.UnfairFightReduction = true;
    }

    if (AtLeast(10, 58)) {
        Protocol.ExpertMode = true;
    }

    if (AtLeast(10, 59)) {
        Protocol.CreatureSpeedPadding = true;
    }

    if (AtLeast(10, 65)) {
        Protocol.GuildPartyChannelId = true;
    }

    if (AtLeast(10, 95)) {
        Protocol.SkillsUnknownPadding = true;

        /* ??? */
        Protocol.OutfitCountU16 = true;
    }
}

TypeProperty VersionBase::TranslateTypeProperty(uint8_t index) const {
    if (index == 255) {
        return TypeProperty::EntryEndMarker;
    }

    return TypeProperties.Get(index);
}

MessageMode VersionBase::TranslateSpeakMode(uint8_t index) const {
    return SpeakModes.Get(index);
}

MessageMode VersionBase::TranslateMessageMode(uint8_t index) const {
    return MessageModes.Get(index);
}

uint8_t VersionBase::TranslateFluidColor(uint8_t color) const {
    enum {
        FLUID_EMPTY = 0x00,
        FLUID_BLUE = 0x01,
        FLUID_RED = 0x02,
        FLUID_BROWN = 0x03,
        FLUID_GREEN = 0x04,
        FLUID_YELLOW = 0x05,
        FLUID_WHITE = 0x06,
        FLUID_PURPLE = 0x07
    };

    static const char until_1095[] = {FLUID_EMPTY,
                                      FLUID_BLUE,
                                      FLUID_PURPLE,
                                      FLUID_BROWN,
                                      FLUID_BROWN,
                                      FLUID_RED,
                                      FLUID_GREEN,
                                      FLUID_BROWN,
                                      FLUID_YELLOW,
                                      FLUID_WHITE,
                                      FLUID_PURPLE,
                                      FLUID_RED,
                                      FLUID_YELLOW,
                                      FLUID_BROWN,
                                      FLUID_YELLOW,
                                      FLUID_WHITE,
                                      FLUID_BLUE,
                                      FLUID_PURPLE};

    if (AtLeast(7, 80)) {
        if (color >= sizeof(until_1095)) {
            throw InvalidDataError();
        }

        return until_1095[color];
    }

    return color % 8;
}

#ifdef DUMP_ITEMS
#    include "renderer.hpp"
#    include "textrenderer.hpp"

#    define DUMP_PROPERTY(Name)                                                \
        if (properties.Name) {                                                 \
            TextRenderer::DrawString(Fonts.InterfaceSmall,                     \
                                     fontColor,                                \
                                     0 + (emitted % 2) * 64,                   \
                                     66 + (emitted / 2) * 14,                  \
                                     #Name,                                    \
                                     canvas);                                  \
            emitted++;                                                         \
        }

void Version::DumpItems() {
    const Pixel fontColor(0xFF, 0xFF, 0xFF);
    Canvas canvas(128, 128);

    for (int i = 100; i < Types.ItemMaxId; i++) {
        const auto &properties = Types.GetItem(i).Properties;
        int emitted = 0;

        canvas.Wipe();
        Renderer::DumpItem(*this, i, canvas);

        DUMP_PROPERTY(Animated);
        DUMP_PROPERTY(AnimateIdle);
        DUMP_PROPERTY(Blocking);
        DUMP_PROPERTY(Container);
        DUMP_PROPERTY(Corpse);
        DUMP_PROPERTY(DontHide);
        DUMP_PROPERTY(EquipmentSlotRestricted);
        DUMP_PROPERTY(ForceUse);
        DUMP_PROPERTY(Hangable);
        DUMP_PROPERTY(Horizontal);
        DUMP_PROPERTY(LiquidContainer);
        DUMP_PROPERTY(LiquidPool);
        DUMP_PROPERTY(LookThrough);
        DUMP_PROPERTY(MarketItem);
        DUMP_PROPERTY(MultiUse);
        DUMP_PROPERTY(NoMoveAnimation);
        DUMP_PROPERTY(RedrawNearbyTop);
        DUMP_PROPERTY(Rotate);
        DUMP_PROPERTY(Rune);
        DUMP_PROPERTY(Stackable);
        DUMP_PROPERTY(Takeable);
        DUMP_PROPERTY(TopEffect);
        DUMP_PROPERTY(Translucent);
        DUMP_PROPERTY(UnknownU16);
        DUMP_PROPERTY(Unlookable);
        DUMP_PROPERTY(Unmovable);
        DUMP_PROPERTY(Unpathable);
        DUMP_PROPERTY(Unwrappable);
        DUMP_PROPERTY(Usable);
        DUMP_PROPERTY(Vertical);
        DUMP_PROPERTY(Walkable);
        DUMP_PROPERTY(Wrappable);
        DUMP_PROPERTY(Write);
        DUMP_PROPERTY(WriteOnce);

        canvas.Dump(std::format("items/{}.bmp", i));
    }
}
#endif

VersionTriplet::operator std::string() const {
    return std::format(
            "{}.{}{}",
            Major,
            Minor,
            (Preview > 0 ? std::format(".{}", Preview) : std::string()));
}

VersionBase::VersionBase(const VersionTriplet &triplet)
    : Triplet(triplet), Protocol({}), Features({}) {
    InitTypeProperties();
    InitMessageTypes();
    InitSpeakTypes();
    InitFeatures();
    InitProtocol();
}

Version::Version(const VersionTriplet &triplet,
                 const DataReader &pictureData,
                 const DataReader &spriteData,
                 const DataReader &typeData)
    : VersionBase(triplet),
      Pictures(static_cast<VersionBase &>(*this), pictureData),
      Sprites(static_cast<VersionBase &>(*this), spriteData),
      Types(*this, typeData),
      Icons(*this),
      Fonts(*this) {
}

} // namespace trc