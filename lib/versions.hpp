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

#ifndef __TRC_VERSIONS_HPP__
#define __TRC_VERSIONS_HPP__

#include <cstdint>

#include "versions_decl.hpp"

#include "fonts.hpp"
#include "icons.hpp"
#include "message.hpp"
#include "pictures.hpp"
#include "types.hpp"

#include <type_traits>
#include <algorithm>

namespace trc {

/* FIXME: Make this less awful. */
template <typename T, size_t Limit = 64> class TranslationTable {
    using U = std::underlying_type<T>::type;

    static_assert(Limit > 0);

    ptrdiff_t Max;
    int8_t Map[Limit];

    void Insert(ptrdiff_t index, U value, U expected) {
        AbortUnless(CheckRange(Max, -1, Limit - 1));
        AbortUnless(CheckRange(index, 0, Limit - 1));

        if (index <= Max) {
            AbortUnless(expected == Map[index]);
            memmove(&Map[index + 1], &Map[index], Max - index + 1);
            Max++;
        } else {
            AbortUnless(expected == -1);
        }

        Max = std::max(Max, index);
        Map[index] = value;
    }

    void Replace(ptrdiff_t index, U value, U expected) {
        AbortUnless(index <= Max);
        AbortUnless(Map[index] == expected);
        Map[index] = value;
    }

    void Remove(ptrdiff_t index, U expected) {
        AbortUnless(Map[index] == expected);

        if (index < Max) {
            memmove(&Map[index], &Map[index + 1], Max - index);
        } else {
            AbortUnless(index == Max);
            Map[index] = -1;
        }

        Max--;
    }

public:
    void Insert(size_t index, T value, T expected) {
        Insert(index, static_cast<U>(value), static_cast<U>(expected));
    }

    void Insert(size_t index, T value) {
        Insert(index, static_cast<U>(value), -1);
    }

    void Gap(size_t index, T expected) {
        Insert(index, -1, static_cast<U>(expected));
    }

    void Gap(size_t index) {
        Insert(index, -1, -1);
    }

    void Replace(size_t index, T value, T expected) {
        Replace(index, static_cast<U>(value), static_cast<U>(expected));
    }

    void Replace(size_t index, T value) {
        Replace(index, static_cast<U>(value), -1);
    }

    void Remove(size_t index, T expected) {
        Remove(index, static_cast<U>(expected));
    }

    void Remove(size_t index) {
        Remove(index, -1);
    }

    T Get(size_t index) const {
        Assert(CheckRange(Max, 0, Limit - 1));

        if (!CheckRange(index, 0, Max) || Map[index] == -1) {
            throw InvalidDataError();
        }

        return static_cast<T>(Map[index]);
    }

    TranslationTable() {
        memset(Map, -1, sizeof(Map));
        Max = -1;
    }
};

struct VersionTriplet {
    int Major, Minor, Preview;

    VersionTriplet(int major, int minor, int preview)
        : Major(major), Minor(minor), Preview(preview) {
    }

    VersionTriplet(const VersionTriplet &other)
        : VersionTriplet(other.Major, other.Minor, other.Preview) {
    }

    VersionTriplet() : VersionTriplet(0, 0, 0) {
    }

    std::strong_ordering operator<=>(const VersionTriplet &other) const =
            default;
    operator std::string() const;
};

struct VersionBase {
    VersionTriplet Triplet;

