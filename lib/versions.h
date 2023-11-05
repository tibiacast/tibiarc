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

#ifndef __TRC_VERSIONS_H__
#define __TRC_VERSIONS_H__

#include <stdbool.h>
#include <stdint.h>

#include "versions_decl.h"

#include "icons.h"
#include "message.h"
#include "parser.h"
#include "pictures.h"
#include "types.h"

struct trc_version {
    int Major;
    int Minor;
    int Preview;

    uint32_t PictureChecksum;
    uint32_t SpriteChecksum;
    uint32_t DataChecksum;

    struct trc_spritefile Sprites;
    struct trc_picture_file Pictures;
    struct trc_type_file Types;
    struct trc_icons Icons;
    struct trc_fonts Fonts;

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
        bool ExperienceBonus : 1;
        bool ExperienceU64 : 1;
        bool ExpertMode : 1;
        bool ExtendedDeathDialog : 1;
        bool GuildChannelId : 1;
        bool HazyNewTileStuff : 1;
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
        bool OfflineStamina : 1;
        bool OutfitAddons : 1;
        bool OutfitCountU16 : 1;
        bool OutfitNames : 1;
        bool OutfitsU16 : 1;
        bool PartyChannelId : 1;
        bool PassableCreatures : 1;
        bool PassableCreatureUpdate : 1;
        bool PlayerHunger : 1;
        bool PlayerMoneyU64 : 1;
        bool PlayerSpeed : 1;
        bool PremiumUntil : 1;
        bool PreviewByte : 1;
        bool PVPFraming : 1;
        bool RawEffectIds : 1;
        bool ReportMessages : 1;
        bool RuneChargeCount : 1;
        bool ShieldIcon : 1;
        bool SinglePVPHelper : 1;
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
        bool AnimationPhases : 1;
        bool FrameGroups : 1;
        bool IconBar : 1;
        bool ModernStacking : 1;
        bool SpriteIndexU32 : 1;
        bool TypeZDiv : 1;
    } Features;
};

#define VERSION_AT_LEAST(Version, A, B)                                        \
    ((Version)->Major > (A) ||                                                 \
     ((Version)->Major == (A) && (Version)->Minor >= (B)))

bool version_Load(int major,
                  int minor,
                  int preview,
                  struct trc_data_reader *pictureData,
                  struct trc_data_reader *spriteData,
                  struct trc_data_reader *typeData,
                  struct trc_version **out);
void version_Free(struct trc_version *version);

void version_IconTable(const struct trc_version *version,
                       int *size,
                       const struct trc_icon_table_entry **table);

bool version_TranslateTypeProperty(const struct trc_version *version,
                                   uint8_t index,
                                   enum TrcTypeProperty *out);

bool version_TranslateSpeakMode(const struct trc_version *version,
                                uint8_t index,
                                enum TrcMessageMode *out);

bool version_TranslateMessageMode(const struct trc_version *version,
                                  uint8_t index,
                                  enum TrcMessageMode *out);

bool version_TranslatePictureIndex(const struct trc_version *version,
                                   enum TrcPictureIndex index,
                                   int *out);

bool version_TranslateFluidColor(const struct trc_version *version,
                                 uint8_t color,
                                 int *out);

#endif /* __TRC_VERSIONS_H__ */
