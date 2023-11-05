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

#include "parser.h"

#include "datareader.h"
#include "types.h"
#include "versions.h"

#include <math.h>

#include "utils.h"

#define ParseAssert(Expr)                                                      \
    if (!(Expr)) {                                                             \
        return trc_ReportError(#Expr);                                         \
    }

static bool parser_ParsePosition(struct trc_data_reader *reader,
                                 struct trc_position *position) {
    ParseAssert(datareader_ReadU16(reader, &position->X));
    ParseAssert(datareader_ReadU16(reader, &position->Y));
    ParseAssert(datareader_ReadU8(reader, &position->Z));
    ParseAssert(CHECK_RANGE(position->Z, 0, 15));

    return true;
}

static bool parser_ParseOutfit(struct trc_data_reader *reader,
                               struct trc_game_state *gamestate,
                               struct trc_outfit *outfit) {
    if ((gamestate->Version)->Protocol.OutfitsU16) {
        ParseAssert(datareader_ReadU16(reader, &outfit->Id));
    } else {
        ParseAssert(datareader_ReadU8(reader, &outfit->Id));
    }

    if (outfit->Id == 0) {
        /* Extra information like stack count or fluid color is omitted when
         * items are used as outfits, so we shouldn't use `parser_ParseObject`
         * here. */
        ParseAssert(datareader_ReadU16(reader, &outfit->Item.Id));
        outfit->Item.ExtraByte = 0;

        ParseAssert(outfit->Item.Id == 0 || outfit->Item.Id >= 100);
    } else {
        ParseAssert(datareader_ReadU8(reader, &outfit->HeadColor));
        ParseAssert(datareader_ReadU8(reader, &outfit->PrimaryColor));
        ParseAssert(datareader_ReadU8(reader, &outfit->SecondaryColor));
        ParseAssert(datareader_ReadU8(reader, &outfit->DetailColor));

        if ((gamestate->Version)->Protocol.OutfitAddons) {
            ParseAssert(datareader_ReadU8(reader, &outfit->Addons));
        }
    }

    if ((gamestate->Version)->Protocol.Mounts) {
        ParseAssert(datareader_ReadU16(reader, &outfit->MountId));
    }

    return true;
}

static bool parser_ParseCreature(struct trc_data_reader *reader,
                                 struct trc_game_state *gamestate,
                                 uint16_t objectId,
                                 uint32_t *creatureId) {
    struct trc_creature *creature;
    uint32_t removeId;

    if (objectId == 0x61) {
        ParseAssert(datareader_ReadU32(reader, &removeId));
    }

    ParseAssert(datareader_ReadU32(reader, creatureId));

    if (objectId == 0x61) {
        creaturelist_ReplaceCreature(&gamestate->CreatureList,
                                     *creatureId,
                                     removeId,
                                     &creature);

        if ((gamestate->Version)->Protocol.CreatureMarks) {
            uint8_t creatureType;
            ParseAssert(datareader_ReadU8(reader, &creatureType));
            ParseAssert(CHECK_RANGE(creatureType,
                                    CREATURE_TYPE_FIRST,
                                    CREATURE_TYPE_LAST));
            creature->Type = (enum TrcCreatureType)creatureType;
        }

        ParseAssert(datareader_ReadString(reader,
                                          sizeof(creature->Name),
                                          &creature->NameLength,
                                          creature->Name));
    } else {
        ParseAssert(creaturelist_GetCreature(&gamestate->CreatureList,
                                             *creatureId,
                                             &creature));
    }

    if (objectId != 0x63) {
        ParseAssert(datareader_ReadU8(reader, &creature->Health));
        ParseAssert(CHECK_RANGE(creature->Health, 0, 100));
    }

    uint8_t direction;
    ParseAssert(datareader_ReadU8(reader, &direction));
    ParseAssert(CHECK_RANGE(direction,
                            CREATURE_DIRECTION_FIRST,
                            CREATURE_DIRECTION_LAST));
    creature->Direction = (enum TrcCreatureDirection)direction;

    if (objectId != 0x63) {
        ParseAssert(parser_ParseOutfit(reader, gamestate, &creature->Outfit));

        ParseAssert(datareader_ReadU8(reader, &creature->LightIntensity));
        ParseAssert(datareader_ReadU8(reader, &creature->LightColor));
        ParseAssert(datareader_ReadU16(reader, &creature->Speed));

        if ((gamestate->Version)->Protocol.SkullIcon) {
            uint8_t skull;
            ParseAssert(datareader_ReadU8(reader, &skull));
            ParseAssert(CHECK_RANGE(skull,
                                    CHARACTER_SKULL_FIRST,
                                    CHARACTER_SKULL_LAST));
            creature->Skull = (enum TrcCharacterSkull)skull;
        }

        if ((gamestate->Version)->Protocol.ShieldIcon) {
            uint8_t shield;
            ParseAssert(datareader_ReadU8(reader, &shield));
            ParseAssert(
                    CHECK_RANGE(shield, PARTY_SHIELD_FIRST, PARTY_SHIELD_LAST));
            creature->Shield = (enum TrcPartyShield)shield;
        }

        if (objectId == 0x61 && (gamestate->Version)->Protocol.WarIcon) {
            uint8_t warIcon;
            ParseAssert(datareader_ReadU8(reader, &warIcon));
            ParseAssert(CHECK_RANGE(warIcon, WAR_ICON_FIRST, WAR_ICON_LAST));
            creature->WarIcon = (enum TrcWarIcon)warIcon;
        }

        if ((gamestate->Version)->Protocol.CreatureMarks) {
            uint8_t creatureType;
            ParseAssert(datareader_ReadU8(reader, &creatureType));
            ParseAssert(CHECK_RANGE(creatureType,
                                    CREATURE_TYPE_FIRST,
                                    CREATURE_TYPE_LAST));
            creature->Type = (enum TrcCreatureType)creatureType;

            if ((gamestate->Version)->Protocol.NPCCategory) {
                uint8_t npcCategory;
                ParseAssert(datareader_ReadU8(reader, &npcCategory));
                ParseAssert(CHECK_RANGE(creatureType,
                                        NPC_CATEGORY_FIRST,
                                        NPC_CATEGORY_LAST));
                creature->NPCCategory = (enum TrcNPCCategory)npcCategory;
            }

            ParseAssert(datareader_ReadU8(reader, &creature->Mark));
            ParseAssert(
                    datareader_ReadU16(reader, &creature->GuildMembersOnline));

            creature->MarkIsPermanent = 1;
        }
    }

    if ((gamestate->Version)->Protocol.PassableCreatures) {
        if (objectId != 0x63 ||
            (gamestate->Version)->Protocol.PassableCreatureUpdate) {
            ParseAssert(datareader_ReadU8(reader, &creature->Impassable));
        }
    }

    return true;
}

static bool parser_ParseObject(struct trc_data_reader *reader,
                               struct trc_game_state *gamestate,
                               struct trc_object *object) {
    uint16_t objectId;

    ParseAssert(datareader_ReadU16(reader, &objectId));

    switch (objectId) {
    case 0:
        /* Null object; behavior added in 9.83 */
        break;
    case 0x61:
    case 0x62:
    case 0x63:
        if (!parser_ParseCreature(reader,
                                  gamestate,
                                  objectId,
                                  &object->CreatureId)) {
            return trc_ReportError("Failed to parse creature");
        }

        objectId = TILE_OBJECT_CREATURE;
        break;
    default: {
        struct trc_entitytype *itemType;

        ParseAssert(objectId >= 100);
        ParseAssert(types_GetItem(gamestate->Version, objectId, &itemType));

        if ((gamestate->Version)->Protocol.ItemMarks) {
            ParseAssert(datareader_ReadU8(reader, &object->Mark));
        } else {
            object->Mark = 255;
        }

        if ((itemType->Properties.LiquidContainer ||
             itemType->Properties.LiquidPool ||
             itemType->Properties.Stackable)) {
            ParseAssert(datareader_ReadU8(reader, &object->ExtraByte));
        } else if (itemType->Properties.Rune) {
            if ((gamestate->Version)->Protocol.RuneChargeCount) {
                ParseAssert(datareader_ReadU8(reader, &object->ExtraByte));
            }
        }

        if (itemType->Properties.Animated &&
            (gamestate->Version)->Protocol.ItemAnimation) {
            ParseAssert(datareader_ReadU8(reader, &object->Animation));
        } else {
            object->Animation = 0;
        }
    }
    }

    object->Id = objectId;

    return true;
}

static bool parser_ParseTileDescription(struct trc_data_reader *reader,
                                        struct trc_game_state *gamestate,
                                        struct trc_tile *tile) {
    uint16_t peekValue;

    tile_Clear(tile);

    ParseAssert(datareader_PeekU16(reader, &peekValue));

    if ((gamestate->Version)->Protocol.HazyNewTileStuff) {
        if (peekValue < 0xFF00) {
            ParseAssert(datareader_ReadU16(reader, &peekValue));
        }
    }

    while (peekValue < 0xFF00) {
        if (tile->ObjectCount < MAX_OBJECTS_PER_TILE) {
            ParseAssert(parser_ParseObject(reader,
                                           gamestate,
                                           &tile->Objects[tile->ObjectCount]));

            tile->ObjectCount++;
        } else {
            struct trc_object nullObject;

            /* We don't particularly care about the result here -- we just need
             * to parse it to catch all relevant creatures */
            ParseAssert(parser_ParseObject(reader, gamestate, &nullObject));
        }

        ParseAssert(datareader_PeekU16(reader, &peekValue));
    }

    return true;
}

static bool parser_ParseFloorDescription(struct trc_data_reader *reader,
                                         struct trc_game_state *gamestate,
                                         int X,
                                         int Y,
                                         int Z,
                                         int width,
                                         int height,
                                         int offset,
                                         uint16_t *tileSkip) {
    for (int xIdx = X + offset; xIdx <= (X + offset + width - 1); xIdx++) {
        for (int yIdx = Y + offset; yIdx <= (Y + offset + height - 1); yIdx++) {
            struct trc_tile *tile = map_GetTile(&gamestate->Map, xIdx, yIdx, Z);

            if ((*tileSkip) == 0) {
                ParseAssert(
                        parser_ParseTileDescription(reader, gamestate, tile));

                ParseAssert(datareader_ReadU16(reader, tileSkip));
                (*tileSkip) = (*tileSkip) & 0xFF;
            } else {
                tile_Clear(tile);

                (*tileSkip) = (*tileSkip) - 1;
            }
        }
    }

    return true;
}

static bool parser_ParseMapDescription(struct trc_data_reader *reader,
                                       struct trc_game_state *gamestate,
                                       int xOffset,
                                       int yOffset,
                                       int width,
                                       int height) {
    int zIdx, endZ, zStep;
    uint16_t tileSkip;

    if (gamestate->Map.Position.Z > 7) {
        zIdx = gamestate->Map.Position.Z - 2;
        endZ = MIN(15, gamestate->Map.Position.Z + 2);
        zStep = 1;
    } else {
        zIdx = 7;
        endZ = 0;
        zStep = -1;
    }

    tileSkip = 0;

    for (; zIdx != (endZ + zStep); zIdx += zStep) {
        ParseAssert(parser_ParseFloorDescription(
                reader,
                gamestate,
                gamestate->Map.Position.X + xOffset,
                gamestate->Map.Position.Y + yOffset,
                zIdx,
                width,
                height,
                gamestate->Map.Position.Z - zIdx,
                &tileSkip));
    }

    ParseAssert(tileSkip == 0);

    return true;
}

static bool parser_ParseTileUpdate(struct trc_data_reader *reader,
                                   struct trc_game_state *gamestate) {
    struct trc_position tilePosition;
    struct trc_tile *currentTile;
    uint16_t tileSkip;

    ParseAssert(parser_ParsePosition(reader, &tilePosition));

    currentTile = map_GetTile(&gamestate->Map,
                              tilePosition.X,
                              tilePosition.Y,
                              tilePosition.Z);

    ParseAssert(parser_ParseTileDescription(reader, gamestate, currentTile));

    /* Doesn't do anything, we still need to skip it however. */
    ParseAssert(datareader_ReadU16(reader, &tileSkip));

    return true;
}

static bool parser_ParseTileAddObject(struct trc_data_reader *reader,
                                      struct trc_game_state *gamestate) {
    struct trc_position tilePosition;
    struct trc_object parsedObject;
    uint8_t stackPosition;
    struct trc_tile *currentTile;

    ParseAssert(parser_ParsePosition(reader, &tilePosition));

    currentTile = map_GetTile(&gamestate->Map,
                              tilePosition.X,
                              tilePosition.Y,
                              tilePosition.Z);

    if ((gamestate->Version)->Protocol.AddObjectStackPosition) {
        ParseAssert(datareader_ReadU8(reader, &stackPosition));
    } else {
        stackPosition = TILE_STACKPOSITION_TOP;
    }

    ParseAssert(parser_ParseObject(reader, gamestate, &parsedObject));
    ParseAssert(tile_InsertObject(gamestate->Version,
                                  currentTile,
                                  &parsedObject,
                                  stackPosition));

    return true;
}

static bool parser_ParseTileSetObject(struct trc_data_reader *reader,
                                      struct trc_game_state *gamestate) {
    struct trc_object parsedObject;
    uint16_t peekValue;

    ParseAssert(datareader_PeekU16(reader, &peekValue));

    if (peekValue != 0xFFFF) {
        struct trc_position tilePosition;
        uint8_t stackPosition;
        struct trc_tile *currentTile;

        ParseAssert(parser_ParsePosition(reader, &tilePosition));
        ParseAssert(datareader_ReadU8(reader, &stackPosition));

        currentTile = map_GetTile(&gamestate->Map,
                                  tilePosition.X,
                                  tilePosition.Y,
                                  tilePosition.Z);

        ParseAssert(parser_ParseObject(reader, gamestate, &parsedObject));
        ParseAssert(stackPosition < MAX_OBJECTS_PER_TILE);
        ParseAssert(tile_SetObject(gamestate->Version,
                                   currentTile,
                                   &parsedObject,
                                   stackPosition));
    } else {
        uint32_t creatureId;

        /* Skip the 0xFFFF marker */
        ParseAssert(datareader_ReadU16(reader, &peekValue));

        ParseAssert(datareader_ReadU32(reader, &creatureId));
        ParseAssert(parser_ParseObject(reader, gamestate, &parsedObject));
    }

    return true;
}

static bool parser_ParseTileRemoveObject(struct trc_data_reader *reader,
                                         struct trc_game_state *gamestate) {
    uint16_t peekValue;

    ParseAssert(datareader_PeekU16(reader, &peekValue));

    if (peekValue != 0xFFFF) {
        struct trc_position tilePosition;
        uint8_t stackPosition;
        struct trc_tile *currentTile;

        ParseAssert(parser_ParsePosition(reader, &tilePosition));
        ParseAssert(datareader_ReadU8(reader, &stackPosition));

        currentTile = map_GetTile(&gamestate->Map,
                                  tilePosition.X,
                                  tilePosition.Y,
                                  tilePosition.Z);

        ParseAssert(stackPosition < MAX_OBJECTS_PER_TILE);
        ParseAssert(tile_RemoveObject(gamestate->Version,
                                      currentTile,
                                      stackPosition));
    } else {
        uint32_t creatureId;

        /* Skip the 0xFFFF marker */
        ParseAssert(datareader_ReadU16(reader, &peekValue));
        ParseAssert(datareader_ReadU32(reader, &creatureId));
    }

    return true;
}

static bool parser_ParseTileMoveCreature(struct trc_data_reader *reader,
                                         struct trc_game_state *gamestate) {
    struct trc_position tilePosition;
    struct trc_creature *creature;
    uint16_t peekValue;
    uint32_t creatureId;

    ParseAssert(datareader_PeekU16(reader, &peekValue));

    if (peekValue != 0xFFFF) {
        uint8_t stackPosition;
        struct trc_object tileObject;
        struct trc_tile *currentTile;

        ParseAssert(parser_ParsePosition(reader, &tilePosition));
        ParseAssert(datareader_ReadU8(reader, &stackPosition));

        currentTile = map_GetTile(&gamestate->Map,
                                  tilePosition.X,
                                  tilePosition.Y,
                                  tilePosition.Z);

        ParseAssert(tile_CopyObject(gamestate->Version,
                                    currentTile,
                                    &tileObject,
                                    stackPosition));
        ParseAssert(tileObject.Id == TILE_OBJECT_CREATURE);
        ParseAssert(tile_RemoveObject(gamestate->Version,
                                      currentTile,
                                      stackPosition));

        creatureId = tileObject.CreatureId;
    } else {
        /* Skip the 0xFFFF marker */
        ParseAssert(datareader_ReadU16(reader, &peekValue));
        ParseAssert(datareader_ReadU32(reader, &creatureId));

        tilePosition.X = 0xFFFF;
        tilePosition.Y = 0xFFFF;
        tilePosition.Z = 0xFF;
    }

    ParseAssert(creaturelist_GetCreature(&gamestate->CreatureList,
                                         creatureId,
                                         &creature));
    {
        int xDifference, yDifference, zDifference;
        struct trc_object tileObject;
        struct trc_tile *currentTile;

        creature->MovementInformation.Origin = tilePosition;

        ParseAssert(parser_ParsePosition(reader, &tilePosition));

        creature->MovementInformation.Target = tilePosition;

        currentTile = map_GetTile(&gamestate->Map,
                                  tilePosition.X,
                                  tilePosition.Y,
                                  tilePosition.Z);

        xDifference = creature->MovementInformation.Target.X -
                      creature->MovementInformation.Origin.X;
        yDifference = creature->MovementInformation.Target.Y -
                      creature->MovementInformation.Origin.Y;
        zDifference = creature->MovementInformation.Target.Z -
                      creature->MovementInformation.Origin.Z;

        if (yDifference < 0) {
            creature->Direction = CREATURE_DIRECTION_NORTH;
        } else if (yDifference > 0) {
            creature->Direction = CREATURE_DIRECTION_SOUTH;
        }

        if (xDifference < 0) {
            creature->Direction = CREATURE_DIRECTION_WEST;
        } else if (xDifference > 0) {
            creature->Direction = CREATURE_DIRECTION_EAST;
        }

        if (zDifference == 0 &&
            (ABS(xDifference) <= 1 && ABS(yDifference) <= 1)) {
            struct trc_entitytype *groundType;
            uint32_t movementSpeed;

            ParseAssert(tile_CopyObject(gamestate->Version,
                                        currentTile,
                                        &tileObject,
                                        0));
            ParseAssert(types_GetItem(gamestate->Version,
                                      tileObject.Id,
                                      &groundType));
            ParseAssert(groundType->Properties.StackPriority == 0);

            if ((gamestate->Version)->Protocol.SpeedAdjustment) {
                if ((double)creature->Speed >= -gamestate->SpeedB) {
                    movementSpeed =
                            MAX(1,
                                (uint32_t)(gamestate->SpeedA *
                                                   log((double)creature->Speed +
                                                       gamestate->SpeedB) +
                                           gamestate->SpeedC));
                } else {
                    movementSpeed = 1;
                }
            } else {
                movementSpeed = MAX(1, creature->Speed);
            }

            creature->MovementInformation.WalkEndTick =
                    gamestate->CurrentTick +
                    (groundType->Properties.Speed * 1000) / movementSpeed;
            creature->MovementInformation.WalkStartTick =
                    gamestate->CurrentTick;
        } else {
            /* Moves between floors, as well as teleportations, are instant. */
            creature->MovementInformation.WalkStartTick = 0;
            creature->MovementInformation.WalkEndTick = 0;
        }

        tileObject.Id = TILE_OBJECT_CREATURE;
        tileObject.CreatureId = creatureId;

        ParseAssert(tile_InsertObject(gamestate->Version,
                                      currentTile,
                                      &tileObject,
                                      TILE_STACKPOSITION_TOP));
    }

    return true;
}

static bool parser_ParseFullMapDescription(struct trc_data_reader *reader,
                                           struct trc_game_state *gamestate) {
    ParseAssert(parser_ParsePosition(reader, &gamestate->Map.Position));

    return parser_ParseMapDescription(reader,
                                      gamestate,
                                      -8,
                                      -6,
                                      TILE_BUFFER_WIDTH,
                                      TILE_BUFFER_HEIGHT);
}

static bool parser_ParseInitialization(struct trc_data_reader *reader,
                                       struct trc_game_state *gamestate) {
    ParseAssert(datareader_ReadU32(reader, &gamestate->Player.Id));

    ParseAssert(datareader_ReadU16(reader, &gamestate->Player.BeatDuration));

    if ((gamestate->Version)->Protocol.SpeedAdjustment) {
        ParseAssert(datareader_ReadFloat(reader, &gamestate->SpeedA));
        ParseAssert(datareader_ReadFloat(reader, &gamestate->SpeedB));
        ParseAssert(datareader_ReadFloat(reader, &gamestate->SpeedC));
    }

    if ((gamestate->Version)->Protocol.BugReporting) {
        ParseAssert(
                datareader_ReadU8(reader, &gamestate->Player.AllowBugReports));
    }

    if ((gamestate->Version)->Protocol.PVPFraming) {
        uint8_t PvPFraming;
        ParseAssert(datareader_ReadU8(reader, &PvPFraming));
    }

    if ((gamestate->Version)->Protocol.ExpertMode) {
        uint8_t ExpertMode;
        ParseAssert(datareader_ReadU8(reader, &ExpertMode));
    }

    /* This is Tibiacast-specific, for versions where it accidentally
     * generated buggy initialization packets. */
    if ((gamestate->Version)->Protocol.TibiacastBuggedInitialization) {
        uint8_t Erroneous;
        ParseAssert(datareader_ReadU8(reader, &Erroneous));
    }

    return true;
}

static bool parser_ParseGMActions(struct trc_data_reader *reader,
                                  struct trc_game_state *gamestate) {
    unsigned skipCount;

    if (VERSION_AT_LEAST(gamestate->Version, 8, 50)) {
        skipCount = 19;
    } else if (VERSION_AT_LEAST(gamestate->Version, 8, 41)) {
        skipCount = 22;
    } else if (VERSION_AT_LEAST(gamestate->Version, 8, 30)) {
        /* Wild guess based on YATC code. */
        skipCount = 28;
    } else if (VERSION_AT_LEAST(gamestate->Version, 7, 40)) {
        skipCount = 32;
    } else {
        /* Actual value seen in 7.30 recording, may need further tweaks. */
        skipCount = 30;
    }

    ParseAssert(datareader_Skip(reader, skipCount));

    return true;
}

static bool parser_ParseContainerOpen(struct trc_data_reader *reader,
                                      struct trc_game_state *gamestate) {
    struct trc_container *container;
    uint8_t containerId;

    struct trc_entitytype *itemType;
    int itemIdx;

    ParseAssert(datareader_ReadU8(reader, &containerId));
    containerlist_OpenContainer(&gamestate->ContainerList,
                                containerId,
                                &container);

    ParseAssert(datareader_ReadU16(reader, &container->ItemId));
    ParseAssert(
            types_GetItem(gamestate->Version, container->ItemId, &itemType));

    if ((gamestate->Version)->Protocol.ItemMarks) {
        ParseAssert(datareader_ReadU8(reader, &container->Mark));
    } else {
        container->Mark = 255;
    }

    if (itemType->Properties.Animated) {
        ParseAssert(datareader_ReadU8(reader, &container->Animation));
    }

    ParseAssert(datareader_ReadString(reader,
                                      sizeof(container->Name),
                                      &container->NameLength,
                                      container->Name));
    ParseAssert(datareader_ReadU8(reader, &container->SlotsPerPage));
    ParseAssert(datareader_ReadU8(reader, &container->HasParent));

    if ((gamestate->Version)->Protocol.ContainerPagination) {
        ParseAssert(datareader_ReadU8(reader, &container->DragAndDrop));
        ParseAssert(datareader_ReadU8(reader, &container->Pagination));
        ParseAssert(datareader_ReadU16(reader, &container->TotalObjects));
        ParseAssert(datareader_ReadU16(reader, &container->StartIndex));
    }

    ParseAssert(datareader_ReadU8(reader, &container->ItemCount));
    ParseAssert(container->ItemCount <
                (sizeof(container->Items) / sizeof(struct trc_object)));

    if (!(gamestate->Version)->Protocol.ContainerPagination) {
        container->TotalObjects =
                MIN(container->TotalObjects, container->ItemCount);
    }

    if (container->ItemCount > container->TotalObjects) {
        container->TotalObjects = container->ItemCount;
    }

    for (itemIdx = 0;
         itemIdx < MIN(container->ItemCount, container->SlotsPerPage);
         itemIdx++) {
        ParseAssert(parser_ParseObject(reader,
                                       gamestate,
                                       &container->Items[itemIdx]));
    }

    for (; itemIdx < container->ItemCount; itemIdx++) {
        struct trc_object nullObject;
        ParseAssert(parser_ParseObject(reader, gamestate, &nullObject));
    }

    return true;
}

static bool parser_ParseContainerClose(struct trc_data_reader *reader,
                                       struct trc_game_state *gamestate) {
    uint8_t containerId;

    ParseAssert(datareader_ReadU8(reader, &containerId));
    containerlist_CloseContainer(&gamestate->ContainerList, containerId);

    return true;
}

static bool parser_ParseContainerAddItem(struct trc_data_reader *reader,
                                         struct trc_game_state *gamestate) {
    struct trc_container *container;
    struct trc_object item;

    uint16_t containerIndex;
    uint8_t containerId;

    containerIndex = 0;

    ParseAssert(datareader_ReadU8(reader, &containerId));

    if ((gamestate->Version)->Protocol.ContainerIndexU16) {
        /* Note that container index is only present at all in versions that
         * have 16-wide indexes. */
        ParseAssert(datareader_ReadU16(reader, &containerIndex));
    }

    ParseAssert(parser_ParseObject(reader, gamestate, &item));

    if (containerlist_GetContainer(&gamestate->ContainerList,
                                   containerId,
                                   &container)) {
        ParseAssert(
                container->ItemCount <
                ((sizeof(container->Items) / sizeof(struct trc_object)) - 1));

        if (containerIndex >= container->StartIndex) {
            int insertionIndex = (containerIndex - container->StartIndex);

            for (int itemIdx = container->SlotsPerPage - 2;
                 itemIdx >= (insertionIndex);
                 itemIdx--) {
                container->Items[insertionIndex + itemIdx + 1] =
                        container->Items[insertionIndex + itemIdx];
            }

            container->Items[insertionIndex] = item;
            container->ItemCount++;
        }

        container->TotalObjects += 1;
    }

    return true;
}

static bool parser_ParseContainerTransformItem(
        struct trc_data_reader *reader,
        struct trc_game_state *gamestate) {
    struct trc_container *container;
    struct trc_object item;

    uint16_t containerIndex;
    uint8_t containerId;

    ParseAssert(datareader_ReadU8(reader, &containerId));

    if ((gamestate->Version)->Protocol.ContainerIndexU16) {
        ParseAssert(datareader_ReadU16(reader, &containerIndex));
    } else {
        ParseAssert(datareader_ReadU8(reader, &containerIndex));
    }

    ParseAssert(parser_ParseObject(reader, gamestate, &item));

    if (containerlist_GetContainer(&gamestate->ContainerList,
                                   containerId,
                                   &container)) {
        int transformIndex = containerIndex - container->StartIndex;
        if (transformIndex < container->ItemCount) {
            container->Items[transformIndex] = item;
        }
    }

    return true;
}

static bool parser_ParseContainerRemoveItem(struct trc_data_reader *reader,
                                            struct trc_game_state *gamestate) {
    struct trc_container *container;
    struct trc_object item;

    uint16_t containerIndex;
    uint8_t containerId;

    ParseAssert(datareader_ReadU8(reader, &containerId));

    if ((gamestate->Version)->Protocol.ContainerIndexU16) {
        ParseAssert(datareader_ReadU16(reader, &containerIndex));
        ParseAssert(parser_ParseObject(reader, gamestate, &item));
    } else {
        ParseAssert(datareader_ReadU8(reader, &containerIndex));
        item.Id = 0;
    }

    if (containerlist_GetContainer(&gamestate->ContainerList,
                                   containerId,
                                   &container)) {
        if (container->ItemCount > (containerIndex - container->StartIndex)) {
            for (int itemIdx = containerIndex - container->StartIndex;
                 itemIdx <
                 (MIN(container->SlotsPerPage, container->ItemCount) - 1);
                 itemIdx++) {
                container->Items[itemIdx] = container->Items[itemIdx + 1];
            }

            if (item.Id == 0) {
                container->ItemCount--;
            }
        }

        /* item.Id is only != 0 when the container is full. */
        if (item.Id != 0) {
            container->Items[container->SlotsPerPage - 1] = item;
        }

        container->TotalObjects--;
    }

    /* Referencing a nonexistent container or poking at the wrong index is not
     * a protocol violation. */
    return true;
}

static bool parser_ParseInventorySetSlot(struct trc_data_reader *reader,
                                         struct trc_game_state *gamestate) {
    struct trc_object *inventoryObject;
    uint8_t slotId;

    ParseAssert(datareader_ReadU8(reader, &slotId));
    ParseAssert(slotId > 0);
    slotId--;

    ParseAssert(CHECK_RANGE(slotId, INVENTORY_SLOT_FIRST, INVENTORY_SLOT_LAST));
    inventoryObject = player_GetInventoryObject(&gamestate->Player,
                                                (enum TrcInventorySlot)slotId);
    ParseAssert(parser_ParseObject(reader, gamestate, inventoryObject));

    return true;
}

static bool parser_ParseInventoryClearSlot(struct trc_data_reader *reader,
                                           struct trc_game_state *gamestate) {
    struct trc_object inventoryObject = {.Id = 0};
    uint8_t slotId;

    ParseAssert(datareader_ReadU8(reader, &slotId));
    ParseAssert(slotId > 0);
    slotId--;

    ParseAssert(CHECK_RANGE(slotId, INVENTORY_SLOT_FIRST, INVENTORY_SLOT_LAST));
    player_SetInventoryObject(&gamestate->Player,
                              (enum TrcInventorySlot)slotId,
                              &inventoryObject);

    return true;
}

static bool parser_ParseNPCVendorBegin(struct trc_data_reader *reader,
                                       struct trc_game_state *gamestate) {
    uint16_t itemCount;

    if ((gamestate->Version)->Protocol.NPCVendorName) {
        ParseAssert(datareader_SkipString(reader));
    }

    if ((gamestate->Version)->Protocol.NPCVendorItemCountU16) {
        ParseAssert(datareader_ReadU16(reader, &itemCount));
    } else {
        ParseAssert(datareader_ReadU8(reader, &itemCount));
    }

    while (itemCount--) {
        uint32_t sellPrice, buyPrice, weight;
        uint8_t extraByte;
        uint16_t itemId;

        ParseAssert(datareader_ReadU16(reader, &itemId));
        ParseAssert(datareader_ReadU8(reader, &extraByte));
        ParseAssert(datareader_SkipString(reader));

        if ((gamestate->Version)->Protocol.NPCVendorWeight) {
            ParseAssert(datareader_ReadU32(reader, &weight));
        }

        ParseAssert(datareader_ReadU32(reader, &buyPrice));
        ParseAssert(datareader_ReadU32(reader, &sellPrice));
    }

    return true;
}

static bool parser_ParseNPCVendorPlayerGoods(struct trc_data_reader *reader,
                                             struct trc_game_state *gamestate) {
    uint64_t playerMoney;
    uint8_t itemCount;

    if ((gamestate->Version)->Protocol.PlayerMoneyU64) {
        ParseAssert(datareader_ReadU64(reader, &playerMoney));
    } else {
        ParseAssert(datareader_ReadU32(reader, &playerMoney));
    }

    ParseAssert(datareader_ReadU8(reader, &itemCount));

    while (itemCount--) {
        uint8_t extraByte;
        uint16_t itemId;

        ParseAssert(datareader_ReadU16(reader, &itemId));
        ParseAssert(datareader_ReadU8(reader, &extraByte));
    }

    return true;
}

static bool parser_ParsePlayerTradeItems(struct trc_data_reader *reader,
                                         struct trc_game_state *gamestate) {
    uint8_t itemCount;

    ParseAssert(datareader_SkipString(reader));
    ParseAssert(datareader_ReadU8(reader, &itemCount));

    while (itemCount--) {
        struct trc_object nullObject;
        ParseAssert(parser_ParseObject(reader, gamestate, &nullObject));
    }

    return true;
}

static bool parser_ParseAmbientLight(struct trc_data_reader *reader,
                                     struct trc_game_state *gamestate) {
    ParseAssert(datareader_ReadU8(reader, &gamestate->Map.LightIntensity));
    ParseAssert(datareader_ReadU8(reader, &gamestate->Map.LightColor));

    return true;
}

static bool parser_ParseGraphicalEffect(struct trc_data_reader *reader,
                                        struct trc_game_state *gamestate) {
    struct trc_position tilePosition;
    struct trc_tile *currentTile;
    uint8_t effectId;

    ParseAssert(parser_ParsePosition(reader, &tilePosition));
    ParseAssert(datareader_ReadU8(reader, &effectId));

    if (!(gamestate->Version)->Protocol.RawEffectIds) {
        effectId += 1;
    }

    currentTile = map_GetTile(&gamestate->Map,
                              tilePosition.X,
                              tilePosition.Y,
                              tilePosition.Z);

    tile_AddGraphicalEffect(currentTile, effectId, gamestate->CurrentTick);

    return true;
}

static bool parser_ParseMissileEffect(struct trc_data_reader *reader,
                                      struct trc_game_state *gamestate) {
    struct trc_position origin, target;
    uint8_t missileId;

    ParseAssert(parser_ParsePosition(reader, &origin));
    ParseAssert(parser_ParsePosition(reader, &target));
    ParseAssert(datareader_ReadU8(reader, &missileId));

    if (!(gamestate->Version)->Protocol.RawEffectIds) {
        missileId += 1;
    }

    gamestate_AddMissileEffect(gamestate, &origin, &target, missileId);

    return true;
}

static bool parser_ParseTextEffect(struct trc_data_reader *reader,
                                   struct trc_game_state *gamestate) {
    struct trc_position tilePosition;
    struct trc_tile *tile;
    uint8_t color;

    uint16_t messageLength;
    char message[16];

    /* Text effects were replaced by message effects, so we've misparsed a
     * previous packet if we land here on a version that uses the latter. */
    ParseAssert(!(gamestate->Version)->Protocol.MessageEffects);
    ParseAssert(parser_ParsePosition(reader, &tilePosition));

    tile = map_GetTile(&gamestate->Map,
                       tilePosition.X,
                       tilePosition.Y,
                       tilePosition.Z);

    ParseAssert(datareader_ReadU8(reader, &color));
    ParseAssert(datareader_ReadString(reader,
                                      sizeof(message),
                                      &messageLength,
                                      message));

    tile_AddTextEffect(tile,
                       color,
                       messageLength,
                       message,
                       gamestate->CurrentTick);

    return true;
}

static bool parser_ParseMarkCreature(struct trc_data_reader *reader,
                                     struct trc_game_state *gamestate) {
    uint32_t creatureId;
    uint8_t color;

    ParseAssert(datareader_ReadU32(reader, &creatureId));
    ParseAssert(datareader_ReadU8(reader, &color));

    return true;
}

static bool parser_ParseTrappers(struct trc_data_reader *reader,
                                 struct trc_game_state *gamestate) {
    uint8_t creatureCount;
    uint32_t creatureId;

    ParseAssert(datareader_ReadU8(reader, &creatureCount));

    while (creatureCount--) {
        ParseAssert(datareader_ReadU32(reader, &creatureId));
    }

    return true;
}

static bool parser_ParseCreatureHealth(struct trc_data_reader *reader,
                                       struct trc_game_state *gamestate) {
    struct trc_creature *creature;
    uint32_t creatureId;
    uint8_t health;

    ParseAssert(datareader_ReadU32(reader, &creatureId));
    ParseAssert(datareader_ReadU8(reader, &health));
    ParseAssert(CHECK_RANGE(health, 0, 100));

    if (creaturelist_GetCreature(&gamestate->CreatureList,
                                 creatureId,
                                 &creature)) {
        creature->Health = health;
    }

    return true;
}

static bool parser_ParseCreatureLight(struct trc_data_reader *reader,
                                      struct trc_game_state *gamestate) {
    struct trc_creature *creature;

    uint8_t lightIntensity;
    uint8_t lightColor;
    uint32_t creatureId;

    ParseAssert(datareader_ReadU32(reader, &creatureId));
    ParseAssert(datareader_ReadU8(reader, &lightIntensity));
    ParseAssert(datareader_ReadU8(reader, &lightColor));

    if (creaturelist_GetCreature(&gamestate->CreatureList,
                                 creatureId,
                                 &creature)) {
        creature->LightIntensity = lightIntensity;
        creature->LightColor = lightColor;
    }

    return true;
}

static bool parser_ParseCreatureOutfit(struct trc_data_reader *reader,
                                       struct trc_game_state *gamestate) {
    struct trc_creature *creature;
    uint32_t creatureId;

    ParseAssert(datareader_ReadU32(reader, &creatureId));

    if (creaturelist_GetCreature(&gamestate->CreatureList,
                                 creatureId,
                                 &creature)) {
        ParseAssert(parser_ParseOutfit(reader, gamestate, &creature->Outfit));
    } else {
        struct trc_outfit nullOutfit;

        /* Nonexistent creatures are not a protocol violation, but we still need
         * to read an outfit */
        ParseAssert(parser_ParseOutfit(reader, gamestate, &nullOutfit));
    }

    return true;
}

static bool parser_ParseCreatureSpeed(struct trc_data_reader *reader,
                                      struct trc_game_state *gamestate) {
    struct trc_creature *creature;
    uint32_t creatureId;
    uint16_t speed;

    ParseAssert(datareader_ReadU32(reader, &creatureId));
    ParseAssert(datareader_ReadU16(reader, &speed));

    if ((gamestate->Version)->Protocol.CreatureSpeedPadding) {
        uint16_t padding;
        ParseAssert(datareader_ReadU16(reader, &padding));
    }

    if (creaturelist_GetCreature(&gamestate->CreatureList,
                                 creatureId,
                                 &creature)) {
        creature->Speed = speed;
    }

    return true;
}

static bool parser_ParseCreatureSkull(struct trc_data_reader *reader,
                                      struct trc_game_state *gamestate) {
    struct trc_creature *creature;
    uint32_t creatureId;
    uint8_t skull;

    ParseAssert(datareader_ReadU32(reader, &creatureId));
    ParseAssert(datareader_ReadU8(reader, &skull));
    ParseAssert(CHECK_RANGE(skull, CHARACTER_SKULL_NONE, CHARACTER_SKULL_LAST));

    if (creaturelist_GetCreature(&gamestate->CreatureList,
                                 creatureId,
                                 &creature)) {
        creature->Skull = (enum TrcCharacterSkull)skull;
    }

    return true;
}

static bool parser_ParseCreatureShield(struct trc_data_reader *reader,
                                       struct trc_game_state *gamestate) {
    struct trc_creature *creature;
    uint32_t creatureId;
    uint8_t shield;

    ParseAssert(datareader_ReadU32(reader, &creatureId));
    ParseAssert(datareader_ReadU8(reader, &shield));
    ParseAssert(CHECK_RANGE(shield, PARTY_SHIELD_NONE, PARTY_SHIELD_LAST));

    if (creaturelist_GetCreature(&gamestate->CreatureList,
                                 creatureId,
                                 &creature)) {
        creature->Shield = (enum TrcPartyShield)shield;
    }

    return true;
}

static bool parser_ParseCreatureImpassable(struct trc_data_reader *reader,
                                           struct trc_game_state *gamestate) {
    struct trc_creature *creature;
    uint8_t impassable;
    uint32_t creatureId;

    ParseAssert(datareader_ReadU32(reader, &creatureId));
    ParseAssert(datareader_ReadU8(reader, &impassable));

    if (creaturelist_GetCreature(&gamestate->CreatureList,
                                 creatureId,
                                 &creature)) {
        creature->Impassable = impassable;
    }

    return true;
}

static bool parser_ParseCreaturePvPHelpers(struct trc_data_reader *reader,
                                           struct trc_game_state *gamestate) {
    struct trc_creature *creature;

    if ((gamestate->Version)->Protocol.SinglePVPHelper) {
        uint8_t markType, mark;
        uint32_t creatureId;

        ParseAssert(datareader_ReadU32(reader, &creatureId));
        ParseAssert(datareader_ReadU8(reader, &markType));
        ParseAssert(datareader_ReadU8(reader, &mark));

        if (creaturelist_GetCreature(&gamestate->CreatureList,
                                     creatureId,
                                     &creature)) {
            creature->MarkIsPermanent = (markType != 1);
            creature->Mark = mark;
        }
    } else {
        uint8_t creatureCount;

        ParseAssert(datareader_ReadU8(reader, &creatureCount));

        while (creatureCount--) {
            uint8_t markType, mark;
            uint32_t creatureId;

            ParseAssert(datareader_ReadU32(reader, &creatureId));
            ParseAssert(datareader_ReadU8(reader, &markType));
            ParseAssert(datareader_ReadU8(reader, &mark));

            if (creaturelist_GetCreature(&gamestate->CreatureList,
                                         creatureId,
                                         &creature)) {
                creature->MarkIsPermanent = (markType != 1);
                creature->Mark = mark;
            }
        }
    }

    return true;
}

static bool parser_ParseCreatureGuildMembersOnline(
        struct trc_data_reader *reader,
        struct trc_game_state *gamestate) {
    struct trc_creature *creature;
    uint16_t guildMembersOnline;
    uint32_t creatureId;

    ParseAssert(datareader_ReadU32(reader, &creatureId));
    ParseAssert(datareader_ReadU16(reader, &guildMembersOnline));

    if (creaturelist_GetCreature(&gamestate->CreatureList,
                                 creatureId,
                                 &creature)) {
        creature->GuildMembersOnline = guildMembersOnline;
    }

    return true;
}

static bool parser_ParseCreatureType(struct trc_data_reader *reader,
                                     struct trc_game_state *gamestate) {
    struct trc_creature *creature;
    uint8_t type;
    uint32_t creatureId;

    ParseAssert(datareader_ReadU32(reader, &creatureId));
    ParseAssert(datareader_ReadU8(reader, &type));
    ParseAssert(CHECK_RANGE(type, CREATURE_TYPE_FIRST, CREATURE_TYPE_LAST));

    if (creaturelist_GetCreature(&gamestate->CreatureList,
                                 creatureId,
                                 &creature)) {
        creature->Type = (enum TrcCreatureType)type;
    }

    return true;
}

static bool parser_ParseOpenEditText(struct trc_data_reader *reader,
                                     struct trc_game_state *gamestate) {
    if ((gamestate->Version)->Protocol.TextEditObject) {
        struct trc_object textObject;
        ParseAssert(parser_ParseObject(reader, gamestate, &textObject));
    } else {
        /* Window id, item id? */
        ParseAssert(datareader_Skip(reader, 4 + 2));
    }

    ParseAssert(datareader_Skip(reader, 2));
    ParseAssert(datareader_SkipString(reader));

    if ((gamestate->Version)->Protocol.TextEditAuthorName) {
        ParseAssert(datareader_SkipString(reader));
    }

    if ((gamestate->Version)->Protocol.TextEditDate) {
        ParseAssert(datareader_SkipString(reader));
    }

    return true;
}

static bool parser_ParseOpenHouseWindow(struct trc_data_reader *reader,
                                        struct trc_game_state *gamestate) {
    /* Kind + window id? */
    ParseAssert(datareader_Skip(reader, 1 + 4));
    ParseAssert(datareader_SkipString(reader));

    return true;
}

static bool parser_ParseBlessings(struct trc_data_reader *reader,
                                  struct trc_game_state *gamestate) {
    ParseAssert(datareader_ReadU16(reader, &gamestate->Player.Blessings));

    return true;
}

static bool parser_ParseHotkeyPresets(struct trc_data_reader *reader,
                                      struct trc_game_state *gamestate) {
    ParseAssert(datareader_ReadU32(reader, &gamestate->Player.HotkeyPreset));

    return true;
}

TRC_UNUSED static bool parser_ParseOpenEditList(
        struct trc_data_reader *reader,
        struct trc_game_state *gamestate) {
    uint8_t type;
    uint16_t id;

    ParseAssert(datareader_ReadU8(reader, &type));
    ParseAssert(datareader_ReadU16(reader, &id));
    ParseAssert(datareader_SkipString(reader));

    return true;
}

static bool parser_ParsePremiumTrigger(struct trc_data_reader *reader,
                                       struct trc_game_state *gamestate) {
    uint8_t unknownCount;
    uint8_t unknownBool;

    ParseAssert(datareader_ReadU8(reader, &unknownCount));
    ParseAssert(datareader_Skip(reader, unknownCount));
    ParseAssert(datareader_ReadU8(reader, &unknownBool));

    return true;
}

static bool parser_ParsePlayerDataBasic(struct trc_data_reader *reader,
                                        struct trc_game_state *gamestate) {
    uint16_t spellCount;

    ParseAssert(datareader_ReadU8(reader, &gamestate->Player.IsPremium));

    if ((gamestate->Version)->Protocol.PremiumUntil) {
        ParseAssert(
                datareader_ReadU32(reader, &gamestate->Player.PremiumUntil));
    }

    ParseAssert(datareader_ReadU8(reader, &gamestate->Player.Vocation));
    ParseAssert(datareader_ReadU16(reader, &spellCount));
    ParseAssert(datareader_Skip(reader, spellCount));

    return true;
}

static bool parser_ParsePlayerDataCurrent(struct trc_data_reader *reader,
                                          struct trc_game_state *gamestate) {
    ParseAssert(datareader_ReadU16(reader, &gamestate->Player.Stats.Health));
    ParseAssert(datareader_ReadU16(reader, &gamestate->Player.Stats.MaxHealth));

    if ((gamestate->Version)->Protocol.CapacityU32) {
        ParseAssert(
                datareader_ReadU32(reader, &gamestate->Player.Stats.Capacity));

        if ((gamestate->Version)->Protocol.MaxCapacity) {
            ParseAssert(
                    datareader_ReadU32(reader,
                                       &gamestate->Player.Stats.MaxCapacity));
        }
    } else {
        ParseAssert(
                datareader_ReadU16(reader, &gamestate->Player.Stats.Capacity));
    }

    if ((gamestate->Version)->Protocol.ExperienceU64) {
        ParseAssert(datareader_ReadU64(reader,
                                       &gamestate->Player.Stats.Experience));
    } else {
        ParseAssert(datareader_ReadU32(reader,
                                       &gamestate->Player.Stats.Experience));
    }

    if ((gamestate->Version)->Protocol.LevelU16) {
        ParseAssert(datareader_ReadU16(reader, &gamestate->Player.Stats.Level));
    } else {
        ParseAssert(datareader_ReadU8(reader, &gamestate->Player.Stats.Level));
    }

    if ((gamestate->Version)->Protocol.SkillPercentages) {
        ParseAssert(datareader_ReadU8(reader,
                                      &gamestate->Player.Stats.LevelPercent));
        ParseAssert(CHECK_RANGE(gamestate->Player.Stats.LevelPercent, 0, 100));
    }

    if ((gamestate->Version)->Protocol.ExperienceBonus) {
        ParseAssert(
                datareader_ReadFloat(reader,
                                     &gamestate->Player.Stats.ExperienceBonus));
    }

    ParseAssert(datareader_ReadU16(reader, &gamestate->Player.Stats.Mana));
    ParseAssert(datareader_ReadU16(reader, &gamestate->Player.Stats.MaxMana));
    ParseAssert(CHECK_RANGE(gamestate->Player.Stats.Mana,
                            0,
                            gamestate->Player.Stats.MaxMana));

    ParseAssert(datareader_ReadU8(reader, &gamestate->Player.Stats.MagicLevel));

    if ((gamestate->Version)->Protocol.LoyaltyBonus) {
        ParseAssert(datareader_ReadU8(reader,
                                      &gamestate->Player.Stats.MagicLevelBase));
    }

    if ((gamestate->Version)->Protocol.SkillPercentages) {
        ParseAssert(
                datareader_ReadU8(reader,
                                  &gamestate->Player.Stats.MagicLevelPercent));
        ParseAssert(
                CHECK_RANGE(gamestate->Player.Stats.MagicLevelPercent, 0, 100));
    }

    if ((gamestate->Version)->Protocol.SoulPoints) {
        ParseAssert(
                datareader_ReadU8(reader, &gamestate->Player.Stats.SoulPoints));
    }

    if ((gamestate->Version)->Protocol.Stamina) {
        ParseAssert(
                datareader_ReadU16(reader, &gamestate->Player.Stats.Stamina));
    }

    if ((gamestate->Version)->Protocol.PlayerSpeed) {
        ParseAssert(datareader_ReadU16(reader, &gamestate->Player.Stats.Speed));
    }

    if ((gamestate->Version)->Protocol.PlayerHunger) {
        ParseAssert(datareader_ReadU16(reader, &gamestate->Player.Stats.Fed));
    }

    if ((gamestate->Version)->Protocol.OfflineStamina) {
        ParseAssert(
                datareader_ReadU16(reader,
                                   &gamestate->Player.Stats.OfflineStamina));
    }

    return true;
}

static bool parser_ParsePlayerSkills(struct trc_data_reader *reader,
                                     struct trc_game_state *gamestate) {
    struct trc_player *player = &gamestate->Player;

    for (int i = 0; i < PLAYER_SKILL_COUNT; i++) {
        if ((gamestate->Version)->Protocol.SkillsU16) {
            ParseAssert(
                    datareader_ReadU16(reader, &player->Skills[i].Effective));
            ParseAssert(datareader_ReadU16(reader, &player->Skills[i].Actual));
            ParseAssert(datareader_ReadU8(reader, &player->Skills[i].Percent));
        } else {
            ParseAssert(
                    datareader_ReadU8(reader, &player->Skills[i].Effective));

            if ((gamestate->Version)->Protocol.LoyaltyBonus) {
                ParseAssert(
                        datareader_ReadU8(reader, &player->Skills[i].Actual));
            }

            if ((gamestate->Version)->Protocol.SkillPercentages) {
                ParseAssert(
                        datareader_ReadU8(reader, &player->Skills[i].Percent));

                ParseAssert(CHECK_RANGE(player->Skills[i].Percent, 0, 100));
            }
        }
    }

    if ((gamestate->Version)->Protocol.SkillsUnknownPadding) {
        ParseAssert(datareader_Skip(reader, 6 * 4));
    }

    return true;
}

static bool parser_ParsePlayerIcons(struct trc_data_reader *reader,
                                    struct trc_game_state *gamestate) {
    uint16_t icons;

    if ((gamestate->Version)->Protocol.IconsU16) {
        ParseAssert(datareader_ReadU16(reader, &icons));
    } else {
        ParseAssert(datareader_ReadU8(reader, &icons));
    }

    /* Make sure we don't forget to add a ParseAssert if this is widened in a
     * later version (>11.x?). */
    _Static_assert(15 == STATUS_ICON_LAST, "Status icons must fit in 16 bits");
    gamestate->Player.Icons = (enum TrcStatusIcon)icons;

    return true;
}

static bool parser_ParseCancelAttack(struct trc_data_reader *reader,
                                     struct trc_game_state *gamestate) {
    if ((gamestate->Version)->Protocol.CancelAttackId) {
        uint32_t attackId;

        ParseAssert(datareader_ReadU32(reader, &attackId));
    }

    return true;
}

static bool parser_ParseSpellCooldown(struct trc_data_reader *reader,
                                      struct trc_game_state *gamestate) {
    uint32_t cooldownTime;
    uint8_t spellId;

    ParseAssert(datareader_ReadU8(reader, &spellId));
    ParseAssert(datareader_ReadU32(reader, &cooldownTime));

    return true;
}

static bool parser_ParseUseCooldown(struct trc_data_reader *reader,
                                    struct trc_game_state *gamestate) {
    uint32_t cooldownTime;

    ParseAssert(datareader_ReadU32(reader, &cooldownTime));

    return true;
}

static bool parser_ParsePlayerTactics(struct trc_data_reader *reader,
                                      struct trc_game_state *gamestate) {
    ParseAssert(datareader_ReadU8(reader, &gamestate->Player.AttackMode));
    ParseAssert(datareader_ReadU8(reader, &gamestate->Player.ChaseMode));
    ParseAssert(datareader_ReadU8(reader, &gamestate->Player.SecureMode));
    ParseAssert(datareader_ReadU8(reader, &gamestate->Player.PvPMode));

    return true;
}

static bool parser_ParseCreatureSpeak(struct trc_data_reader *reader,
                                      struct trc_game_state *gamestate) {
    uint16_t authorNameLength;
    enum TrcMessageMode messageMode;
    uint16_t speakerLevel;
    uint8_t speakType;
    uint32_t messageId;
    char authorName[32];

    if ((gamestate->Version)->Protocol.ReportMessages) {
        ParseAssert(datareader_ReadU32(reader, &messageId));
    }

    ParseAssert(datareader_ReadString(reader,
                                      sizeof(authorName),
                                      &authorNameLength,
                                      authorName));

    if ((gamestate->Version)->Protocol.SpeakerLevel) {
        ParseAssert(datareader_ReadU16(reader, &speakerLevel));
    }

    ParseAssert(datareader_ReadU8(reader, &speakType));
    ParseAssert(version_TranslateSpeakMode(gamestate->Version,
                                           speakType,
                                           &messageMode));

    switch (messageMode) {
    case MESSAGEMODE_SAY:
    case MESSAGEMODE_WHISPER:
    case MESSAGEMODE_YELL:
    case MESSAGEMODE_SPELL:
    case MESSAGEMODE_NPC_START:
    case MESSAGEMODE_MONSTER_SAY:
    case MESSAGEMODE_MONSTER_YELL: {
        struct trc_position tilePosition;
        uint16_t messageLength;
        char message[1024];

        ParseAssert(parser_ParsePosition(reader, &tilePosition));

        /* There's no need to cut off messages that are seemingly incorrect;
         * the Tibia client displays all received messages regardless of
         * coordinates. */
        ParseAssert(datareader_ReadString(reader,
                                          sizeof(message),
                                          &messageLength,
                                          message));

#ifdef DUMP_MESSAGE_TYPES
        fprintf(stdout, "SP %i %s: %s\n", speakType, authorName, message);
#endif

        gamestate_AddTextMessage(gamestate,
                                 &tilePosition,
                                 messageMode,
                                 authorNameLength,
                                 authorName,
                                 messageLength,
                                 message);
        break;
    }
    case MESSAGEMODE_NPC_CONTINUED: {
        uint16_t messageLength;
        char message[256];

        ParseAssert(datareader_ReadString(reader,
                                          sizeof(message),
                                          &messageLength,
                                          message));

#ifdef DUMP_MESSAGE_TYPES
        fprintf(stdout, "SP %i %s: %s\n", speakType, authorName, message);
#endif

        /* TODO: Implement this properly, I think it should appear at the same
         * position as the corresponding MESSAGEMODE_NPC_START. */
        break;
    }
    case MESSAGEMODE_PRIVATE_IN: {
        uint16_t messageLength;
        char message[256];

        ParseAssert(datareader_ReadString(reader,
                                          sizeof(message),
                                          &messageLength,
                                          message));

#ifdef DUMP_MESSAGE_TYPES
        fprintf(stdout, "SP %i %s: %s\n", speakType, authorName, message);
#endif

        gamestate_AddTextMessage(gamestate,
                                 NULL,
                                 messageMode,
                                 authorNameLength,
                                 authorName,
                                 messageLength,
                                 message);

        break;
    }
    case MESSAGEMODE_CHANNEL_YELLOW:
    case MESSAGEMODE_CHANNEL_ORANGE:
    case MESSAGEMODE_CHANNEL_RED: {
        uint16_t channelId;

        ParseAssert(datareader_ReadU16(reader, &channelId));

        /* FALLTHROUGH */
    }
    default:
#ifdef DUMP_MESSAGE_TYPES
    {
        uint16_t messageLength;
        char message[1024];

        ParseAssert(datareader_ReadString(reader,
                                          sizeof(message),
                                          &messageLength,
                                          message));

        fprintf(stdout, "SP %i %s: %s\n", speakType, authorName, message);
    }
#else
        ParseAssert(datareader_SkipString(reader));
#endif
    }

    return true;
}

static bool parser_ParseChannelList(struct trc_data_reader *reader,
                                    struct trc_game_state *gamestate) {
    uint8_t channelCount;

    ParseAssert(datareader_ReadU8(reader, &channelCount));

    while (channelCount--) {
        uint16_t channelId;

        ParseAssert(datareader_ReadU16(reader, &channelId));
        ParseAssert(datareader_SkipString(reader));
    }

    return true;
}

static bool parser_ParseChannelOpen(struct trc_data_reader *reader,
                                    struct trc_game_state *gamestate) {
    uint16_t participantCount, inviteeCount, channelId;

    ParseAssert(datareader_ReadU16(reader, &channelId));
    ParseAssert(datareader_SkipString(reader));

    if ((gamestate->Version)->Protocol.ChannelParticipants) {
        ParseAssert(datareader_ReadU16(reader, &participantCount));

        while (participantCount--) {
            ParseAssert(datareader_SkipString(reader));
        }

        ParseAssert(datareader_ReadU16(reader, &inviteeCount));

        while (inviteeCount--) {
            ParseAssert(datareader_SkipString(reader));
        }
    }

    return true;
}

static bool parser_ParseChannelClose(struct trc_data_reader *reader,
                                     struct trc_game_state *gamestate) {
    uint16_t channelId;

    ParseAssert(datareader_ReadU16(reader, &channelId));

    return true;
}

static bool parser_ParseOpenPrivateConversation(
        struct trc_data_reader *reader,
        struct trc_game_state *gamestate) {
    ParseAssert(datareader_SkipString(reader));

    return true;
}

static bool parser_ParseTextMessage(struct trc_data_reader *reader,
                                    struct trc_game_state *gamestate) {
    enum TrcMessageMode messageMode;
    uint8_t speakType;
    uint8_t color;

    ParseAssert(datareader_ReadU8(reader, &speakType));
    ParseAssert(version_TranslateMessageMode(gamestate->Version,
                                             speakType,
                                             &messageMode));

    switch (messageMode) {
    case MESSAGEMODE_CHANNEL_WHITE: {
        uint16_t channelId;

        ParseAssert(datareader_ReadU16(reader, &channelId));

        break;
    }
    case MESSAGEMODE_DAMAGE_DEALT:
    case MESSAGEMODE_DAMAGE_RECEIVED:
    case MESSAGEMODE_DAMAGE_RECEIVED_OTHERS: {
        struct trc_position tilePosition;
        uint32_t damageValue;
        struct trc_tile *tile;

        if (!(gamestate->Version)->Protocol.MessageEffects) {
            break;
        }

        ParseAssert(parser_ParsePosition(reader, &tilePosition));

        tile = map_GetTile(&gamestate->Map,
                           tilePosition.X,
                           tilePosition.Y,
                           tilePosition.Z);

        ParseAssert(datareader_ReadU32(reader, &damageValue));
        ParseAssert(datareader_ReadU8(reader, &color));

        if (damageValue > 0) {
            ASSERT((gamestate->Version)->Protocol.MessageEffects);
            tile_AddNumericalEffect(tile,
                                    color,
                                    damageValue,
                                    gamestate->CurrentTick);
        }

        ParseAssert(datareader_ReadU32(reader, &damageValue));
        ParseAssert(datareader_ReadU8(reader, &color));

        if (damageValue > 0) {
            ASSERT((gamestate->Version)->Protocol.MessageEffects);
            tile_AddNumericalEffect(tile,
                                    color,
                                    damageValue,
                                    gamestate->CurrentTick);
        }
        break;
    }
    case MESSAGEMODE_HEALING:
    case MESSAGEMODE_HEALING_OTHERS:
    case MESSAGEMODE_EXPERIENCE:
    case MESSAGEMODE_EXPERIENCE_OTHERS:
    case MESSAGEMODE_MANA: {
        struct trc_position tilePosition;
        struct trc_tile *tile;
        uint32_t value;

        if (!(gamestate->Version)->Protocol.MessageEffects) {
            break;
        }

        ParseAssert(parser_ParsePosition(reader, &tilePosition));

        tile = map_GetTile(&gamestate->Map,
                           tilePosition.X,
                           tilePosition.Y,
                           tilePosition.Z);

        ParseAssert(datareader_ReadU32(reader, &value));
        ParseAssert(datareader_ReadU8(reader, &color));

        if (value > 0) {
            tile_AddNumericalEffect(tile, color, value, gamestate->CurrentTick);
        }
        break;
    }
    case MESSAGEMODE_PARTY:
    case MESSAGEMODE_PARTY_WHITE:
        if ((gamestate->Version)->Protocol.PartyChannelId) {
            uint16_t channelId;
            ParseAssert(datareader_ReadU16(reader, &channelId));
            break;
        }

    case MESSAGEMODE_GUILD:
        if ((gamestate->Version)->Protocol.GuildChannelId) {
            uint16_t channelId;
            ParseAssert(datareader_ReadU16(reader, &channelId));
        }
        /* FALLTHROUGH */
    case MESSAGEMODE_HOTKEY:
    case MESSAGEMODE_NPC_TRADE:
    case MESSAGEMODE_GAME:
    case MESSAGEMODE_LOOK:
    case MESSAGEMODE_LOOT:
    case MESSAGEMODE_LOGIN:
    case MESSAGEMODE_WARNING:
    case MESSAGEMODE_FAILURE:
    case MESSAGEMODE_STATUS: {
        uint16_t messageLength;
        char message[1024];

        ParseAssert(datareader_ReadString(reader,
                                          sizeof(message),
                                          &messageLength,
                                          message));
        gamestate_AddTextMessage(gamestate,
                                 NULL,
                                 messageMode,
                                 0,
                                 NULL,
                                 messageLength,
                                 message);

#ifdef DUMP_MESSAGE_TYPES
        fprintf(stdout, "TM %i: %s\n", speakType, message);
#endif

        return true;
    }
    default:
        ParseAssert(!"Unhandled message mode");
    }

#ifdef DUMP_MESSAGE_TYPES
    {
        uint16_t messageLength;
        char message[1024];

        ParseAssert(datareader_ReadString(reader,
                                          sizeof(message),
                                          &messageLength,
                                          message));

        fprintf(stdout, "TM %i: %s\n", speakType, message);
    }
#else
    ParseAssert(datareader_SkipString(reader));
#endif

    return true;
}

static bool parser_ParseMoveDenied(struct trc_data_reader *reader,
                                   struct trc_game_state *gamestate) {
    if ((gamestate->Version)->Protocol.MoveDeniedDirection) {
        uint8_t direction;
        ParseAssert(datareader_ReadU8(reader, &direction));
        ParseAssert(CHECK_RANGE(direction,
                                CREATURE_DIRECTION_FIRST,
                                CREATURE_DIRECTION_LAST));
    }

    return true;
}

static bool parser_ParseMoveDelay(struct trc_data_reader *reader,
                                  struct trc_game_state *gamestate) {
    uint16_t delay;

    ParseAssert(datareader_ReadU16(reader, &delay));

    return true;
}

static bool parser_ParseOpenPvPSituations(struct trc_data_reader *reader,
                                          struct trc_game_state *gamestate) {
    ParseAssert(
            datareader_ReadU8(reader, &gamestate->Player.OpenPvPSituations));

    return true;
}

static bool parser_ParseUnjustifiedPoints(struct trc_data_reader *reader,
                                          struct trc_game_state *gamestate) {
    ParseAssert(datareader_ReadU8(
            reader,
            &gamestate->Player.UnjustifiedKillsInfo.ProgressDay));
    ParseAssert(datareader_ReadU8(
            reader,
            &gamestate->Player.UnjustifiedKillsInfo.KillsRemainingDay));

    ParseAssert(datareader_ReadU8(
            reader,
            &gamestate->Player.UnjustifiedKillsInfo.ProgressWeek));
    ParseAssert(datareader_ReadU8(
            reader,
            &gamestate->Player.UnjustifiedKillsInfo.KillsRemainingWeek));

    ParseAssert(datareader_ReadU8(
            reader,
            &gamestate->Player.UnjustifiedKillsInfo.ProgressMonth));
    ParseAssert(datareader_ReadU8(
            reader,
            &gamestate->Player.UnjustifiedKillsInfo.KillsRemainingMonth));

    ParseAssert(datareader_ReadU8(
            reader,
            &gamestate->Player.UnjustifiedKillsInfo.SkullDuration));

    return true;
}

static bool parser_ParseFloorChangeUp(struct trc_data_reader *reader,
                                      struct trc_game_state *gamestate) {
    uint16_t tileSkip;

    gamestate->Map.Position.Z--;
    tileSkip = 0;

    if (gamestate->Map.Position.Z == 7) {
        for (int zIdx = 5; zIdx >= 0; zIdx--) {
            ParseAssert(
                    parser_ParseFloorDescription(reader,
                                                 gamestate,
                                                 gamestate->Map.Position.X - 8,
                                                 gamestate->Map.Position.Y - 6,
                                                 zIdx,
                                                 TILE_BUFFER_WIDTH,
                                                 TILE_BUFFER_HEIGHT,
                                                 TILE_BUFFER_DEPTH - zIdx,
                                                 &tileSkip));
        }
    } else if (gamestate->Map.Position.Z > 7) {
        ParseAssert(parser_ParseFloorDescription(reader,
                                                 gamestate,
                                                 gamestate->Map.Position.X - 8,
                                                 gamestate->Map.Position.Y - 6,
                                                 gamestate->Map.Position.Z - 2,
                                                 TILE_BUFFER_WIDTH,
                                                 TILE_BUFFER_HEIGHT,
                                                 3,
                                                 &tileSkip));
    }

    gamestate->Map.Position.X++;
    gamestate->Map.Position.Y++;

    return true;
}

static bool parser_ParseFloorChangeDown(struct trc_data_reader *reader,
                                        struct trc_game_state *gamestate) {
    uint16_t tileSkip;

    gamestate->Map.Position.Z++;
    tileSkip = 0;

    if (gamestate->Map.Position.Z == 8) {
        for (int zIdx = gamestate->Map.Position.Z, offset = -1;
             zIdx <= gamestate->Map.Position.Z + 2;
             zIdx++) {
            ParseAssert(
                    parser_ParseFloorDescription(reader,
                                                 gamestate,
                                                 gamestate->Map.Position.X - 8,
                                                 gamestate->Map.Position.Y - 6,
                                                 zIdx,
                                                 TILE_BUFFER_WIDTH,
                                                 TILE_BUFFER_HEIGHT,
                                                 offset,
                                                 &tileSkip));

            offset--;
        }
    } else if (gamestate->Map.Position.Z > 7 &&
               gamestate->Map.Position.Z < 14) {
        ParseAssert(parser_ParseFloorDescription(reader,
                                                 gamestate,
                                                 gamestate->Map.Position.X - 8,
                                                 gamestate->Map.Position.Y - 6,
                                                 gamestate->Map.Position.Z + 2,
                                                 TILE_BUFFER_WIDTH,
                                                 TILE_BUFFER_HEIGHT,
                                                 -3,
                                                 &tileSkip));
    }

    gamestate->Map.Position.X--;
    gamestate->Map.Position.Y--;

    return true;
}

static bool parser_ParseOutfitDialog(struct trc_data_reader *reader,
                                     struct trc_game_state *gamestate) {
    struct trc_outfit nullOutfit;
    uint16_t outfitCount;
    uint16_t mountCount;

    ParseAssert(parser_ParseOutfit(reader, gamestate, &nullOutfit));

    if ((gamestate->Version)->Protocol.OutfitAddons) {
        if ((gamestate->Version)->Protocol.OutfitCountU16) {
            ParseAssert(datareader_ReadU16(reader, &outfitCount));
        } else {
            ParseAssert(datareader_ReadU8(reader, &outfitCount));
        }

        while (outfitCount--) {
            uint16_t outfitId;
            uint8_t addons;

            ParseAssert(datareader_ReadU16(reader, &outfitId));
            if ((gamestate->Version)->Protocol.OutfitNames) {
                ParseAssert(datareader_SkipString(reader));
            }
            ParseAssert(datareader_ReadU8(reader, &addons));
        }
    } else if ((gamestate->Version)->Protocol.OutfitsU16) {
        /* Start outfit, end outfit */
        ParseAssert(datareader_Skip(reader, 2 + 2));
    } else {
        /* Start outfit, end outfit */
        ParseAssert(datareader_Skip(reader, 1 + 1));
    }

    if ((gamestate->Version)->Protocol.Mounts) {
        if ((gamestate->Version)->Protocol.OutfitCountU16) {
            ParseAssert(datareader_ReadU16(reader, &mountCount));
        } else {
            ParseAssert(datareader_ReadU8(reader, &mountCount));
        }

        while (mountCount--) {
            uint16_t mountId;

            ParseAssert(datareader_ReadU16(reader, &mountId));
            ParseAssert(datareader_SkipString(reader));
        }
    }

    return true;
}

static bool parser_ParseVIPStatus(struct trc_data_reader *reader,
                                  struct trc_game_state *gamestate) {
    uint8_t isOnline;
    uint32_t playerId;

    ParseAssert(datareader_ReadU32(reader, &playerId));
    ParseAssert(datareader_SkipString(reader));
    ParseAssert(datareader_ReadU8(reader, &isOnline));

    return true;
}

static bool parser_ParseVIPOnline(struct trc_data_reader *reader,
                                  struct trc_game_state *gamestate) {
    uint32_t playerId;

    ParseAssert(datareader_ReadU32(reader, &playerId));

    return true;
}

static bool parser_ParseVIPOffline(struct trc_data_reader *reader,
                                   struct trc_game_state *gamestate) {
    uint32_t playerId;

    ParseAssert(datareader_ReadU32(reader, &playerId));

    return true;
}

static bool parser_ParseTutorialShow(struct trc_data_reader *reader,
                                     struct trc_game_state *gamestate) {
    uint8_t tutorialId;

    ParseAssert(datareader_ReadU8(reader, &tutorialId));

    return true;
}

static bool parser_ParseMinimapFlag(struct trc_data_reader *reader,
                                    struct trc_game_state *gamestate) {
    struct trc_position tilePosition;
    uint8_t tutorialId;

    ParseAssert(parser_ParsePosition(reader, &tilePosition));
    ParseAssert(datareader_ReadU8(reader, &tutorialId));
    ParseAssert(datareader_SkipString(reader));

    return true;
}

static bool parser_ParseQuestDialog(struct trc_data_reader *reader,
                                    struct trc_game_state *gamestate) {
    int questCount;

    ParseAssert(datareader_ReadU16(reader, &questCount));

    while (questCount--) {
        int completed, questId;

        ParseAssert(datareader_ReadU16(reader, &questId));
        ParseAssert(datareader_SkipString(reader));
        ParseAssert(datareader_ReadU8(reader, &completed));
    }

    return true;
}

static bool parser_ParseQuestDialogMission(struct trc_data_reader *reader,
                                           struct trc_game_state *gamestate) {
    uint8_t missionCount;
    uint16_t questId;

    ParseAssert(datareader_ReadU16(reader, &questId));
    ParseAssert(datareader_ReadU8(reader, &missionCount));

    while (missionCount--) {
        ParseAssert(datareader_SkipString(reader));
        ParseAssert(datareader_SkipString(reader));
    }

    return true;
}

static bool parser_ParseOffenseReportResponse(
        struct trc_data_reader *reader,
        struct trc_game_state *gamestate) {
    ParseAssert(datareader_SkipString(reader));

    return true;
}

static bool parser_ParseChannelEvent(struct trc_data_reader *reader,
                                     struct trc_game_state *gamestate) {
    uint16_t channelId;
    uint8_t eventType;

    ParseAssert(datareader_ReadU16(reader, &channelId));
    ParseAssert(datareader_SkipString(reader));
    ParseAssert(datareader_ReadU8(reader, &eventType));

    return true;
}

static bool parser_ParseMarketInitialization(struct trc_data_reader *reader,
                                             struct trc_game_state *gamestate) {
    uint64_t playerMoney;
    uint16_t itemTypeCount;
    uint8_t vocationId;

    if ((gamestate->Version)->Protocol.PlayerMoneyU64) {
        ParseAssert(datareader_ReadU64(reader, &playerMoney));
    } else {
        ParseAssert(datareader_ReadU32(reader, &playerMoney));
    }

    ParseAssert(datareader_ReadU16(reader, &itemTypeCount));
    ParseAssert(datareader_ReadU8(reader, &vocationId));

    while (itemTypeCount--) {
        uint16_t itemId;
        uint16_t itemCount;

        ParseAssert(datareader_ReadU16(reader, &itemId));
        ParseAssert(datareader_ReadU16(reader, &itemCount));
    }

    return true;
}

static bool parser_ParseMarketItemDetails(struct trc_data_reader *reader,
                                          struct trc_game_state *gamestate) {
    uint8_t buyOfferIterator, sellOfferIterator;
    uint16_t itemId;
    int propertyIterator;

    ParseAssert(datareader_ReadU16(reader, &itemId));

    propertyIterator = 15;

    while (propertyIterator--) {
        ParseAssert(datareader_SkipString(reader));
    }

    ParseAssert(datareader_ReadU8(reader, &buyOfferIterator));

    while (buyOfferIterator--) {
        uint32_t offerCount, lowestBid, averageBid, highestBid;

        ParseAssert(datareader_ReadU32(reader, &offerCount));
        ParseAssert(datareader_ReadU32(reader, &lowestBid));
        ParseAssert(datareader_ReadU32(reader, &averageBid));
        ParseAssert(datareader_ReadU32(reader, &highestBid));
    }

    ParseAssert(datareader_ReadU8(reader, &sellOfferIterator));

    while (sellOfferIterator--) {
        uint32_t offerCount, lowestBid, averageBid, highestBid;

        ParseAssert(datareader_ReadU32(reader, &offerCount));
        ParseAssert(datareader_ReadU32(reader, &lowestBid));
        ParseAssert(datareader_ReadU32(reader, &averageBid));
        ParseAssert(datareader_ReadU32(reader, &highestBid));
    }

    return true;
}

static bool parser_ParseMarketBrowse(struct trc_data_reader *reader,
                                     struct trc_game_state *gamestate) {
    uint16_t browseType;

    ParseAssert(datareader_ReadU16(reader, &browseType));

    for (int blockIdx = 0; blockIdx < 3; blockIdx++) {
        uint32_t offerCount;

        ParseAssert(datareader_ReadU32(reader, &offerCount));

        while (offerCount--) {
            uint16_t offerSize, itemId, unknown;
            uint32_t offerEndTime, offerPrice;

            ParseAssert(datareader_ReadU32(reader, &offerEndTime));
            ParseAssert(datareader_ReadU16(reader, &unknown));

            switch (browseType) {
            case 0xFFFF: /* Own offers */
            case 0xFFFE: /* Own history */
                ParseAssert(datareader_ReadU16(reader, &itemId));
            }

            ParseAssert(datareader_ReadU16(reader, &offerSize));
            ParseAssert(datareader_ReadU32(reader, &offerPrice));

            switch (browseType) {
            case 0xFFFF: /* Own offers */
                break;
            case 0xFFFE: /* Own history */
            {
                uint8_t offerState;

                ParseAssert(datareader_ReadU8(reader, &offerState));
            }

                /* Fallthrough intentional. */
            default:
                ParseAssert(datareader_SkipString(reader));
            }
        }
    }

    return true;
}

static bool parser_ParseDeathDialog(struct trc_data_reader *reader,
                                    struct trc_game_state *gamestate) {
    if ((gamestate->Version)->Protocol.ExtendedDeathDialog) {
        uint8_t dialogType;

        ParseAssert(datareader_ReadU8(reader, &dialogType));

        if ((gamestate->Version)->Protocol.UnfairFightReduction) {
            if (dialogType == 0) {
                uint8_t unfairFightReduction;

                ParseAssert(datareader_ReadU8(reader, &unfairFightReduction));
            }
        }
    }

    return true;
}

static bool parser_ParsePlayerInventory(struct trc_data_reader *reader,
                                        struct trc_game_state *gamestate) {
    uint16_t count;

    ParseAssert(datareader_ReadU16(reader, &count));

    for (int i = 0; i < count; i++) {
        uint16_t itemId;
        uint16_t itemCount;
        uint8_t itemData;

        ParseAssert(datareader_ReadU16(reader, &itemId));
        ParseAssert(datareader_ReadU8(reader, &itemData));
        ParseAssert(datareader_ReadU16(reader, &itemCount));
    }

    return true;
}

static bool parser_ParseRuleViolation(struct trc_data_reader *reader,
                                      struct trc_game_state *gamestate) {
    ParseAssert(datareader_Skip(reader, 2));
    return true;
}

bool parser_ParsePacket(struct trc_data_reader *reader,
                        struct trc_game_state *gamestate) {
    uint8_t packetType;

    ParseAssert(datareader_ReadU8(reader, &packetType));

    switch (packetType) {
    case 0x0A:
        /* HACK: This got re-used as a ping packet in 9.72, perhaps we should
         * translate packet types to canonical constants as well? */
        if (!VERSION_AT_LEAST(gamestate->Version, 9, 72)) {
            return parser_ParseInitialization(reader, gamestate);
        }
        break;
    case 0x0B:
        return parser_ParseGMActions(reader, gamestate);
    case 0x0F:
        return true;
    case 0x17:
        return parser_ParseInitialization(reader, gamestate);
    case 0x1D:
    case 0x1E:
        /* Single-byte ping packets. */
        return true;
    case 0x28:
        return parser_ParseDeathDialog(reader, gamestate);
    case 0x64:
        return parser_ParseFullMapDescription(reader, gamestate);
    case 0x65:
        /* Move north */
        gamestate->Map.Position.Y--;

        return parser_ParseMapDescription(reader,
                                          gamestate,
                                          -8,
                                          -6,
                                          TILE_BUFFER_WIDTH,
                                          1);
    case 0x66:
        /* Move east */
        gamestate->Map.Position.X++;

        return parser_ParseMapDescription(reader,
                                          gamestate,
                                          +9,
                                          -6,
                                          1,
                                          TILE_BUFFER_HEIGHT);
    case 0x67:
        /* Move south */
        gamestate->Map.Position.Y++;

        return parser_ParseMapDescription(reader,
                                          gamestate,
                                          -8,
                                          +7,
                                          TILE_BUFFER_WIDTH,
                                          1);
    case 0x68:
        /* Move west */
        gamestate->Map.Position.X--;

        return parser_ParseMapDescription(reader,
                                          gamestate,
                                          -8,
                                          -6,
                                          1,
                                          TILE_BUFFER_HEIGHT);
    case 0x69:
        return parser_ParseTileUpdate(reader, gamestate);
    case 0x6A:
        return parser_ParseTileAddObject(reader, gamestate);
    case 0x6B:
        return parser_ParseTileSetObject(reader, gamestate);
    case 0x6C:
        return parser_ParseTileRemoveObject(reader, gamestate);
    case 0x6D:
        return parser_ParseTileMoveCreature(reader, gamestate);
    case 0x6E:
        return parser_ParseContainerOpen(reader, gamestate);
    case 0x6F:
        return parser_ParseContainerClose(reader, gamestate);
    case 0x70:
        return parser_ParseContainerAddItem(reader, gamestate);
    case 0x71:
        return parser_ParseContainerTransformItem(reader, gamestate);
    case 0x72:
        return parser_ParseContainerRemoveItem(reader, gamestate);
    case 0x78:
        return parser_ParseInventorySetSlot(reader, gamestate);
    case 0x79:
        return parser_ParseInventoryClearSlot(reader, gamestate);
    case 0x7A:
        return parser_ParseNPCVendorBegin(reader, gamestate);
    case 0x7B:
        return parser_ParseNPCVendorPlayerGoods(reader, gamestate);
    case 0x7C:
        /* Single-byte NPC vendor abort */
        return true;
    case 0x7D:
    case 0x7E:
        return parser_ParsePlayerTradeItems(reader, gamestate);
    case 0x7F:
        /* Single-byte player trade abort */
        return true;
    case 0x82:
        return parser_ParseAmbientLight(reader, gamestate);
    case 0x83:
        return parser_ParseGraphicalEffect(reader, gamestate);
    case 0x84:
        return parser_ParseTextEffect(reader, gamestate);
    case 0x85:
        return parser_ParseMissileEffect(reader, gamestate);
    case 0x86:
        return parser_ParseMarkCreature(reader, gamestate);
    case 0x87:
        return parser_ParseTrappers(reader, gamestate);
    case 0x8C:
        return parser_ParseCreatureHealth(reader, gamestate);
    case 0x8D:
        return parser_ParseCreatureLight(reader, gamestate);
    case 0x8E:
        return parser_ParseCreatureOutfit(reader, gamestate);
    case 0x8F:
        return parser_ParseCreatureSpeed(reader, gamestate);
    case 0x90:
        return parser_ParseCreatureSkull(reader, gamestate);
    case 0x91:
        return parser_ParseCreatureShield(reader, gamestate);
    case 0x92:
        return parser_ParseCreatureImpassable(reader, gamestate);
    case 0x93:
        return parser_ParseCreaturePvPHelpers(reader, gamestate);
    case 0x94:
        return parser_ParseCreatureGuildMembersOnline(reader, gamestate);
    case 0x95:
        return parser_ParseCreatureType(reader, gamestate);
    case 0x96:
        return parser_ParseOpenEditText(reader, gamestate);
    case 0x97:
        return parser_ParseOpenHouseWindow(reader, gamestate);
    case 0x9C:
        return parser_ParseBlessings(reader, gamestate);
    case 0x9D:
        /* FIXME: This is also parser_ParseOpenEditList, version this! */
        return parser_ParseHotkeyPresets(reader, gamestate);
    case 0x9E:
        return parser_ParsePremiumTrigger(reader, gamestate);
    case 0x9F:
        return parser_ParsePlayerDataBasic(reader, gamestate);
    case 0xA0:
        return parser_ParsePlayerDataCurrent(reader, gamestate);
    case 0xA1:
        return parser_ParsePlayerSkills(reader, gamestate);
    case 0xA2:
        return parser_ParsePlayerIcons(reader, gamestate);
    case 0xA3:
        return parser_ParseCancelAttack(reader, gamestate);
    case 0xA4:
    case 0xA5:
        return parser_ParseSpellCooldown(reader, gamestate);
    case 0xA6:
        return parser_ParseUseCooldown(reader, gamestate);
    case 0xA7:
        return parser_ParsePlayerTactics(reader, gamestate);
    case 0xAA:
        return parser_ParseCreatureSpeak(reader, gamestate);
    case 0xAB:
        return parser_ParseChannelList(reader, gamestate);
    case 0xAC:
        /* Public channel */
        return parser_ParseChannelOpen(reader, gamestate);
    case 0xAD:
        return parser_ParseOpenPrivateConversation(reader, gamestate);
    case 0xAE:
    case 0xAF:
    case 0xB0:
        /* Rule-violation-related packet with two-byte payload. */
        return parser_ParseRuleViolation(reader, gamestate);
    case 0xB1:
        /* Single-byte rule-violation-related packet. */
        return true;
    case 0xB2:
        /* Private channel, identical to AC */
        return parser_ParseChannelOpen(reader, gamestate);
    case 0xB3:
        return parser_ParseChannelClose(reader, gamestate);
    case 0xB4:
        return parser_ParseTextMessage(reader, gamestate);
    case 0xB5:
        return parser_ParseMoveDenied(reader, gamestate);
    case 0xB6:
        return parser_ParseMoveDelay(reader, gamestate);
    case 0xB7:
        return parser_ParseUnjustifiedPoints(reader, gamestate);
    case 0xB8:
        return parser_ParseOpenPvPSituations(reader, gamestate);
    case 0xBE:
        return parser_ParseFloorChangeUp(reader, gamestate);
    case 0xBF:
        return parser_ParseFloorChangeDown(reader, gamestate);
    case 0xC8:
        return parser_ParseOutfitDialog(reader, gamestate);
    case 0xD2:
        return parser_ParseVIPStatus(reader, gamestate);
    case 0xD3:
        return parser_ParseVIPOnline(reader, gamestate);
    case 0xD4:
        return parser_ParseVIPOffline(reader, gamestate);
    case 0xDC:
        return parser_ParseTutorialShow(reader, gamestate);
    case 0xDD:
        return parser_ParseMinimapFlag(reader, gamestate);
    case 0xF0:
        return parser_ParseQuestDialog(reader, gamestate);
    case 0xF1:
        return parser_ParseQuestDialogMission(reader, gamestate);
    case 0xF2:
        return parser_ParseOffenseReportResponse(reader, gamestate);
    case 0xF3:
        return parser_ParseChannelEvent(reader, gamestate);
    case 0xF5:
        return parser_ParsePlayerInventory(reader, gamestate);
    case 0xF6:
        return parser_ParseMarketInitialization(reader, gamestate);
    case 0xF7:
        /* empty packet! */
        return true;
    case 0xF8:
        return parser_ParseMarketItemDetails(reader, gamestate);
    case 0xF9:
        return parser_ParseMarketBrowse(reader, gamestate);
    default:
        ParseAssert(!"Unhandled packet");
    }

    return false;
}