    struct {
        bool AddObjectStackPosition : 1;
        bool BugReporting : 1;
        bool CancelAttackId : 1;
        bool CapacityU32 : 1;
        bool ChannelParticipants : 1;
        bool ContainerIndexU16 : 1;
        bool ContainerPagination : 1;
        bool CreatureMarks : 1;
        bool CreatureSpeedPadding : 1;
        bool CreatureTypes : 1;
        bool EnvironmentalEffects : 1;
        bool ExperienceBonus : 1;
        bool ExperienceU64 : 1;
        bool ExpertMode : 1;
        bool ExtendedDeathDialog : 1;
        bool ExtendedVIPData : 1;
        bool IconsU16 : 1;
        bool ItemAnimation : 1;
        bool ItemMarks : 1;
        bool LevelU16 : 1;
        bool LoyaltyBonus : 1;
        bool MaxCapacity : 1;
        bool MessageEffects : 1;
        bool Mounts : 1;
        bool MoveDeniedDirection : 1;
        bool NPCCategory : 1;
        bool NPCVendorItemCountU16 : 1;
        bool NPCVendorName : 1;
        bool NPCVendorWeight : 1;
        bool NullObjects : 1;
        bool OfflineStamina : 1;
        bool OutfitAddons : 1;
        bool OutfitCountU16 : 1;
        bool OutfitNames : 1;
        bool OutfitsU16 : 1;
        bool GuildPartyChannelId : 1;
        bool PassableCreatures : 1;
        bool PassableCreatureUpdate : 1;
        bool PlayerHunger : 1;
        bool PlayerMoneyU64 : 1;
        bool PlayerSpeed : 1;
        bool PremiumUntil : 1;
        bool PreviewByte : 1;
        bool PvPFraming : 1;
        bool RawEffectIds : 1;
        bool ReportMessages : 1;
        bool RuneChargeCount : 1;
        bool ShieldIcon : 1;
        bool SinglePvPHelper : 1;
        bool SkillBonuses : 1;
        bool SkillPercentages : 1;
        bool SkillsU16 : 1; /* Implies LoyaltyBonus and SkillPercentages */
        bool SkillsUnknownPadding : 1;
        bool SkullIcon : 1;
        bool SoulPoints : 1;
        bool SpeakerLevel : 1;
        bool SpeedAdjustment : 1;
        bool Stamina : 1;
        bool TextEditAuthorName : 1;
        bool TextEditDate : 1;
        bool TextEditObject : 1;
        bool TibiacastBuggedInitialization : 1;
        bool UnfairFightReduction : 1;
        bool WarIcon : 1;
    } Protocol;

    struct {
        uint8_t CapacityDivisor;

        bool AnimationPhases : 1;
        bool FrameGroups : 1;
        bool IconBar : 1;
        bool ModernStacking : 1;
        bool SpriteIndexU32 : 1;
        bool TypeZDiv : 1;
    } Features;

    void InitUnifiedMessageTypes(TranslationTable<MessageMode> &table);
    void InitTypeProperties();
    void InitMessageTypes();
    void InitSpeakTypes();
    void InitFeatures();
    void InitProtocol();

    bool AtLeast(int major, int minor, int preview = 0) const {
        return Triplet >= VersionTriplet(major, minor, preview);
    }

    TypeProperty TranslateTypeProperty(uint8_t index) const;
    MessageMode TranslateSpeakMode(uint8_t index) const;
    MessageMode TranslateMessageMode(uint8_t index) const;
    uint8_t TranslateFluidColor(uint8_t color) const;

    VersionBase(const VersionTriplet &triplet);

protected:
    TranslationTable<MessageMode> SpeakModes;
    TranslationTable<MessageMode> MessageModes;
    TranslationTable<TypeProperty> TypeProperties;
};

struct Version : public VersionBase {
    PictureFile Pictures;
    SpriteFile Sprites;
    TypeFile Types;
    trc::Icons Icons;
    trc::Fonts Fonts;

#ifdef DUMP_ITEMS
    void DumpItems();
#endif

public:
    Version(const VersionTriplet &triplet,
            const trc::DataReader &pictureData,
            const trc::DataReader &spriteData,
            const trc::DataReader &typeData);

    const EntityType &GetItem(uint16_t id) const {
        return Types.GetItem(id);
    }

    const EntityType &GetOutfit(uint16_t id) const {
        return Types.GetOutfit(id);
    }

    const EntityType &GetEffect(uint16_t id) const {
        return Types.GetEffect(id);
    }

    const EntityType &GetMissile(uint16_t id) const {
        return Types.GetMissile(id);
    }
};

} // namespace trc

#endif /* __TRC_VERSIONS_HPP__ */
