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

#include "types.h"

#include "datareader.h"
#include "sprites.h"
#include "versions.h"

#include "utils.h"

#ifdef DUMP_ITEMS
#    define types_ReadOptionalU16(Reader, Result)                              \
        datareader_ReadU16(Reader, Result)
#    define types_ReadOptionalString(Reader, MaxLength, Length, Result)        \
        datareader_ReadString(Reader, MaxLength, Length, Result)
#    define types_SetOptionalProperty(Expr) Expr = 1
#else
#    define types_ReadOptionalU16(Reader, Result) datareader_Skip(Reader, 2)
#    define types_ReadOptionalString(Reader, MaxLength, Length, Result)        \
        datareader_SkipString(Reader)
#    define types_SetOptionalProperty(Expr)
#endif

static bool types_ReadTypeProperties(struct trc_version *version,
                                     struct trc_data_reader *reader,
                                     struct trc_entitytype *type) {
    type->Properties.StackPriority = 5;

    for (;;) {
        enum TrcTypeProperty typeProperty;
        uint8_t byte;

        if (!datareader_ReadU8(reader, &byte)) {
            return trc_ReportError("Failed to read type property marker");
        }

        if (!version_TranslateTypeProperty(version, byte, &typeProperty)) {
            return trc_ReportError("Failed to translate type property %x",
                                   byte);
        }

        switch (typeProperty) {
        case TYPEPROPERTY_GROUND:
            type->Properties.StackPriority = 0;

            if (!datareader_ReadU16(reader, &type->Properties.Speed)) {
                return trc_ReportError("Failed to read speed value");
            }

            break;
        case TYPEPROPERTY_CLIP:
            type->Properties.StackPriority = 1;
            break;
        case TYPEPROPERTY_BOTTOM:
            type->Properties.StackPriority = 2;
            break;
        case TYPEPROPERTY_TOP:
            type->Properties.StackPriority = 3;
            break;
        case TYPEPROPERTY_STACKABLE:
            type->Properties.Stackable = true;
            break;
        case TYPEPROPERTY_RUNE:
            type->Properties.Rune = true;
            break;
        case TYPEPROPERTY_LIQUID_CONTAINER:
            type->Properties.LiquidContainer = true;
            break;
        case TYPEPROPERTY_LIQUID_POOL:
            type->Properties.LiquidPool = true;
            break;
        case TYPEPROPERTY_UNLOOKABLE:
            type->Properties.Unlookable = true;
            break;
        case TYPEPROPERTY_HANGABLE:
            type->Properties.Hangable = true;
            break;
        case TYPEPROPERTY_VERTICAL:
            type->Properties.Vertical = true;
            break;
        case TYPEPROPERTY_HORIZONTAL:
            type->Properties.Horizontal = true;
            break;
        case TYPEPROPERTY_DONT_HIDE:
            type->Properties.DontHide = true;
            break;
        case TYPEPROPERTY_DISPLACEMENT:
            if (!datareader_ReadU16(reader, &type->Properties.DisplacementX)) {
                return trc_ReportError("Failed to read DisplacementX");
            }

            if (!datareader_ReadU16(reader, &type->Properties.DisplacementY)) {
                return trc_ReportError("Failed to read DisplacementY");
            }

            break;
        case TYPEPROPERTY_DISPLACEMENT_LEGACY:
            type->Properties.DisplacementX = 8;
            type->Properties.DisplacementY = 8;
            break;
        case TYPEPROPERTY_HEIGHT:
            if (!datareader_ReadU16(reader, &type->Properties.Height)) {
                return trc_ReportError("Failed to read Height");
            }

            break;
        case TYPEPROPERTY_REDRAW_NEARBY_TOP:
            type->Properties.RedrawNearbyTop = true;
            break;
        case TYPEPROPERTY_ANIMATE_IDLE:
            type->Properties.AnimateIdle = true;
            break;

            /* Optional properties follow: these are parsed but not used. */
        case TYPEPROPERTY_CONTAINER:
            types_SetOptionalProperty(type->Properties.Container);

            break;
        case TYPEPROPERTY_AUTOMAP:
            if (!types_ReadOptionalU16(reader,
                                       &type->Properties.AutomapColor)) {
                return trc_ReportError("Failed to read AutomapColor");
            }

            break;
        case TYPEPROPERTY_LENSHELP:
            if (!types_ReadOptionalU16(reader, &type->Properties.LensHelp)) {
                return trc_ReportError("Failed to read LensHelp");
            }

            break;
        case TYPEPROPERTY_WRAPPABLE:
            types_SetOptionalProperty(type->Properties.Wrappable);
            break;
        case TYPEPROPERTY_UNWRAPPABLE:
            types_SetOptionalProperty(type->Properties.Unwrappable);
            break;
        case TYPEPROPERTY_TOP_EFFECT:
            types_SetOptionalProperty(type->Properties.TopEffect);
            break;
        case TYPEPROPERTY_NO_MOVE_ANIMATION:
            types_SetOptionalProperty(type->Properties.NoMoveAnimation);
            break;
        case TYPEPROPERTY_USABLE:
            types_SetOptionalProperty(type->Properties.Usable);
            break;
        case TYPEPROPERTY_CORPSE:
            types_SetOptionalProperty(type->Properties.Corpse);
            break;
        case TYPEPROPERTY_BLOCKING:
            types_SetOptionalProperty(type->Properties.Blocking);
            break;
        case TYPEPROPERTY_UNMOVABLE:
            types_SetOptionalProperty(type->Properties.Unmovable);
            break;
        case TYPEPROPERTY_UNPATHABLE:
            types_SetOptionalProperty(type->Properties.Unpathable);
            break;
        case TYPEPROPERTY_TAKEABLE:
            types_SetOptionalProperty(type->Properties.Takeable);
            break;
        case TYPEPROPERTY_FORCE_USE:
            types_SetOptionalProperty(type->Properties.ForceUse);
            break;
        case TYPEPROPERTY_MULTI_USE:
            types_SetOptionalProperty(type->Properties.MultiUse);
            break;
        case TYPEPROPERTY_TRANSLUCENT:
            types_SetOptionalProperty(type->Properties.Translucent);
            break;
        case TYPEPROPERTY_WALKABLE:
            types_SetOptionalProperty(type->Properties.Walkable);
            break;
        case TYPEPROPERTY_LOOK_THROUGH:
            types_SetOptionalProperty(type->Properties.LookThrough);
            break;
        case TYPEPROPERTY_ROTATE:
            types_SetOptionalProperty(type->Properties.Rotate);
            break;
        case TYPEPROPERTY_WRITE:
            types_SetOptionalProperty(type->Properties.Write);

            if (!types_ReadOptionalU16(reader,
                                       &type->Properties.MaxTextLength)) {
                return trc_ReportError("Failed to read MaxTextLength");
            }

            break;
        case TYPEPROPERTY_WRITE_ONCE:
            types_SetOptionalProperty(type->Properties.WriteOnce);

            if (!types_ReadOptionalU16(reader,
                                       &type->Properties.MaxTextLength)) {
                return trc_ReportError("Failed to read MaxTextLength");
            }

            break;
        case TYPEPROPERTY_LIGHT:
            if (!types_ReadOptionalU16(reader,
                                       &type->Properties.LightIntensity)) {
                return trc_ReportError("Failed to read LightIntensity");
            }

            if (!types_ReadOptionalU16(reader, &type->Properties.LightColor)) {
                return trc_ReportError("Failed to read LightColor");
            }
            break;
        case TYPEPROPERTY_EQUIPMENT_SLOT:
            types_SetOptionalProperty(type->Properties.EquipmentSlotRestricted);

            if (!types_ReadOptionalU16(reader,
                                       &type->Properties.EquipmentSlot)) {
                return trc_ReportError("Failed to read EquipmentSlot");
            }

            break;
        case TYPEPROPERTY_MARKET_ITEM:
            types_SetOptionalProperty(type->Properties.MarketItem);

            if (!types_ReadOptionalU16(reader,
                                       &type->Properties.MarketCategory)) {
                return trc_ReportError("Failed to read MarketCategory");
            }

            if (!types_ReadOptionalU16(reader, &type->Properties.TradeAs)) {
                return trc_ReportError("Failed to read TradeAs");
            }

            if (!types_ReadOptionalU16(reader, &type->Properties.ShowAs)) {
                return trc_ReportError("Failed to read ShowAs");
            }

            if (!types_ReadOptionalString(reader,
                                          sizeof(type->Properties.Name),
                                          &type->Properties.NameLength,
                                          type->Properties.Name)) {
                return trc_ReportError("Failed to read NameLength");
            }

            if (!types_ReadOptionalU16(reader,
                                       &type->Properties.VocationRestriction)) {
                return trc_ReportError("Failed to read VocationRestriction");
            }

            if (!types_ReadOptionalU16(reader,
                                       &type->Properties.LevelRestriction)) {
                return trc_ReportError("Failed to read LevelRestriction");
            }

            break;
        case TYPEPROPERTY_DEFAULT_ACTION:
            if (!types_ReadOptionalU16(reader,
                                       &type->Properties.DefaultAction)) {
                return trc_ReportError("Failed to read DefaultAction");
            }
            break;
        case TYPEPROPERTY_UNKNOWN_U16:
            types_SetOptionalProperty(type->Properties.UnknownU16);

            if (!datareader_Skip(reader, 2)) {
                return trc_ReportError("Failed to skip unknown metadata");
            }

            break;
        case TYPEPROPERTY_ENTRY_END_MARKER:
            return true;
        }
    }
}

static bool types_ReadFrameGroup(struct trc_version *version,
                                 struct trc_data_reader *reader,
                                 struct trc_entitytype *type,
                                 int currentGroup) {
    struct trc_framegroup *frameGroup = &type->FrameGroups[currentGroup];
    frameGroup->SpriteCount = 1;
    frameGroup->Active = 1;

    if (!datareader_ReadU8(reader, &frameGroup->SizeX) ||
        frameGroup->SizeX == 0) {
        return trc_ReportError("Failed to read SizeX");
    }

    frameGroup->SpriteCount *= frameGroup->SizeX;

    if (!datareader_ReadU8(reader, &frameGroup->SizeY) ||
        frameGroup->SizeY == 0) {
        return trc_ReportError("Failed to read SizeY");
    }

    frameGroup->SpriteCount *= frameGroup->SizeY;

    if (frameGroup->SpriteCount > 1) {
        if (!datareader_ReadU8(reader, &frameGroup->RenderSize)) {
            return trc_ReportError("Failed to read RenderSize");
        }
    } else {
        /* Default to 1x1 tiles. */
        frameGroup->RenderSize = 32;
    }

    if (!datareader_ReadU8(reader, &frameGroup->LayerCount) ||
        frameGroup->LayerCount == 0) {
        return trc_ReportError("Failed to read LayerCount");
    }

    frameGroup->SpriteCount *= frameGroup->LayerCount;

    if (!datareader_ReadU8(reader, &frameGroup->XDiv) ||
        frameGroup->XDiv == 0) {
        return trc_ReportError("Failed to read XDiv");
    }

    frameGroup->SpriteCount *= frameGroup->XDiv;

    if (!datareader_ReadU8(reader, &frameGroup->YDiv) ||
        frameGroup->YDiv == 0) {
        return trc_ReportError("Failed to read YDiv");
    }

    frameGroup->SpriteCount *= frameGroup->YDiv;

    if (version->Features.TypeZDiv) {
        if (!datareader_ReadU8(reader, &frameGroup->ZDiv) ||
            frameGroup->ZDiv == 0) {
            return trc_ReportError("Failed to read ZDiv");
        }
    } else {
        frameGroup->ZDiv = 1;
    }

    frameGroup->SpriteCount *= frameGroup->ZDiv;

    if (!datareader_ReadU8(reader, &frameGroup->FrameCount) ||
        frameGroup->FrameCount == 0) {
        return trc_ReportError("Failed to read FrameCount");
    }

    frameGroup->SpriteCount *= frameGroup->FrameCount;
    type->Properties.Animated = (frameGroup->FrameCount > 1);

    if (frameGroup->SpriteCount > UINT16_MAX) {
        return trc_ReportError("Sprite count is unreasonably large");
    }

    if (type->Properties.Animated && version->Features.AnimationPhases) {
        frameGroup->Phases = (struct trc_spritephase *)checked_allocate(
                frameGroup->FrameCount,
                sizeof(struct trc_spritephase));

        if (!datareader_ReadU8(reader, &frameGroup->StartPhase)) {
            return trc_ReportError("Failed to read StartPhase");
        }

        if (!datareader_ReadU32(reader, &frameGroup->LoopCount)) {
            return trc_ReportError("Failed to read LoopCount");
        }

        if (!datareader_ReadU8(reader, &frameGroup->AnimationType)) {
            return trc_ReportError("Failed to read AnimationType");
        }

        for (int i = 0; i < frameGroup->FrameCount; i++) {
            if (!datareader_ReadU32(reader, &frameGroup->Phases[i].Minimum)) {
                return trc_ReportError("Failed to read Phases[i].Minimum");
            }
            if (!datareader_ReadU32(reader, &frameGroup->Phases[i].Maximum)) {
                return trc_ReportError("Failed to read Phases[i].Maximum");
            }
        }
    }

    frameGroup->SpriteIds =
            (uint32_t *)checked_allocate(frameGroup->SpriteCount,
                                         sizeof(uint32_t));

    for (unsigned i = 0; i < frameGroup->SpriteCount; i++) {
        if (version->Sprites.IndexSize == 4) {
            if (!datareader_ReadU32(reader, &frameGroup->SpriteIds[i])) {
                return trc_ReportError("Failed to read SpriteIds[i]");
            }
        } else {
            if (!datareader_ReadU16(reader, &frameGroup->SpriteIds[i])) {
                return trc_ReportError("Failed to read SpriteIds[i]");
            }
        }

        if (frameGroup->SpriteIds[i] >= version->Sprites.Count) {
            return trc_ReportError("Sprite index out of range");
        }
    }

    /* For types that have the same idle and walking frames, Cipsoft simply
     * omits the idle and uses walking.
     *
     * We'll do the same for versions before frame groups are present. */
    if (version->Features.FrameGroups) {
        if ((currentGroup == FRAME_GROUP_WALKING &&
             (type->FrameGroups[FRAME_GROUP_IDLE].Active == 0 ||
              type->FrameGroups[FRAME_GROUP_IDLE].FrameCount == 0))) {
            memcpy(&type->FrameGroups[FRAME_GROUP_IDLE],
                   frameGroup,
                   sizeof(struct trc_framegroup));
        }

        return true;
    }

    memcpy(&type->FrameGroups[FRAME_GROUP_WALKING],
           frameGroup,
           sizeof(struct trc_framegroup));
    return currentGroup == FRAME_GROUP_IDLE;
}

static bool types_ReadType(struct trc_version *version,
                           struct trc_data_reader *reader,
                           int hasFrameGroups,
                           struct trc_entitytype *type) {
    uint8_t frameGroupCount;

    if (!types_ReadTypeProperties(version, reader, type)) {
        return trc_ReportError("Failed to read type properties");
    }

    if (hasFrameGroups) {
        if (!datareader_ReadU8(reader, &frameGroupCount)) {
            return trc_ReportError("Failed to read frameGroupCount");
        }
    } else {
        frameGroupCount = 1;
    }

    type->Properties.Animated = 0;

    while (frameGroupCount--) {
        uint8_t currentGroup;

        if (hasFrameGroups) {
            if (!datareader_ReadU8(reader, &currentGroup)) {
                return trc_ReportError("Failed to read currentGroup");
            }
        } else {
            currentGroup = FRAME_GROUP_DEFAULT;
        }

        if (!CHECK_RANGE(currentGroup, FRAME_GROUP_FIRST, FRAME_GROUP_LAST)) {
            return trc_ReportError("Frame group index is out of range");
        }

        if (!types_ReadFrameGroup(version, reader, type, currentGroup)) {
            return trc_ReportError("Failed to read frame group");
        }
    }

    return true;
}

static bool types_ReadTypeArray(struct trc_version *version,
                                struct trc_data_reader *reader,
                                struct trc_entitytype *typeArray,
                                int hasFrameGroups,
                                int minId,
                                int maxId) {
    for (int typeIdx = minId; typeIdx <= maxId; typeIdx++) {
        if (!types_ReadType(version,
                            reader,
                            hasFrameGroups,
                            &typeArray[typeIdx - minId])) {
            return trc_ReportError("Failed to read type definition");
        }
    }

    return true;
}

static bool types_ReadTypeCategories(struct trc_version *version,
                                     struct trc_type_file *types,
                                     struct trc_data_reader *reader) {
    if (!datareader_ReadU32(reader, &types->Signature)) {
        return trc_ReportError("Failed to read Signature");
    }

    if (!datareader_ReadU16(reader, &types->Items.MaxId)) {
        return trc_ReportError("Failed to read MaxItemId");
    }

    if (!datareader_ReadU16(reader, &types->Outfits.MaxId)) {
        return trc_ReportError("Failed to read MaxOutfitId");
    }

    if (!datareader_ReadU16(reader, &types->Effects.MaxId)) {
        return trc_ReportError("Failed to read MaxEffectId");
    }

    if (!datareader_ReadU16(reader, &types->Missiles.MaxId)) {
        return trc_ReportError("Failed to read MaxMissileId");
    }

    types->Items.Types = (struct trc_entitytype *)checked_allocate(
            types->Items.MaxId - types->Items.MinId + 1,
            sizeof(struct trc_entitytype));
    types->Outfits.Types = (struct trc_entitytype *)checked_allocate(
            types->Outfits.MaxId - types->Outfits.MinId + 1,
            sizeof(struct trc_entitytype));
    types->Effects.Types = (struct trc_entitytype *)checked_allocate(
            types->Effects.MaxId - types->Effects.MinId + 1,
            sizeof(struct trc_entitytype));
    types->Missiles.Types = (struct trc_entitytype *)checked_allocate(
            types->Missiles.MaxId - types->Missiles.MinId + 1,
            sizeof(struct trc_entitytype));

    if (!types_ReadTypeArray(version,
                             reader,
                             types->Items.Types,
                             0,
                             types->Items.MinId,
                             types->Items.MaxId)) {
        return trc_ReportError("Failed to read item type array");
    }

    if (!types_ReadTypeArray(version,
                             reader,
                             types->Outfits.Types,
                             version->Features.FrameGroups,
                             types->Outfits.MinId,
                             types->Outfits.MaxId)) {
        return trc_ReportError("Failed to read outfit type array");
    }

    if (!types_ReadTypeArray(version,
                             reader,
                             types->Effects.Types,
                             0,
                             types->Effects.MinId,
                             types->Effects.MaxId)) {
        return trc_ReportError("Failed to read effect type array");
    }

    if (!types_ReadTypeArray(version,
                             reader,
                             types->Missiles.Types,
                             0,
                             types->Missiles.MinId,
                             types->Missiles.MaxId)) {
        return trc_ReportError("Failed to read missile type array");
    }

    return true;
}

bool types_Load(struct trc_version *version, struct trc_data_reader *data) {
    struct trc_type_file *types = &version->Types;

    types->Items.MinId = 100;
    types->Outfits.MinId = 1;
    types->Effects.MinId = 1;
    types->Missiles.MinId = 1;

    if (!types_ReadTypeCategories(version, types, data)) {
        /* The categories will be freed by version_Free later on.*/
        return trc_ReportError("Failed to read type categories");
    }

    return true;
}

static void types_FreeCategory(struct trc_type_category *category) {
    const int lastEntity = category->MaxId - category->MinId;

    if (lastEntity > 0) {
        for (int i = 0; i <= lastEntity; i++) {
            struct trc_entitytype *entity = &category->Types[i];
            struct trc_framegroup *idle, *walking;

            /* Frame groups may re-use sprite id maps in between them, make sure
             * we don't double-free. */
            _Static_assert((FRAME_GROUP_LAST - FRAME_GROUP_FIRST) == 1,
                           "Only two frame groups are supported at present");
            idle = &entity->FrameGroups[FRAME_GROUP_IDLE];
            walking = &entity->FrameGroups[FRAME_GROUP_WALKING];

            if (idle->SpriteIds != walking->SpriteIds) {
                checked_deallocate(idle->SpriteIds);
            }

            checked_deallocate(walking->SpriteIds);
        }

        checked_deallocate(category->Types);
    }
}

void types_Free(struct trc_version *version) {
    struct trc_type_file *types = &version->Types;

    types_FreeCategory(&types->Items);
    types_FreeCategory(&types->Outfits);
    types_FreeCategory(&types->Effects);
    types_FreeCategory(&types->Missiles);
}

bool types_GetItem(const struct trc_version *version,
                   uint16_t id,
                   struct trc_entitytype **result) {
    const struct trc_type_file *types = &version->Types;

    if (!CHECK_RANGE(id, types->Items.MinId, types->Items.MaxId)) {
        return trc_ReportError("The given id (%X) was out of bounds.", id);
    }

    (*result) = &types->Items.Types[id - types->Items.MinId];

    return true;
}

bool types_GetOutfit(const struct trc_version *version,
                     uint16_t id,
                     struct trc_entitytype **result) {
    const struct trc_type_file *types = &version->Types;

    if (!CHECK_RANGE(id, types->Outfits.MinId, types->Outfits.MaxId)) {
        return trc_ReportError("The given id (%X) was out of bounds.", id);
    }

    (*result) = &types->Outfits.Types[id - types->Outfits.MinId];

    return true;
}

bool types_GetEffect(const struct trc_version *version,
                     uint16_t id,
                     struct trc_entitytype **result) {
    const struct trc_type_file *types = &version->Types;

    if (!CHECK_RANGE(id, types->Effects.MinId, types->Effects.MaxId)) {
        return trc_ReportError("The given id (%X) was out of bounds.", id);
    }

    (*result) = &types->Effects.Types[id - types->Effects.MinId];

    return true;
}

bool types_GetMissile(const struct trc_version *version,
                      uint16_t id,
                      struct trc_entitytype **result) {
    const struct trc_type_file *types = &version->Types;

    if (!CHECK_RANGE(id, types->Missiles.MinId, types->Missiles.MaxId)) {
        return trc_ReportError("The given id (%X) was out of bounds.", id);
    }

    (*result) = &types->Missiles.Types[id - types->Missiles.MinId];

    return true;
}
