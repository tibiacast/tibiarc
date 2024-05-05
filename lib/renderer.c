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

#include "renderer.h"
#include "versions.h"

#include "canvas.h"
#include "creature.h"
#include "effect.h"
#include "fonts.h"
#include "icons.h"
#include "missile.h"
#include "pictures.h"
#include "textrenderer.h"
#include "types.h"

#include <math.h>
#include <stdlib.h>

#include "utils.h"

#define IS_PLAYER_CREATURE(Id) ((Id) < 0x10000000)
#define MAX_HEIGHT_DISPLACEMENT 24

static void renderer_Convert8BitColor(uint8_t color, struct trc_pixel *result) {
    pixel_SetRGB(result,
                 ((color / 36) * 51),
                 (((color / 6) % 6) * 51),
                 ((color % 6) * 51));
}

static bool renderer_UpdateWalkOffset(struct trc_game_state *gamestate,
                                      struct trc_creature *creature) {
    if (creature->MovementInformation.LastUpdateTick < gamestate->CurrentTick) {
        uint32_t startTick, endTick;

        creature->MovementInformation.LastUpdateTick = gamestate->CurrentTick;

        startTick = creature->MovementInformation.WalkStartTick;
        endTick = creature->MovementInformation.WalkEndTick;
        ASSERT(startTick <= endTick);

        if ((endTick > gamestate->CurrentTick) &&
            ((endTick - startTick) != 0)) {
            float const walkProgress =
                    ((float)(gamestate->CurrentTick - startTick) /
                     (float)(endTick - startTick));

            creature->MovementInformation.WalkOffsetX =
                    (int)((creature->MovementInformation.Target.X -
                           creature->MovementInformation.Origin.X) *
                          (walkProgress - 1) * 32);
            creature->MovementInformation.WalkOffsetY =
                    (int)((creature->MovementInformation.Target.Y -
                           creature->MovementInformation.Origin.Y) *
                          (walkProgress - 1) * 32);
        } else {
            creature->MovementInformation.WalkOffsetX = 0;
            creature->MovementInformation.WalkOffsetY = 0;
        }
    }

    return true;
}

static bool renderer_GetCreatureInfoColor(int healthPercentage,
                                          int isObscured,
                                          struct trc_pixel *color) {
    if (isObscured) {
        pixel_SetRGB(color, 192, 192, 192);
    } else if (healthPercentage < 4) {
        pixel_SetRGB(color, 96, 0, 0);
    } else if (healthPercentage < 10) {
        pixel_SetRGB(color, 192, 0, 0);
    } else if (healthPercentage < 30) {
        pixel_SetRGB(color, 192, 48, 48);
    } else if (healthPercentage < 60) {
        pixel_SetRGB(color, 192, 192, 0);
    } else if (healthPercentage < 95) {
        pixel_SetRGB(color, 96, 192, 96);
    } else {
        pixel_SetRGB(color, 0, 192, 0);
    }

    return true;
}

static bool renderer_GetTileUnlookable(const struct trc_version *version,
                                       struct trc_tile *tile) {
    for (int objectIdx = 0; objectIdx < tile->ObjectCount; objectIdx++) {
        if (tile->Objects[objectIdx].Id != TILE_OBJECT_CREATURE) {
            struct trc_entitytype *itemType;

            ABORT_UNLESS(types_GetItem(version,
                                       tile->Objects[objectIdx].Id,
                                       &itemType));

            if (itemType->Properties.Unlookable) {
                return true;
            }
        }
    }

    return false;
}

static bool renderer_GetTileUpdateRenderHeight(
        const struct trc_version *version,
        struct trc_tile *tile) {
    for (int objectIdx = 0; objectIdx < tile->ObjectCount; objectIdx++) {
        if (tile->Objects[objectIdx].Id != TILE_OBJECT_CREATURE) {
            struct trc_entitytype *itemType;

            ABORT_UNLESS(types_GetItem(version,
                                       tile->Objects[objectIdx].Id,
                                       &itemType));

            if (!itemType->Properties.DontHide &&
                itemType->Properties.StackPriority == 0) {
                return true;
            }
        }
    }

    return false;
}

static bool renderer_GetTileBlocksPlayerVision(
        const struct trc_version *version,
        struct trc_tile *tile) {
    for (int objectIdx = 0; objectIdx < tile->ObjectCount; objectIdx++) {
        if (tile->Objects[objectIdx].Id != TILE_OBJECT_CREATURE) {
            struct trc_entitytype *itemType;

            ABORT_UNLESS(types_GetItem(version,
                                       tile->Objects[objectIdx].Id,
                                       &itemType));

            /* Things with a stack priority of 0 (ground) and 2 (some railings)
             * count as solids and cannot be seen through. */
            if (!itemType->Properties.DontHide &&
                (itemType->Properties.StackPriority == 0 ||
                 itemType->Properties.StackPriority == 2)) {
                return true;
            }
        }
    }

    return false;
}

static int renderer_GetTopVisibleFloor(struct trc_game_state *gamestate) {
    int minZ = 0;

    for (int xIdx = gamestate->Map.Position.X - 1;
         xIdx <= gamestate->Map.Position.X + 1;
         xIdx++) {
        for (int yIdx = gamestate->Map.Position.Y - 1;
             yIdx <= gamestate->Map.Position.Y + 1;
             yIdx++) {
            struct trc_tile *currentTile;

            currentTile = map_GetTile(&gamestate->Map,
                                      xIdx,
                                      yIdx,
                                      gamestate->Map.Position.Z);

            if (!renderer_GetTileUnlookable(gamestate->Version, currentTile) &&
                !(xIdx != gamestate->Map.Position.X &&
                  yIdx != gamestate->Map.Position.Y)) {
                for (int zIdx = gamestate->Map.Position.Z - 1; zIdx >= minZ;
                     zIdx--) {
                    currentTile = map_GetTile(
                            &gamestate->Map,
                            xIdx + (gamestate->Map.Position.Z - zIdx),
                            yIdx + (gamestate->Map.Position.Z - zIdx),
                            zIdx);

                    if (renderer_GetTileBlocksPlayerVision(gamestate->Version,
                                                           currentTile)) {
                        minZ = zIdx + 1;
                        break;
                    }

                    currentTile =
                            map_GetTile(&gamestate->Map, xIdx, yIdx, zIdx);

                    if (renderer_GetTileBlocksPlayerVision(gamestate->Version,
                                                           currentTile)) {
                        minZ = zIdx + 1;
                        break;
                    }
                }
            }
        }
    }

    return minZ;
}

static bool renderer_DrawTypeBounded(const struct trc_version *version,
                                     struct trc_framegroup *frameGroup,
                                     int rightX,
                                     int bottomY,
                                     int layer,
                                     int xMod,
                                     int yMod,
                                     int zMod,
                                     int frame,
                                     int maxWidth,
                                     int maxHeight,
                                     struct trc_canvas *canvas) {
    int heightLeft, widthLeft;
    unsigned spriteIndex;

    spriteIndex = (layer + (xMod + (yMod + (zMod + (frame)*frameGroup->ZDiv) *
                                                   frameGroup->YDiv) *
                                           frameGroup->XDiv) *
                                   frameGroup->LayerCount);
    spriteIndex *= (frameGroup->SizeX * frameGroup->SizeY);

    heightLeft = maxHeight;
    widthLeft = maxWidth;

    for (int yIdx = 0; yIdx < frameGroup->SizeY && heightLeft > 0; yIdx++) {
        const int adjustedHeight = MIN(32, heightLeft);

        for (int xIdx = 0; xIdx < frameGroup->SizeX && widthLeft > 0; xIdx++) {
            const int adjustedWidth = MIN(32, widthLeft);
            struct trc_sprite *sprite;

            /* Not an error; creatures like bears, for instance, only have 2
             * sprites in each direction, and we're not supposed to draw the
             * empty sprites. */
            if (spriteIndex >= frameGroup->SpriteCount) {
                return true;
            }

            if (!sprites_GetObjectSprite(version,
                                         frameGroup->SpriteIds[spriteIndex],
                                         &sprite)) {
                return trc_ReportError("Failed to get sprite");
            }

            canvas_Draw(canvas,
                        sprite,
                        rightX - xIdx * 32 - adjustedWidth,
                        bottomY - yIdx * 32 - adjustedHeight,
                        adjustedWidth,
                        adjustedHeight);

            widthLeft -= MIN(32, adjustedWidth);
            spriteIndex++;
        }

        heightLeft -= MIN(32, adjustedHeight);
    }

    return true;
}

static bool renderer_DrawType(const struct trc_version *version,
                              struct trc_framegroup *frameGroup,
                              int rightX,
                              int bottomY,
                              int layer,
                              int xMod,
                              int yMod,
                              int zMod,
                              int frame,
                              struct trc_canvas *canvas) {
    unsigned spriteIndex;

    spriteIndex = (layer + (xMod + (yMod + (zMod + frame * frameGroup->ZDiv) *
                                                   frameGroup->YDiv) *
                                           frameGroup->XDiv) *
                                   frameGroup->LayerCount);
    spriteIndex *= (frameGroup->SizeX * frameGroup->SizeY);

    for (int yIdx = 0; yIdx < frameGroup->SizeY; yIdx++) {
        for (int xIdx = 0; xIdx < frameGroup->SizeX; xIdx++) {
            struct trc_sprite *sprite;

            if (spriteIndex >= frameGroup->SpriteCount) {
                return true;
            }

            if (!sprites_GetObjectSprite(version,
                                         frameGroup->SpriteIds[spriteIndex],
                                         &sprite)) {
                return trc_ReportError("Failed to get sprite");
            }

            canvas_Draw(canvas,
                        sprite,
                        rightX - xIdx * 32 - 32,
                        bottomY - yIdx * 32 - 32,
                        32,
                        32);

            spriteIndex++;
        }
    }

    return true;
}

static bool renderer_TintType(const struct trc_version *version,
                              struct trc_framegroup *frameGroup,
                              int head,
                              int primary,
                              int secondary,
                              int detail,
                              int rightX,
                              int bottomY,
                              int layer,
                              int xMod,
                              int yMod,
                              int zMod,
                              int frame,
                              struct trc_canvas *canvas) {
    unsigned spriteIndex;

    spriteIndex = (layer + (xMod + (yMod + (zMod + frame * frameGroup->ZDiv) *
                                                   frameGroup->YDiv) *
                                           frameGroup->XDiv) *
                                   frameGroup->LayerCount);
    spriteIndex *= (frameGroup->SizeX * frameGroup->SizeY);

    for (int yIdx = 0; yIdx < frameGroup->SizeY; yIdx++) {
        for (int xIdx = 0; xIdx < frameGroup->SizeX; xIdx++) {
            struct trc_sprite *sprite;

            if (!sprites_GetObjectSprite(version,
                                         frameGroup->SpriteIds[spriteIndex],
                                         &sprite)) {
                return trc_ReportError("Failed to get sprite");
            }

            canvas_Tint(canvas,
                        sprite,
                        rightX - xIdx * 32 - 32,
                        bottomY - yIdx * 32 - 32,
                        32,
                        32,
                        head,
                        primary,
                        secondary,
                        detail);

            spriteIndex++;
        }
    }

    return true;
}

static bool renderer_DrawGraphicalEffect(const struct trc_version *version,
                                         struct trc_graphical_effect *effect,
                                         struct trc_position *position,
                                         int rightX,
                                         int bottomY,
                                         uint32_t tick,
                                         struct trc_canvas *canvas) {
    if (effect->Id > 0) {
        struct trc_entitytype *effectType;
        struct trc_framegroup *frameGroup;

        if (!types_GetEffect(version, effect->Id, &effectType)) {
            return trc_ReportError("Failed to get effect type");
        }

        frameGroup = &effectType->FrameGroups[FRAME_GROUP_DEFAULT];

        if ((effect->StartTick + 100 * frameGroup->FrameCount) > tick) {
            unsigned frame = MIN((tick - effect->StartTick) / 100,
                                 (unsigned)frameGroup->FrameCount - 1);

            for (int layerIdx = 0; layerIdx < frameGroup->LayerCount;
                 layerIdx++) {
                if (!renderer_DrawType(version,
                                       frameGroup,
                                       rightX,
                                       bottomY,
                                       layerIdx,
                                       position->X % frameGroup->XDiv,
                                       position->Y % frameGroup->YDiv,
                                       position->Z % frameGroup->ZDiv,
                                       frame,
                                       canvas)) {
                    return trc_ReportError("Failed to render effect layer");
                }
            }
        }
    }

    return true;
}

static bool renderer_DrawMissile(const struct trc_version *version,
                                 struct trc_missile *missile,
                                 int rightX,
                                 int bottomY,
                                 uint32_t tick,
                                 struct trc_canvas *canvas) {
    struct trc_entitytype *missileType;
    struct trc_framegroup *frameGroup;
    float directionalRatio;
    float deltaX, deltaY;
    int direction;

    deltaX = (float)(missile->Origin.X - missile->Target.X);
    deltaY = (float)(missile->Origin.Y - missile->Target.Y);

    if (!types_GetMissile(version, missile->Id, &missileType)) {
        return trc_ReportError("Failed to get missile type");
    }

    frameGroup = &missileType->FrameGroups[FRAME_GROUP_DEFAULT];

    directionalRatio = (deltaX == 0) ? 10.0F : (deltaY / deltaX);

    if (fabs(directionalRatio) < 0.4142F) {
        direction = (deltaX > 0) ? 3 : 5; /* West or east */
    } else if (fabs(directionalRatio) < 2.4242F) {
        if (directionalRatio <= 0) {
            direction = (deltaY > 0) ? 2 : 6; /* Southwest or northeast */
        } else {
            direction = (deltaY > 0) ? 0 : 8; /* Northwest or southeast */
        }
    } else {
        direction = (deltaY > 0) ? 1 : 7; /* North or south */
    }

    for (int layerIdx = 0; layerIdx < frameGroup->LayerCount; layerIdx++) {
        if (!renderer_DrawType(version,
                               frameGroup,
                               rightX,
                               bottomY,
                               layerIdx,
                               direction % frameGroup->XDiv,
                               (direction / frameGroup->XDiv) %
                                       frameGroup->YDiv,
                               0,
                               0,
                               canvas)) {
            return trc_ReportError("Failed to write missile layer");
        }
    }

    return true;
}

static bool renderer_DrawOutfit(const struct trc_version *version,
                                struct trc_creature *creature,
                                struct trc_entitytype *outfitType,
                                int isMounted,
                                int rightX,
                                int bottomY,
                                uint32_t tick,
                                struct trc_canvas *canvas) {
    int actualDirection, frame = 0;
    struct trc_framegroup *frameGroup;

    actualDirection = creature->Direction;

    if (creature->MovementInformation.WalkEndTick > tick) {
        frameGroup = &outfitType->FrameGroups[FRAME_GROUP_WALKING];
    } else {
        frameGroup = &outfitType->FrameGroups[FRAME_GROUP_IDLE];
    }

    if (!isMounted) {
        /* Mounted outfits do not use any offsets. */
        rightX -= outfitType->Properties.DisplacementX;
        bottomY -= outfitType->Properties.DisplacementY;
    }

    if (outfitType->Properties.AnimateIdle) {
        frame = (tick / 500) % frameGroup->FrameCount;
    } else {
        if (creature->MovementInformation.WalkEndTick > tick) {
            int xDifference, yDifference;

            /* When a creature has less than 3 frames, the first is used for
             * animation, otherwise it isn't. */
            if (frameGroup->FrameCount <= 2) {
                frame = ((creature->MovementInformation.WalkStartTick - tick) /
                         100) %
                        frameGroup->FrameCount;
            } else {
                frame = (tick / 100) % (frameGroup->FrameCount - 1) + 1;
            }

            /* In case a creature's direction gets updated while walking (Fairly
             * common), we don't want to see the thing moonwalk. */
            xDifference = creature->MovementInformation.Target.X -
                          creature->MovementInformation.Origin.X;
            yDifference = creature->MovementInformation.Target.Y -
                          creature->MovementInformation.Origin.Y;

            if (yDifference < 0) {
                actualDirection = 0;
            } else if (yDifference > 0) {
                actualDirection = 2;
            }

            if (xDifference < 0) {
                actualDirection = 3;
            } else if (xDifference > 0) {
                actualDirection = 1;
            }
        }
    }

    for (int addonIdx = 0; addonIdx < frameGroup->YDiv; addonIdx++) {
        if ((addonIdx == 0) ||
            (creature->Outfit.Addons & (1 << (addonIdx - 1)))) {
            if (!renderer_DrawType(version,
                                   frameGroup,
                                   rightX,
                                   bottomY,
                                   0,
                                   actualDirection,
                                   addonIdx,
                                   isMounted,
                                   isMounted ? (frame % 3) : frame,
                                   canvas)) {
                return trc_ReportError("Failed to draw outfit sprite");
            }

            if (frameGroup->LayerCount == 2) {
                if (!renderer_TintType(version,
                                       frameGroup,
                                       creature->Outfit.HeadColor,
                                       creature->Outfit.PrimaryColor,
                                       creature->Outfit.SecondaryColor,
                                       creature->Outfit.DetailColor,
                                       rightX,
                                       bottomY,
                                       1,
                                       actualDirection,
                                       addonIdx,
                                       isMounted,
                                       isMounted ? (frame % 3) : frame,
                                       canvas)) {
                    return trc_ReportError("Failed to tint outfit sprite");
                }
            }
        }
    }

    return true;
}

static bool renderer_DrawItem(const struct trc_version *version,
                              struct trc_object *item,
                              int rightX,
                              int bottomY,
                              uint32_t tick,
                              struct trc_position *position,
                              int horizontal,
                              int vertical,
                              int isInInventory,
                              struct trc_canvas *canvas) {
    struct trc_framegroup *frameGroup;
    struct trc_entitytype *itemType;
    int frame, xMod, yMod, zMod;

    if (!types_GetItem(version, item->Id, &itemType)) {
        return trc_ReportError("Failed to get item type");
    }

    frameGroup = &itemType->FrameGroups[FRAME_GROUP_DEFAULT];

    if (itemType->Properties.Animated) {
        /* Phases were introduced in 10.50, otherwise let each animation frame
         * take 500ms. */
        if (VERSION_AT_LEAST(version, 10, 50)) {
            /* FIXME: This is a lazy and incorrect implementation as the
             * animation time is effectively reset every time an item bounces
             * in and out of the viewport. I remember it being especially
             * noticeable with magic walls back in the day, but didn't have the
             * time to implement it properly back then. Revisit this once we've
             * got some >=10.50 recordings to test with. */
            uint32_t minTime = frameGroup->Phases[item->Animation].Minimum;

            if (tick >= (item->PhaseTick + minTime)) {
                item->Animation =
                        (item->Animation + 1) % frameGroup->FrameCount;
                item->PhaseTick = tick;
            }

            frame = item->Animation;
        } else {
            frame = (tick / 500) % frameGroup->FrameCount;
        }
    } else {
        frame = 0;
    }

    /* For some reason, items in inventory ignore the displacement of their
     * types. */
    if (!isInInventory) {
        rightX -= itemType->Properties.DisplacementX;
        bottomY -= itemType->Properties.DisplacementY;
    }

    if (itemType->Properties.Hangable) {
        int picture = 0;

        if (vertical) {
            picture = 1;
        } else if (horizontal) {
            picture = 2;
        }

        xMod = picture % frameGroup->XDiv;
        yMod = 0;
        zMod = 0;
    } else if (itemType->Properties.LiquidContainer ||
               itemType->Properties.LiquidPool) {
        int picture = 0;

        if (!version_TranslateFluidColor(version, item->ExtraByte, &picture)) {
            return trc_ReportError("Failed to translate fluid color");
        }

        xMod = picture % frameGroup->XDiv;
        yMod = (picture / frameGroup->XDiv) % frameGroup->YDiv;
        zMod = 0;
    } else if (itemType->Properties.Stackable) {
        int picture;

        if (item->ExtraByte == 0) {
            picture = 0;
        } else if (item->ExtraByte < 5) {
            picture = item->ExtraByte - 1;
        } else if (item->ExtraByte < 10) {
            picture = 4;
        } else if (item->ExtraByte < 25) {
            picture = 5;
        } else if (item->ExtraByte < 50) {
            picture = 6;
        } else {
            picture = 7;
        }

        xMod = picture % frameGroup->XDiv;
        yMod = (picture / frameGroup->XDiv) % frameGroup->YDiv;
        zMod = 0;
    } else {
        xMod = position->X % frameGroup->XDiv;
        yMod = position->Y % frameGroup->YDiv;
        zMod = position->Z % frameGroup->ZDiv;
    }

    for (int layerIdx = 0; layerIdx < frameGroup->LayerCount; layerIdx++) {
        if (isInInventory) {
            if (!renderer_DrawTypeBounded(version,
                                          frameGroup,
                                          rightX,
                                          bottomY,
                                          layerIdx,
                                          xMod,
                                          yMod,
                                          zMod,
                                          frame,
                                          32,
                                          32,
                                          canvas)) {
                return trc_ReportError("Failed to draw item (inventory)");
            }
        } else {
            if (!renderer_DrawType(version,
                                   frameGroup,
                                   rightX,
                                   bottomY,
                                   layerIdx,
                                   xMod,
                                   yMod,
                                   zMod,
                                   frame,
                                   canvas)) {
                return trc_ReportError("Failed to draw item");
            }
        }
    }

    return true;
}

static bool renderer_DrawCreature(const struct trc_version *version,
                                  struct trc_creature *creature,
                                  int rightX,
                                  int bottomY,
                                  uint32_t tick,
                                  struct trc_canvas *canvas) {
    if (creature->Outfit.Id == 0) {
        if (creature->Outfit.Item.Id != 0) {
            /* Render item */
            struct trc_position position;
            struct trc_object item;

            item.Id = creature->Outfit.Item.Id;
            item.ExtraByte = creature->Outfit.Item.ExtraByte;

            position.Y = 0;
            position.Y = 0;
            position.Z = 0;

            if (!renderer_DrawItem(version,
                                   &item,
                                   rightX,
                                   bottomY,
                                   tick,
                                   &position,
                                   0,
                                   0,
                                   0,
                                   canvas)) {
                return trc_ReportError("Failed to draw creature as item");
            }
        } else if (IS_PLAYER_CREATURE(creature->Id)) {
            /* Invisible players should be rendered as a shimmer, while
             * invisible monsters are ignored altogether. */
            struct trc_position nullPosition;
            struct trc_graphical_effect shimmerEffect;

            shimmerEffect.Id = 0x0D;

            nullPosition.X = 0;
            nullPosition.Y = 0;
            nullPosition.Z = 0;

            /* Draw the effect at the default creature offset */
            rightX -= 8;
            bottomY -= 8;

            /* TODO: find out the proper cycling. */
            shimmerEffect.StartTick = MAX(0, tick - 500);

            if (!renderer_DrawGraphicalEffect(version,
                                              &shimmerEffect,
                                              &nullPosition,
                                              rightX,
                                              bottomY,
                                              MAX(0, tick - tick % 500),
                                              canvas)) {
                return trc_ReportError("Failed to draw invisibility shimmer");
            }
        }
    } else {
        struct trc_entitytype *outfitType;

        if (!types_GetOutfit(version, creature->Outfit.Id, &outfitType)) {
            return trc_ReportError("Failed to get creature outfit");
        }

        if (creature->Outfit.MountId == 0) {
            if (!renderer_DrawOutfit(version,
                                     creature,
                                     outfitType,
                                     0,
                                     rightX,
                                     bottomY,
                                     tick,
                                     canvas)) {
                return trc_ReportError("Failed to draw creature outfit");
            }
        } else {
            struct trc_entitytype *mountType;

            if (!types_GetOutfit(version,
                                 creature->Outfit.MountId,
                                 &mountType)) {
                return trc_ReportError(
                        "Failed to get mount outfit of creature");
            }

            if (!renderer_DrawOutfit(version,
                                     creature,
                                     mountType,
                                     0,
                                     rightX,
                                     bottomY,
                                     tick,
                                     canvas)) {
                return trc_ReportError("Failed to draw mount outfit");
            }

            if (!renderer_DrawOutfit(version,
                                     creature,
                                     outfitType,
                                     1,
                                     rightX,
                                     bottomY,
                                     tick,
                                     canvas)) {
                return trc_ReportError("Failed to draw creature outfit");
            }
        }
    }

    return true;
}

static bool renderer_DrawMovingCreatures(struct trc_game_state *gamestate,
                                         struct trc_position *position,
                                         int heightDisplacement,
                                         int rightX,
                                         int bottomY,
                                         uint32_t tick,
                                         struct trc_canvas *canvas) {
    /* Check for and draw creatures which might overlap with this tile. */
    for (int yIdx = -1; yIdx <= 1; yIdx++) {
        for (int xIdx = -1; xIdx <= 1; xIdx++) {
            struct trc_tile *neighborTile = map_GetTile(&gamestate->Map,
                                                        position->X + xIdx,
                                                        position->Y + yIdx,
                                                        position->Z);

            for (int objectIdx = 0; objectIdx < neighborTile->ObjectCount;
                 objectIdx++) {
                if (neighborTile->Objects[objectIdx].Id ==
                    TILE_OBJECT_CREATURE) {
                    int offsetRelativeThisX, offsetRelativeThisY;
                    struct trc_creature *creature;

                    if (!creaturelist_GetCreature(
                                &gamestate->CreatureList,
                                neighborTile->Objects[objectIdx].CreatureId,
                                &creature)) {
                        continue;
                    }

                    if (!renderer_UpdateWalkOffset(gamestate, creature)) {
                        return trc_ReportError(
                                "Could not update walk offset of "
                                "creature");
                    }

                    if (creature->MovementInformation.WalkEndTick <= tick) {
                        continue;
                    }

                    /* Walk offset is relative to the owning tile, translate to
                     * make it relative this tile */
                    offsetRelativeThisX =
                            creature->MovementInformation.WalkOffsetX +
                            xIdx * 32;
                    offsetRelativeThisY =
                            creature->MovementInformation.WalkOffsetY +
                            yIdx * 32;

                    if ((offsetRelativeThisX <= 0) &&
                        (offsetRelativeThisY <= 0) &&
                        (offsetRelativeThisX >= -31) &&
                        (offsetRelativeThisY >= -31)) {
                        if (!renderer_DrawCreature(gamestate->Version,
                                                   creature,
                                                   rightX - heightDisplacement +
                                                           offsetRelativeThisX,
                                                   bottomY -
                                                           heightDisplacement +
                                                           offsetRelativeThisY,
                                                   tick,
                                                   canvas)) {
                            return trc_ReportError("Failed to draw moving "
                                                   "creature");
                        }
                    }
                }
            }
        }
    }

    return true;
}

static bool renderer_DrawMissiles(struct trc_game_state *gamestate,
                                  struct trc_position *position,
                                  int heightDisplacement,
                                  int rightX,
                                  int bottomY,
                                  uint32_t tick,
                                  struct trc_canvas *canvas) {
    /* Check for and draw all projectiles which might overlap with this
     * tile. */
    unsigned missileIdx = gamestate->MissileIndex;

    do {
        struct trc_missile *missile;
        uint32_t endTick;

        missileIdx = (missileIdx - 1) % MAX_MISSILES_IN_GAMESTATE;

        missile = &gamestate->MissileList[missileIdx];
        endTick = missile->StartTick + 200;

        if (endTick < tick) {
            break;
        } else if (missile->Id != 0 && missile->Origin.Z == position->Z) {
            struct trc_entitytype *missileType;

            if (!types_GetMissile(gamestate->Version,
                                  missile->Id,
                                  &missileType)) {
                return trc_ReportError("Tried to draw an invalid missile type");
            } else {
                int relativeThisX, relativeThisY, globalX, globalY;
                float progress;

                progress =
                        (endTick != missile->StartTick)
                                ? (float)(tick - missile->StartTick) /
                                          (float)(endTick - missile->StartTick)
                                : 0.0f;

                globalX = (int)((missile->Origin.X +
                                 (missile->Target.X - missile->Origin.X) *
                                         progress) *
                                32);
                globalY = (int)((missile->Origin.Y +
                                 (missile->Target.Y - missile->Origin.Y) *
                                         progress) *
                                32);
                relativeThisX = globalX - position->X * 32;
                relativeThisY = globalY - position->Y * 32;

                if ((relativeThisX <= 0) && (relativeThisY <= 0) &&
                    (relativeThisX >= -31) && (relativeThisY >= -31)) {
                    renderer_DrawMissile(
                            gamestate->Version,
                            missile,
                            rightX - heightDisplacement + relativeThisX,
                            bottomY - heightDisplacement + relativeThisY,
                            tick,
                            canvas);
                }
            }
        }
    } while (missileIdx != gamestate->MissileIndex);

    return true;
}

static bool renderer_DrawTile(const struct trc_render_options *options,
                              struct trc_game_state *gamestate,
                              struct trc_position *position,
                              int viewOffsetX,
                              int viewOffsetY,
                              uint32_t tick,
                              int *redrawNearbyTop,
                              struct trc_canvas *canvas) {
    int heightDisplacement, horizontal, vertical, rightX, bottomY;
    struct trc_tile *tile;

    heightDisplacement = 0;
    horizontal = 0;
    vertical = 0;

    rightX = position->X * 32 + viewOffsetX;
    bottomY = position->Y * 32 + viewOffsetY;

    tile = map_GetTile(&gamestate->Map, position->X, position->Y, position->Z);

    if (tile->ObjectCount > 0 &&
        renderer_GetTileUpdateRenderHeight(gamestate->Version, tile)) {
        map_UpdateRenderHeight(&gamestate->Map, rightX, bottomY, position->Z);
    }

    if (*redrawNearbyTop) {
        /* We're only supposed to redraw the top items, so just calculate the
         * proper height displacement and then get on with it. */
        for (int objectIdx = 0; objectIdx < tile->ObjectCount; objectIdx++) {
            if (tile->Objects[objectIdx].Id == TILE_OBJECT_CREATURE) {
                // objectIdx = tile->creatureEndIndex;
                continue;
            } else {
                struct trc_entitytype *itemType;
                struct trc_object *item;

                item = &tile->Objects[objectIdx];

                if (!types_GetItem(gamestate->Version, item->Id, &itemType)) {
                    return trc_ReportError("Could not get item type");
                }

                if (itemType->Properties.StackPriority != 3) {
                    heightDisplacement = MIN(
                            MAX_HEIGHT_DISPLACEMENT,
                            heightDisplacement + itemType->Properties.Height);
                    horizontal = horizontal || itemType->Properties.Horizontal;
                    vertical = vertical || itemType->Properties.Vertical;
                }
            }
        }
    } else {
        /* Draw things as they were sent by the Tibia server until we hit a
         * creature or stack priority 5.
         *
         * This is not pixel-perfect but it's the only method that offers decent
         * performance in areas with a ton of crap on the ground. */
        for (int objectIdx = 0;
             objectIdx < tile->ObjectCount &&
             tile->Objects[objectIdx].Id != TILE_OBJECT_CREATURE;
             objectIdx++) {
            struct trc_entitytype *itemType;
            struct trc_object *item;

            item = &tile->Objects[objectIdx];

            if (!types_GetItem(gamestate->Version, item->Id, &itemType)) {
                return trc_ReportError(
                        "Could not get type of low-priority item");
            }

            if (itemType->Properties.StackPriority > 2) {
                break;
            }

            if (!renderer_DrawItem(gamestate->Version,
                                   item,
                                   rightX - heightDisplacement,
                                   bottomY - heightDisplacement,
                                   tick,
                                   position,
                                   horizontal,
                                   vertical,
                                   0,
                                   canvas)) {
                return trc_ReportError("Failed to draw low-priority item");
            }

            heightDisplacement =
                    MIN(MAX_HEIGHT_DISPLACEMENT,
                        heightDisplacement + itemType->Properties.Height);
            horizontal = horizontal || itemType->Properties.Horizontal;
            vertical = vertical || itemType->Properties.Vertical;
        }

        if (!options->SkipRenderingItems) {
            /* Draw stack priority 5. Unlike the other priorities, its items are
             * to be drawn in reversed order.
             *
             * We can skip drawing once we hit a creature or an item with a
             * lower stack priority is found, and since the Tibia server will
             * ALWAYS handle stack priority 5 correctly this is pixel-perfect
             * by default. */
            for (int objectIdx = tile->ObjectCount - 1;
                 objectIdx >= 0 &&
                 tile->Objects[objectIdx].Id != TILE_OBJECT_CREATURE;
                 objectIdx--) {
                struct trc_entitytype *itemType;
                struct trc_object *item;

                item = &tile->Objects[objectIdx];

                if (!types_GetItem(gamestate->Version, item->Id, &itemType)) {
                    return trc_ReportError("Could not get type of high-"
                                           "priority item");
                }

                if (itemType->Properties.StackPriority != 5) {
                    break;
                }

                if (!renderer_DrawItem(gamestate->Version,
                                       item,
                                       rightX - heightDisplacement,
                                       bottomY - heightDisplacement,
                                       tick,
                                       position,
                                       horizontal,
                                       vertical,
                                       0,
                                       canvas)) {
                    return trc_ReportError("Failed to draw high-priority item");
                }

                heightDisplacement =
                        MIN(MAX_HEIGHT_DISPLACEMENT,
                            heightDisplacement + itemType->Properties.Height);
                horizontal = horizontal || itemType->Properties.Horizontal;
                vertical = vertical || itemType->Properties.Vertical;

                if (itemType->Properties.RedrawNearbyTop) {
                    (*redrawNearbyTop) = 1;
                }
            }
        }

        if (!options->SkipRenderingCreatures) {
            if (!renderer_DrawMovingCreatures(gamestate,
                                              position,
                                              heightDisplacement,
                                              rightX,
                                              bottomY,
                                              tick,
                                              canvas)) {
                return trc_ReportError("Could not draw moving creatures");
            }
        }
    }

    if (!options->SkipRenderingCreatures) {
        /* Draw creatures that are standing on this tile. */
        for (int objectIdx = 0; objectIdx < tile->ObjectCount; objectIdx++) {
            if (tile->Objects[objectIdx].Id == TILE_OBJECT_CREATURE) {
                struct trc_creature *creature;

                if (!creaturelist_GetCreature(
                            &gamestate->CreatureList,
                            tile->Objects[objectIdx].CreatureId,
                            &creature)) {
                    continue;
                }

                if (!renderer_UpdateWalkOffset(gamestate, creature)) {
                    return trc_ReportError("Could not update walk offset of "
                                           "creature");
                }

                if (creature->MovementInformation.WalkEndTick <= tick) {
                    if (!renderer_DrawCreature(
                                gamestate->Version,
                                creature,
                                rightX - heightDisplacement +
                                        creature->MovementInformation
                                                .WalkOffsetX,
                                bottomY - heightDisplacement +
                                        creature->MovementInformation
                                                .WalkOffsetY,
                                tick,
                                canvas)) {
                        return trc_ReportError("Failed to draw stationary "
                                               "creature");
                    }
                }
            }
        }
    }

    if (!options->SkipRenderingGraphicalEffects) {
        /* Render all effects on current tile. */
        for (int effectIdx = 0; effectIdx < MAX_EFFECTS_PER_TILE; effectIdx++) {
            struct trc_graphical_effect *effect =
                    &tile->GraphicalEffects[effectIdx];

            if (effect->Id > 0) {
                struct trc_entitytype *effectType;

                if (!types_GetEffect(gamestate->Version,
                                     effect->Id,
                                     &effectType)) {
                    return trc_ReportError("Tried to draw an invalid effect "
                                           "type");
                }

                renderer_DrawGraphicalEffect(gamestate->Version,
                                             effect,
                                             position,
                                             rightX - heightDisplacement,
                                             bottomY - heightDisplacement,
                                             tick,
                                             canvas);
            }
        }
    }

    if (!options->SkipRenderingMissiles) {
        if (!renderer_DrawMissiles(gamestate,
                                   position,
                                   heightDisplacement,
                                   rightX,
                                   bottomY,
                                   tick,
                                   canvas)) {
            return trc_ReportError("Failed to render missiles");
        }
    }

    if (!options->SkipRenderingItems) {
        /* Draw stack priority 3. */
        for (int objectIdx = 0;
             objectIdx < tile->ObjectCount &&
             tile->Objects[objectIdx].Id != TILE_OBJECT_CREATURE;
             objectIdx++) {
            struct trc_entitytype *itemType;
            struct trc_object *item;

            item = &tile->Objects[objectIdx];

            if (!types_GetItem(gamestate->Version, item->Id, &itemType)) {
                return trc_ReportError("Could not get item type");
            }

            if (itemType->Properties.StackPriority > 3) {
                break;
            } else if (itemType->Properties.StackPriority == 3) {
                if (!renderer_DrawItem(gamestate->Version,
                                       item,
                                       rightX,
                                       bottomY,
                                       tick,
                                       position,
                                       horizontal,
                                       vertical,
                                       0,
                                       canvas)) {
                    return trc_ReportError("Failed to draw low-priority item");
                }

                horizontal = horizontal || itemType->Properties.Horizontal;
                vertical = vertical || itemType->Properties.Vertical;
            }
        }
    }

    return true;
}

static bool renderer_DrawInventoryItem(struct trc_game_state *gamestate,
                                       struct trc_object *item,
                                       int X,
                                       int Y,
                                       struct trc_canvas *canvas) {
    const struct trc_icons *icons = &(gamestate->Version)->Icons;
    const struct trc_fonts *fonts = &(gamestate->Version)->Fonts;
    struct trc_position inventoryPosition;

    inventoryPosition.X = 0xFFFF;
    inventoryPosition.Y = 0;
    inventoryPosition.Z = 0;

    canvas_Draw(canvas,
                &icons->InventoryBackground,
                X,
                Y,
                icons->InventoryBackground.Width,
                icons->InventoryBackground.Height);

    if (item->Id != 0) {
        struct trc_entitytype *itemType;

        if (!renderer_DrawItem(gamestate->Version,
                               item,
                               X + 32,
                               Y + 32,
                               gamestate->CurrentTick,
                               &inventoryPosition,
                               0,
                               0,
                               1,
                               canvas)) {
            return trc_ReportError("Could not draw inventory item");
        }

        if (!types_GetItem(gamestate->Version, item->Id, &itemType)) {
            return trc_ReportError("Could not get type of inventory item");
        }

        if (itemType->Properties.Stackable && item->ExtraByte > 1) {
            struct trc_pixel foregroundColor;

            uint16_t textLength;
            char textBuffer[32];

            pixel_SetRGB(&foregroundColor, 0xBF, 0xBF, 0xBF);

            textLength = (uint16_t)snprintf(textBuffer,
                                            sizeof(textBuffer),
                                            "%hhu",
                                            item->ExtraByte);
            textrenderer_DrawRightAlignedString(&fonts->GameFont,
                                                &foregroundColor,
                                                X + 32,
                                                Y + 22,
                                                textLength,
                                                textBuffer,
                                                canvas);
        }
    }

    return true;
}

static bool renderer_DrawInventorySlot(struct trc_game_state *gamestate,
                                       enum TrcInventorySlot slot,
                                       int X,
                                       int Y,
                                       struct trc_canvas *canvas) {
    struct trc_object *inventoryItem =
            player_GetInventoryObject(&gamestate->Player, slot);

    if (!renderer_DrawInventoryItem(gamestate, inventoryItem, X, Y, canvas)) {
        return trc_ReportError("Could not draw inventory item for slot %u.",
                               slot);
    }

    if (inventoryItem->Id == 0) {
        const struct trc_sprite *emptyInventorySprite =
                icons_GetEmptyInventory(gamestate->Version, slot);

        canvas_Draw(canvas,
                    emptyInventorySprite,
                    X,
                    Y,
                    emptyInventorySprite->Width,
                    emptyInventorySprite->Height);
    }

    return true;
}

bool renderer_DrawGamestate(const struct trc_render_options *options,
                            struct trc_game_state *gamestate,
                            struct trc_canvas *canvas) {
    int bottomVisibleFloor, topVisibleFloor;
    int viewOffsetX, viewOffsetY;

    struct trc_creature *playerCreature;

    if (!creaturelist_GetCreature(&gamestate->CreatureList,
                                  gamestate->Player.Id,
                                  &playerCreature)) {
        return trc_ReportError("Failed to get the player creature");
    }

    /* Force a small amount of light around the player like the Tibia client
     * does. */
    playerCreature->LightIntensity = MAX(playerCreature->LightIntensity, 1);

    if (!renderer_UpdateWalkOffset(gamestate, playerCreature)) {
        return trc_ReportError("Failed to update walk offset of "
                               "playerCreature");
    }

    viewOffsetX = (8 - gamestate->Map.Position.X) * 32 -
                  playerCreature->MovementInformation.WalkOffsetX;
    viewOffsetY = (6 - gamestate->Map.Position.Y) * 32 -
                  playerCreature->MovementInformation.WalkOffsetY;

    if (gamestate->Map.Position.Z > 7) {
        bottomVisibleFloor = MIN(15, gamestate->Map.Position.Z + 2);
        topVisibleFloor = gamestate->Map.Position.Z;
    } else {
        if (!options->SkipRenderingUpperFloors) {
            topVisibleFloor = renderer_GetTopVisibleFloor(gamestate);
        } else {
            topVisibleFloor = gamestate->Map.Position.Z;
        }

        bottomVisibleFloor = 7;
    }

    for (int zIdx = bottomVisibleFloor; zIdx >= topVisibleFloor; zIdx--) {
        int xyOffset = gamestate->Map.Position.Z - zIdx;

        for (int xIdx = 0; xIdx <= 17; xIdx++) {
            for (int yIdx = 0; yIdx <= 13; yIdx++) {
                struct trc_position tilePosition;
                int redrawNearbyTop;

                tilePosition.X =
                        gamestate->Map.Position.X - 8 + xIdx + xyOffset;
                tilePosition.Y =
                        gamestate->Map.Position.Y - 6 + yIdx + xyOffset;
                tilePosition.Z = zIdx;

                redrawNearbyTop = 0;

                if (!renderer_DrawTile(options,
                                       gamestate,
                                       &tilePosition,
                                       viewOffsetX - xyOffset * 32,
                                       viewOffsetY - xyOffset * 32,
                                       gamestate->CurrentTick,
                                       &redrawNearbyTop,
                                       canvas)) {
                    return trc_ReportError("Failed to draw a tile");
                }

                if (redrawNearbyTop) {
                    if (xIdx > 0) {
                        tilePosition.X--;

                        if (!renderer_DrawTile(options,
                                               gamestate,
                                               &tilePosition,
                                               viewOffsetX - xyOffset * 32,
                                               viewOffsetY - xyOffset * 32,
                                               gamestate->CurrentTick,
                                               &redrawNearbyTop,
                                               canvas)) {
                            return trc_ReportError("Failed to redraw a tile");
                        }
                    }

                    if (yIdx > 0) {
                        tilePosition.Y--;

                        if (!renderer_DrawTile(options,
                                               gamestate,
                                               &tilePosition,
                                               viewOffsetX - xyOffset * 32,
                                               viewOffsetY - xyOffset * 32,
                                               gamestate->CurrentTick,
                                               &redrawNearbyTop,
                                               canvas)) {
                            return trc_ReportError("Failed to redraw a tile");
                        }

                        tilePosition.X++;

                        if (!renderer_DrawTile(options,
                                               gamestate,
                                               &tilePosition,
                                               viewOffsetX - xyOffset * 32,
                                               viewOffsetY - xyOffset * 32,
                                               gamestate->CurrentTick,
                                               &redrawNearbyTop,
                                               canvas)) {
                            return trc_ReportError("Failed to redraw a tile");
                        }

                        tilePosition.Y++;
                    }

                    if (!renderer_DrawTile(options,
                                           gamestate,
                                           &tilePosition,
                                           viewOffsetX - xyOffset * 32,
                                           viewOffsetY - xyOffset * 32,
                                           gamestate->CurrentTick,
                                           &redrawNearbyTop,
                                           canvas)) {
                        return trc_ReportError("Failed to redraw a tile");
                    }
                }
            }
        }
    }

    return true;
}

static void renderer_DrawNumericalEffects(
        const struct trc_render_options *options,
        struct trc_game_state *gamestate,
        struct trc_canvas *canvas,
        int viewOffsetX,
        int viewOffsetY,
        float scaleX,
        float scaleY,
        struct trc_position *tilePosition,
        struct trc_tile *currentTile) {
    const struct trc_fonts *fonts = &(gamestate->Version)->Fonts;

    struct trc_pixel foregroundColor;
    unsigned effectShuntX, effectShuntY;
    unsigned effectIdx;

    pixel_SetRGB(&foregroundColor, 0, 0, 0);

    effectShuntX = 0;
    effectShuntY = 0;

    effectIdx = currentTile->NumericalIndex;
    do {
        struct trc_numerical_effect *effect;

        effectIdx = ((unsigned)(effectIdx)-1) % MAX_EFFECTS_PER_TILE;
        effect = &currentTile->NumericalEffects[effectIdx];

        if ((effect->StartTick + 750) < gamestate->CurrentTick) {
            break;
        } else if (effect->Value != 0) {
            unsigned textCenterX, textCenterY;
            uint16_t textLength;
            char textBuffer[32];

            renderer_Convert8BitColor(effect->Color, &foregroundColor);

            textCenterX = ((tilePosition->X * 32 + viewOffsetX - 16) * scaleX) +
                          2 + effectShuntX;
            textCenterY = ((tilePosition->Y * 32 + viewOffsetY - 32) * scaleY) +
                          2 - effectShuntY;
            textCenterY -=
                    (((gamestate->CurrentTick - effect->StartTick) / 750.0f) *
                     32.0f);

            /* Yes, they actually do get shunted this far. */
            effectShuntX += 2 + (int)(scaleX * 9.0f);
            effectShuntY += 0;

            textLength = (uint16_t)snprintf(textBuffer,
                                            sizeof(textBuffer),
                                            "%u",
                                            effect->Value);
            textrenderer_DrawCenteredString(&fonts->GameFont,
                                            &foregroundColor,
                                            textCenterX,
                                            textCenterY,
                                            textLength,
                                            textBuffer,
                                            canvas);
        }
    } while (effectIdx != currentTile->NumericalIndex);
}

static bool renderer_DrawCreatureOverlay(
        const struct trc_render_options *options,
        struct trc_game_state *gamestate,
        struct trc_canvas *canvas,
        int isObscured,
        int heightDisplacement,
        int rightX,
        int bottomY,
        float scaleX,
        float scaleY,
        struct trc_creature *creature) {
    const struct trc_fonts *fonts = &(gamestate->Version)->Fonts;

    struct trc_pixel foregroundColor, backgroundColor;
    int creatureRX, creatureBY;

    pixel_SetRGB(&backgroundColor, 0, 0, 0);
    renderer_GetCreatureInfoColor(creature->Health,
                                  isObscured,
                                  &foregroundColor);

    creatureRX = rightX - heightDisplacement;
    creatureBY = bottomY - heightDisplacement;

    if (creature->MovementInformation.WalkEndTick >= gamestate->CurrentTick) {
        creatureRX += creature->MovementInformation.WalkOffsetX;
        creatureBY += creature->MovementInformation.WalkOffsetY;
    }

    if (creature->Outfit.Id != 0) {
        if (creature->Outfit.MountId == 0) {
            struct trc_entitytype *outfitType;

            if (!types_GetOutfit(gamestate->Version,
                                 creature->Outfit.Id,
                                 &outfitType)) {
                return trc_ReportError("Failed to get creature outfit");
            }

            creatureRX -= outfitType->Properties.DisplacementX;
            creatureBY -= outfitType->Properties.DisplacementY;
        } else {
            struct trc_entitytype *mountType;

            if (!types_GetOutfit(gamestate->Version,
                                 creature->Outfit.MountId,
                                 &mountType)) {
                return trc_ReportError("Failed to get creature outfit (As "
                                       "mount)");
            }

            creatureRX -= mountType->Properties.DisplacementX;
            creatureBY -= mountType->Properties.DisplacementY;
        }
    } else if (creature->Outfit.Item.Id == 0 &&
               !IS_PLAYER_CREATURE(creature->Id)) {
        /* Invisible monster, skip it altogether. These stopped being sent
         * altogether in recent versions to prevent client-side cheats. */
        return true;
    } else {
        creatureRX -= 8;
        creatureBY -= 8;
    }

    if (!(options->SkipRenderingPlayerNames &&
          IS_PLAYER_CREATURE(creature->Id)) &&
        !(options->SkipRenderingNonPlayerNames &&
          !IS_PLAYER_CREATURE(creature->Id))) {
        int nameCenterX, nameCenterY;

        nameCenterX = MAX(2, (creatureRX - 32) * scaleX + (16 * scaleX));
        nameCenterY = MAX(2, (creatureBY - 32) * scaleY - 16);

        textrenderer_DrawCenteredProperCaseString(&fonts->GameFont,
                                                  &foregroundColor,
                                                  nameCenterX,
                                                  nameCenterY,
                                                  creature->NameLength,
                                                  creature->Name,
                                                  canvas);
    }

    if (!options->SkipRenderingCreatureHealthBars) {
        int healthBarAbsoluteX, healthBarAbsoluteY;

        healthBarAbsoluteX =
                MAX(2, (creatureRX - 32) * scaleX + (16 * scaleX) - 14);
        healthBarAbsoluteY = MAX(14, (creatureBY - 32) * scaleY - 4);

        canvas_DrawRectangle(canvas,
                             &backgroundColor,
                             healthBarAbsoluteX + 0,
                             healthBarAbsoluteY + 0,
                             27,
                             4);
        canvas_DrawRectangle(canvas,
                             &foregroundColor,
                             healthBarAbsoluteX + 1,
                             healthBarAbsoluteY + 1,
                             creature->Health / 4,
                             2);
    }

    if (!options->SkipRenderingCreatureIcons) {
        int iconAbsoluteX, iconAbsoluteY;

        iconAbsoluteX = MAX(2, (creatureRX - 32) * scaleX + (16 * scaleX) + 9);
        iconAbsoluteY = MAX(2, (creatureBY - 32) * scaleY + 1);

        switch (creature->Type) {
        case CREATURE_TYPE_NPC:
            /* TODO: render those newfangled NPC icon thingies.
             *
             * Cycle time between dual icons seems to be about a second or
             * something. */
            break;
        case CREATURE_TYPE_SUMMON_OTHERS:
        case CREATURE_TYPE_SUMMON_OWN: {
            /* Currently only these creature types have a type sprite.
             */
            const struct trc_sprite *typeSprite =
                    icons_GetCreatureTypeIcon(gamestate->Version,
                                              creature->Type);

            canvas_Draw(canvas,
                        typeSprite,
                        iconAbsoluteX,
                        iconAbsoluteY,
                        typeSprite->Width,
                        typeSprite->Height);
            break;
        }
        case CREATURE_TYPE_PLAYER:
            /* Only players have this kind of stuff. */

            if (creature->Shield != PARTY_SHIELD_NONE) {
                const struct trc_sprite *shieldSprite;
                int iconOffset;

                switch (creature->Shield) {
                case PARTY_SHIELD_YELLOW_NOSHAREDEXP_BLINK:
                case PARTY_SHIELD_BLUE_NOSHAREDEXP_BLINK:
                    if (gamestate->CurrentTick % 1000 >= 500) {
                        break;
                    }
                default:
                    shieldSprite = icons_GetShield(gamestate->Version,
                                                   creature->Shield);

                    /* If a skull is present, it will push the shield to the
                     * left. */
                    iconOffset =
                            (creature->Skull != CHARACTER_SKULL_NONE) ? -13 : 0;

                    canvas_Draw(canvas,
                                shieldSprite,
                                iconAbsoluteX + iconOffset,
                                iconAbsoluteY,
                                shieldSprite->Width,
                                shieldSprite->Height);

                    break;
                }
            }

            if (creature->Skull != CHARACTER_SKULL_NONE) {
                const struct trc_sprite *skullSprite =
                        icons_GetSkull(gamestate->Version, creature->Skull);
                canvas_Draw(canvas,
                            skullSprite,
                            iconAbsoluteX,
                            iconAbsoluteY,
                            skullSprite->Width,
                            skullSprite->Height);
            }

            if (creature->Shield != PARTY_SHIELD_NONE ||
                creature->Skull != CHARACTER_SKULL_NONE) {
                iconAbsoluteY += 13;
            }

            if (creature->WarIcon != WAR_ICON_NONE) {
                const struct trc_sprite *warSprite =
                        icons_GetWar(gamestate->Version, creature->WarIcon);

                canvas_Draw(canvas,
                            warSprite,
                            iconAbsoluteX,
                            iconAbsoluteY,
                            warSprite->Width,
                            warSprite->Height);

                iconAbsoluteY += 13;
            }

            if (creature->GuildMembersOnline >= 5) {
                const struct trc_icons *icons = &(gamestate->Version)->Icons;

                canvas_Draw(canvas,
                            &icons->RiskyIcon,
                            iconAbsoluteX,
                            iconAbsoluteY,
                            icons->RiskyIcon.Width,
                            icons->RiskyIcon.Height);
            }
        case CREATURE_TYPE_MONSTER:
            break;
        }
    }

    return true;
}

static bool renderer_DrawTileOverlay(const struct trc_render_options *options,
                                     struct trc_game_state *gamestate,
                                     struct trc_canvas *canvas,
                                     int isObscured,
                                     int rightX,
                                     int bottomY,
                                     float scaleX,
                                     float scaleY,
                                     struct trc_tile *currentTile) {
    int heightDisplacement = 0;

    /* Get the height displacement of the current tile */
    for (int objectIdx = 0; objectIdx < currentTile->ObjectCount; objectIdx++) {
        struct trc_entitytype *itemType;
        struct trc_object *item;

        if (currentTile->Objects[objectIdx].Id == TILE_OBJECT_CREATURE) {
            continue;
        }

        item = &currentTile->Objects[objectIdx];

        if (!types_GetItem(gamestate->Version, item->Id, &itemType)) {
            return trc_ReportError("Could not get item type");
        }

        if (itemType->Properties.StackPriority != 3) {
            heightDisplacement =
                    MIN(MAX_HEIGHT_DISPLACEMENT,
                        heightDisplacement + itemType->Properties.Height);
        }
    }

    /* Draw info of creatures that belong on this tile */
    for (int objectIdx = 0; objectIdx < currentTile->ObjectCount; objectIdx++) {
        struct trc_creature *creature;

        if (currentTile->Objects[objectIdx].Id != TILE_OBJECT_CREATURE) {
            continue;
        }

        if (!creaturelist_GetCreature(
                    &gamestate->CreatureList,
                    currentTile->Objects[objectIdx].CreatureId,
                    &creature)) {
            return trc_ReportError("Could not get creature");
        }

        if (creature->Health > 0) {
            if (!renderer_DrawCreatureOverlay(options,
                                              gamestate,
                                              canvas,
                                              isObscured,
                                              heightDisplacement,
                                              rightX,
                                              bottomY,
                                              scaleX,
                                              scaleY,
                                              creature)) {
                return trc_ReportError("Failed to draw creature overlay");
            }
        }
    }

    return true;
}

static bool renderer_DrawMapOverlay(const struct trc_render_options *options,
                                    struct trc_game_state *gamestate,
                                    struct trc_canvas *canvas,
                                    int viewOffsetX,
                                    int viewOffsetY,
                                    float scaleX,
                                    float scaleY) {
    for (int xIdx = 0; xIdx < TILE_BUFFER_WIDTH; xIdx++) {
        for (int yIdx = 0; yIdx < TILE_BUFFER_HEIGHT; yIdx++) {
            int isObscured, rightX, bottomY;
            struct trc_position tilePosition;
            struct trc_tile *currentTile;

            tilePosition.X = gamestate->Map.Position.X - 8 + xIdx;
            tilePosition.Y = gamestate->Map.Position.Y - 6 + yIdx;
            tilePosition.Z = gamestate->Map.Position.Z;

            rightX = tilePosition.X * 32 + viewOffsetX;
            bottomY = tilePosition.Y * 32 + viewOffsetY;

            if (rightX > (NATIVE_RESOLUTION_X + 32) ||
                bottomY > (NATIVE_RESOLUTION_Y + 32)) {
                continue;
            } else if (rightX < -32 || bottomY < -32) {
                continue;
            }

            currentTile = map_GetTile(&gamestate->Map,
                                      tilePosition.X,
                                      tilePosition.Y,
                                      tilePosition.Z);

            /* Only show status for creatures on tiles that are completely
             * visible. */
            if ((yIdx > 0 && yIdx < (TILE_BUFFER_HEIGHT - 2)) &&
                (xIdx > 0 && xIdx < (TILE_BUFFER_WIDTH - 1))) {
                if (!options->SkipRenderingUpperFloors && tilePosition.Z <= 7) {
                    int renderHeight = map_GetRenderHeight(&gamestate->Map,
                                                           rightX,
                                                           bottomY);

                    isObscured = renderHeight < tilePosition.Z;
                } else {
                    isObscured = 0;
                }

                renderer_DrawTileOverlay(options,
                                         gamestate,
                                         canvas,
                                         isObscured,
                                         rightX,
                                         bottomY,
                                         scaleX,
                                         scaleY,
                                         currentTile);
            }

            if (!options->SkipRenderingNumericalEffects) {
                renderer_DrawNumericalEffects(options,
                                              gamestate,
                                              canvas,
                                              viewOffsetX,
                                              viewOffsetY,
                                              scaleX,
                                              scaleY,
                                              &tilePosition,
                                              currentTile);
            }
        }
    }

    return true;
}

static int renderer_MessageColor(enum TrcMessageMode mode) {
    switch (mode) {
    case MESSAGEMODE_SAY:
    case MESSAGEMODE_SPELL:
    case MESSAGEMODE_WHISPER:
    case MESSAGEMODE_YELL:
        return 210;
    case MESSAGEMODE_MONSTER_SAY:
    case MESSAGEMODE_MONSTER_YELL:
        return 192;
    case MESSAGEMODE_NPC_START:
        /* Light-blue, above creature */
        return 35;
    case MESSAGEMODE_GAME:
        /* White, center screen */
        return 215;
    case MESSAGEMODE_PRIVATE_IN:
        /* Light-blue, top-center screen */
        return 35;
    case MESSAGEMODE_WARNING:
        /* Red, center screen */
        return 194;
    case MESSAGEMODE_HOTKEY:
    case MESSAGEMODE_NPC_TRADE:
    case MESSAGEMODE_GUILD:
    case MESSAGEMODE_LOOT:
    case MESSAGEMODE_LOOK:
        /* Green, center screen */
        return 30;
    case MESSAGEMODE_FAILURE:
    case MESSAGEMODE_STATUS:
    case MESSAGEMODE_LOGIN:
        /* White, bottom-center screen */
        return 215;
    default:
        /* Return something fugly so it gets reported. */
        return 10;
    }
}

static bool renderer_DrawMessages(const struct trc_render_options *options,
                                  struct trc_game_state *gamestate,
                                  struct trc_canvas *canvas,
                                  int viewOffsetX,
                                  int viewOffsetY,
                                  float scaleX,
                                  float scaleY) {
    const struct trc_fonts *fonts = &(gamestate->Version)->Fonts;
    struct trc_pixel foregroundColor;
    struct trc_message *message;

    int preserveCoordinates, canMerge;
    int drawnWhiteNotification;
    int drawnGreenNotification;
    int drawnRedNotification;
    int drawnPrivateMessage;
    int drawnStatusMessage;
    int drawnMessages;
    int centerX, bottomY;

    pixel_SetRGB(&foregroundColor, 0, 0, 0);

    drawnWhiteNotification = 0;
    drawnGreenNotification = 0;
    drawnRedNotification = 0;
    drawnPrivateMessage = 0;
    drawnStatusMessage = 0;
    preserveCoordinates = 0;
    drawnMessages = 0;

    centerX = 0;
    bottomY = 0;

    message = (struct trc_message *)&gamestate->MessageList;
    while (messagelist_Sweep(&gamestate->MessageList,
                             gamestate->CurrentTick,
                             &message)) {
        enum TrcTextTransforms transformations = TEXTTRANSFORM_NONE;
        int lineMaxLength = 39;

        switch (message->Type) {
        case MESSAGEMODE_GAME:
            if (drawnWhiteNotification) {
                preserveCoordinates = 0;
                continue;
            }

            drawnWhiteNotification = 1;
            break;
        case MESSAGEMODE_WARNING:
            if (drawnRedNotification) {
                preserveCoordinates = 0;
                continue;
            }

            drawnRedNotification = 1;
            break;
        case MESSAGEMODE_SPELL:
            if (options->SkipRenderingSpellMessages) {
                continue;
            }
            break;
        case MESSAGEMODE_HOTKEY:
            if (options->SkipRenderingHotkeyMessages ||
                drawnGreenNotification) {
                preserveCoordinates = 0;
                continue;
            }

            drawnGreenNotification = 1;
            break;
        case MESSAGEMODE_LOOT:
            if (options->SkipRenderingLootMessages || drawnGreenNotification) {
                preserveCoordinates = 0;
                continue;
            }

            drawnGreenNotification = 1;
            break;
        case MESSAGEMODE_NPC_TRADE:
        case MESSAGEMODE_GUILD:
        case MESSAGEMODE_LOOK:
            if (drawnGreenNotification) {
                preserveCoordinates = 0;
                continue;
            }

            drawnGreenNotification = 1;
            break;
        case MESSAGEMODE_PRIVATE_IN:
            if (options->SkipRenderingPrivateMessages || drawnPrivateMessage) {
                preserveCoordinates = 0;
                continue;
            }

            drawnPrivateMessage = 1;
            break;
        case MESSAGEMODE_FAILURE:
        case MESSAGEMODE_STATUS:
        case MESSAGEMODE_LOGIN:
            if (options->SkipRenderingStatusMessages || drawnStatusMessage) {
                preserveCoordinates = 0;
                continue;
            }

            drawnStatusMessage = 1;
            break;
        case MESSAGEMODE_YELL:
            transformations |= TEXTTRANSFORM_UPPERCASE;
            break;
        case MESSAGEMODE_NPC_START:
            transformations |= TEXTTRANSFORM_HIGHLIGHT;
            break;
        default:
            break;
        }

        if (preserveCoordinates) {
            drawnMessages++;

            if (drawnMessages > 8) {
                preserveCoordinates = 0;
                continue;
            }
        } else {
            drawnMessages = 1;
            pixel_SetTextColor(&foregroundColor,
                               renderer_MessageColor(message->Type));

            switch (message->Type) {
            case MESSAGEMODE_NPC_START:
            case MESSAGEMODE_SAY:
            case MESSAGEMODE_SPELL:
            case MESSAGEMODE_WHISPER:
            case MESSAGEMODE_YELL:
                centerX = MIN(MAX(2,
                                  (message->Position.X * 32 + viewOffsetX -
                                   16) * scaleX),
                              (NATIVE_RESOLUTION_X * scaleX));
                bottomY = MIN(MAX(2,
                                  (message->Position.Y * 32 + viewOffsetY -
                                   32) * scaleY),
                              (NATIVE_RESOLUTION_Y * scaleY));

                /* Shift above the message a bit above the health bar. */
                bottomY -= (int)(8 * scaleY);
                break;
            case MESSAGEMODE_MONSTER_SAY:
            case MESSAGEMODE_MONSTER_YELL:
                centerX = MIN(MAX(2,
                                  (message->Position.X * 32 + viewOffsetX -
                                   16) * scaleX),
                              (NATIVE_RESOLUTION_X * scaleX));
                bottomY = MIN(MAX(2,
                                  (message->Position.Y * 32 + viewOffsetY -
                                   32) * scaleY),
                              (NATIVE_RESOLUTION_Y * scaleY));
                break;
            case MESSAGEMODE_GAME:
                /* White, center screen above player character */
                centerX = MIN(MAX(2, (scaleX * 32) * 15 / 2),
                              (NATIVE_RESOLUTION_X * scaleX));
                bottomY = MIN(MAX(2, (scaleY * 32) * 11 / 2) - 32,
                              (NATIVE_RESOLUTION_Y * scaleY));
                break;
            case MESSAGEMODE_PRIVATE_IN:
                /* Light-blue, top-center screen */
                centerX = MIN(MAX(2, (scaleX * 32) * 15 / 2),
                              (NATIVE_RESOLUTION_X * scaleX));
                bottomY = MIN(MAX(2, (scaleY * 32) * 11 / 2) - (scaleY * 128),
                              (NATIVE_RESOLUTION_Y * scaleY));
                break;
            case MESSAGEMODE_WARNING:
            case MESSAGEMODE_HOTKEY:
            case MESSAGEMODE_NPC_TRADE:
            case MESSAGEMODE_GUILD:
            case MESSAGEMODE_LOOT:
            case MESSAGEMODE_LOOK:
                /* Center screen */
                centerX = MIN(MAX(2, (scaleX * 32) * 15 / 2),
                              (NATIVE_RESOLUTION_X * scaleX));
                bottomY = MIN(MAX(2, (scaleY * 32) * 11 / 2),
                              (NATIVE_RESOLUTION_Y * scaleY));
                break;
            case MESSAGEMODE_FAILURE:
            case MESSAGEMODE_STATUS:
            case MESSAGEMODE_LOGIN:
                /* Bottom-center screen */
                centerX = MIN(MAX(2, (scaleX * 32) * 15 / 2),
                              (NATIVE_RESOLUTION_X * scaleX));
                bottomY = NATIVE_RESOLUTION_Y * scaleY;
                lineMaxLength = INT16_MAX;
                break;
            default:
                break;
            }
        }

        messagelist_QueryNext(&gamestate->MessageList,
                              message,
                              gamestate->CurrentTick,
                              &preserveCoordinates,
                              &canMerge);

        unsigned textWidth, textHeight;

        textrenderer_MeasureBounds(&fonts->GameFont,
                                   transformations,
                                   lineMaxLength,
                                   message->TextLength,
                                   message->Text,
                                   &textWidth,
                                   &textHeight);

        bottomY -= textHeight;

        textrenderer_Render(&fonts->GameFont,
                            TEXTALIGNMENT_CENTER,
                            transformations,
                            &foregroundColor,
                            centerX,
                            bottomY,
                            lineMaxLength,
                            message->TextLength,
                            message->Text,
                            canvas);

        if (!canMerge) {
            const char *messagePrefix = NULL;

            switch (message->Type) {
            case MESSAGEMODE_WHISPER:
                messagePrefix = "whispers";
                break;
            case MESSAGEMODE_YELL:
                if (options->SkipRenderingYellingMessages) {
                    break;
                }

                messagePrefix = "yells";
                break;
            case MESSAGEMODE_NPC_START:
            case MESSAGEMODE_PRIVATE_IN:
            case MESSAGEMODE_SAY:
            case MESSAGEMODE_SPELL:
                messagePrefix = "says";
                break;
            default:
                break;
            }

            if (messagePrefix != NULL) {
                uint16_t textLength;
                char textBuffer[64];

                textLength = (uint16_t)snprintf(textBuffer,
                                                sizeof(textBuffer),
                                                "%s %s:",
                                                message->Author,
                                                messagePrefix);
                textrenderer_DrawCenteredString(&fonts->GameFont,
                                                &foregroundColor,
                                                centerX,
                                                bottomY -
                                                        fonts->GameFont.Height,
                                                textLength,
                                                textBuffer,
                                                canvas);

                bottomY -= fonts->GameFont.Height;
            }
        }
    }

    return true;
}

bool renderer_DrawOverlay(const struct trc_render_options *options,
                          struct trc_game_state *gamestate,
                          struct trc_canvas *canvas) {
    struct trc_creature *playerCreature;
    int viewOffsetX, viewOffsetY;
    float scaleX, scaleY;

    if (!creaturelist_GetCreature(&gamestate->CreatureList,
                                  gamestate->Player.Id,
                                  &playerCreature)) {
        return trc_ReportError("Failed to get the player creature");
    }

    viewOffsetX = (8 - gamestate->Map.Position.X) * 32 -
                  playerCreature->MovementInformation.WalkOffsetX;
    viewOffsetY = (6 - gamestate->Map.Position.Y) * 32 -
                  playerCreature->MovementInformation.WalkOffsetY;

    scaleX = canvas->Width / (float)NATIVE_RESOLUTION_X;
    scaleY = canvas->Height / (float)NATIVE_RESOLUTION_Y;

    if (!options->SkipRenderingCreatures) {
        if (!renderer_DrawMapOverlay(options,
                                     gamestate,
                                     canvas,
                                     viewOffsetX,
                                     viewOffsetY,
                                     scaleX,
                                     scaleY)) {
            return trc_ReportError("Failed to render creature overlay");
        }
    }

    if (!options->SkipRenderingMessages) {
        if (!renderer_DrawMessages(options,
                                   gamestate,
                                   canvas,
                                   viewOffsetX,
                                   viewOffsetY,
                                   scaleX,
                                   scaleY)) {
            return trc_ReportError("Failed to render message overlay");
        }
    }

    return true;
}

bool renderer_DrawIconBar(const struct trc_render_options *options,
                          struct trc_game_state *gamestate,
                          struct trc_canvas *canvas,
                          int *offsetX,
                          int *offsetY) {
    const struct trc_icons *icons = &(gamestate->Version)->Icons;
    struct trc_creature *playerCreature;
    int baseX, baseY;
    int iconOffset;

    baseX = *offsetX;
    baseY = *offsetY;

    if (!creaturelist_GetCreature(&gamestate->CreatureList,
                                  gamestate->Player.Id,
                                  &playerCreature)) {
        return trc_ReportError("Failed to get the player creature");
    }

    canvas_Draw(canvas,
                &icons->IconBarBackground,
                baseX + 16,
                baseY,
                icons->IconBarBackground.Width,
                icons->IconBarBackground.Height);
    iconOffset = baseX + 2 + 16;

    for (int statusIdx = STATUS_ICON_FIRST; statusIdx < STATUS_ICON_LAST;
         statusIdx++) {
        if (gamestate->Player.Icons & (1 << statusIdx)) {
            const struct trc_sprite *statusIconSprite;

            /* Don't render both swords and pz block at the same time. */
            if (statusIdx == STATUS_ICON_SWORDS &&
                (gamestate->Player.Icons & (1 << STATUS_ICON_PZBLOCK))) {
                continue;
            }

            statusIconSprite =
                    icons_GetIconBarStatus(gamestate->Version,
                                           (enum TrcStatusIcon)statusIdx);

            canvas_Draw(canvas,
                        statusIconSprite,
                        iconOffset,
                        baseY + 2,
                        statusIconSprite->Width,
                        statusIconSprite->Height);

            iconOffset += statusIconSprite->Width;
        }
    }

    /* Draw party/skulls/waricon, if any. */
    if (playerCreature->Skull != CHARACTER_SKULL_NONE) {
        const struct trc_sprite *iconBarSkull =
                icons_GetIconBarSkull(gamestate->Version,
                                      playerCreature->Skull);
        canvas_Draw(canvas,
                    iconBarSkull,
                    iconOffset,
                    baseY + 2,
                    iconBarSkull->Width,
                    iconBarSkull->Height);

        iconOffset += iconBarSkull->Width;
    }

    if (playerCreature->WarIcon == WAR_ICON_ALLY) {
        canvas_Draw(canvas,
                    &icons->IconBarWar,
                    iconOffset,
                    baseY + 2,
                    icons->IconBarWar.Width,
                    icons->IconBarWar.Height);

        iconOffset += icons->IconBarWar.Width;
    }

    *offsetY = baseY + 2 + icons->IconBarBackground.Height;
    return true;
}

static bool renderer_DrawIconArea(const struct trc_render_options *options,
                                  struct trc_game_state *gamestate,
                                  struct trc_canvas *canvas,
                                  int offsetX,
                                  int offsetY) {
    const struct trc_icons *icons = &(gamestate->Version)->Icons;
    struct trc_creature *playerCreature;

    int iconAreaLimitX, iconAreaX, iconAreaY;
    int iconOffsetX, iconOffsetY;

    if (!creaturelist_GetCreature(&gamestate->CreatureList,
                                  gamestate->Player.Id,
                                  &playerCreature)) {
        return trc_ReportError("Failed to get the player creature");
    }

    iconAreaX = 16 + offsetX;
    iconAreaY = offsetY + 125;
    iconAreaLimitX = iconAreaX + icons->SecondaryStatBackground.Width;

    canvas_Draw(canvas,
                &icons->SecondaryStatBackground,
                iconAreaX,
                iconAreaY,
                icons->SecondaryStatBackground.Width,
                icons->SecondaryStatBackground.Height);
    iconAreaX += 1;
    iconAreaY += 1;

    iconOffsetX = iconAreaX;
    iconOffsetY = iconAreaY;

    for (int statusIconIdx = STATUS_ICON_FIRST;
         statusIconIdx < STATUS_ICON_LAST;
         statusIconIdx++) {
        if (gamestate->Player.Icons & (1 << statusIconIdx)) {
            const struct trc_sprite *statusIconSprite;

            /* Don't render both swords and pz block at the same time. */
            if ((1 << statusIconIdx) == STATUS_ICON_SWORDS &&
                (gamestate->Player.Icons & STATUS_ICON_PZBLOCK)) {
                continue;
            }

            statusIconSprite =
                    icons_GetIconBarStatus(gamestate->Version,
                                           (enum TrcStatusIcon)statusIconIdx);

            canvas_Draw(canvas,
                        statusIconSprite,
                        iconOffsetX,
                        iconOffsetY,
                        statusIconSprite->Width,
                        statusIconSprite->Height);

            iconOffsetX += statusIconSprite->Width + 2;

            if ((iconOffsetX + statusIconSprite->Width) >= iconAreaLimitX) {
                iconOffsetX = iconAreaX;
                iconOffsetY += statusIconSprite->Height;
            }
        }
    }

    /* Draw party/skulls, if any. */
    if (playerCreature->Skull != CHARACTER_SKULL_NONE) {
        const struct trc_sprite *iconBarSkull =
                icons_GetIconBarSkull(gamestate->Version,
                                      playerCreature->Skull);

        canvas_Draw(canvas,
                    iconBarSkull,
                    iconOffsetX,
                    iconOffsetY,
                    iconBarSkull->Width,
                    iconBarSkull->Height);
    }

    /* We'll skip war icon since it's not present in the versions using the
     * icon area. */

    return true;
}

bool renderer_DrawStatusBars(const struct trc_render_options *options,
                             struct trc_game_state *gamestate,
                             struct trc_canvas *canvas,
                             int *offsetX,
                             int *offsetY) {
    const struct trc_icons *icons = &(gamestate->Version)->Icons;
    const struct trc_fonts *fonts = &(gamestate->Version)->Fonts;
    struct trc_pixel foregroundColor;

    pixel_SetRGB(&foregroundColor, 0xFF, 0xFF, 0xFF);

    int statusBarX, statusBarY, baseX, baseY;
    uint16_t textLength;
    char textBuffer[32];

    baseX = *offsetX;
    baseY = *offsetY;

    statusBarX = baseX + 24;
    statusBarY = baseY;

    canvas_Draw(canvas,
                &icons->HealthIcon,
                baseX,
                baseY + 2,
                icons->HealthIcon.Width,
                icons->HealthIcon.Height);
    canvas_Draw(canvas,
                &icons->ManaIcon,
                baseX,
                baseY + 15,
                icons->ManaIcon.Width,
                icons->ManaIcon.Height);

    /* Draw health and mana bar background. */
    canvas_Draw(canvas,
                &icons->EmptyStatusBar,
                statusBarX + 2,
                statusBarY + 2,
                icons->EmptyStatusBar.Width,
                icons->EmptyStatusBar.Height);
    canvas_Draw(canvas,
                &icons->EmptyStatusBar,
                statusBarX + 2,
                statusBarY + 15,
                icons->EmptyStatusBar.Width,
                icons->EmptyStatusBar.Height);

    if (gamestate->Player.Stats.MaxHealth > 0) {
        canvas_Draw(canvas,
                    &icons->HealthBar,
                    statusBarX + 2,
                    statusBarY + 2,
                    (icons->HealthBar.Width * gamestate->Player.Stats.Health) /
                            gamestate->Player.Stats.MaxHealth,
                    11);
    }

    if (gamestate->Player.Stats.MaxMana > 0) {
        canvas_Draw(canvas,
                    &icons->ManaBar,
                    statusBarX + 2,
                    statusBarY + 15,
                    (icons->ManaBar.Width * gamestate->Player.Stats.Mana) /
                            gamestate->Player.Stats.MaxMana,
                    11);
    }

    textLength = (uint16_t)snprintf(textBuffer,
                                    sizeof(textBuffer),
                                    "%hi / %hi",
                                    gamestate->Player.Stats.Health,
                                    gamestate->Player.Stats.MaxHealth);
    textrenderer_DrawCenteredString(&fonts->GameFont,
                                    &foregroundColor,
                                    statusBarX + 2 + icons->HealthBar.Width / 2,
                                    statusBarY + 2,
                                    textLength,
                                    textBuffer,
                                    canvas);

    textLength = (uint16_t)snprintf(textBuffer,
                                    sizeof(textBuffer),
                                    "%hi / %hi",
                                    gamestate->Player.Stats.Mana,
                                    gamestate->Player.Stats.MaxMana);
    textrenderer_DrawCenteredString(&fonts->GameFont,
                                    &foregroundColor,
                                    statusBarX + 2 + icons->ManaBar.Width / 2,
                                    statusBarY + 15,
                                    textLength,
                                    textBuffer,
                                    canvas);

    /* Update the render position */
    baseY += 18 + icons->EmptyStatusBar.Height;

    (*offsetY) = baseY;
    return true;
}

bool renderer_DrawInventoryArea(const struct trc_render_options *options,
                                struct trc_game_state *gamestate,
                                struct trc_canvas *canvas,
                                int *offsetX,
                                int *offsetY) {
    const struct trc_icons *icons = &(gamestate->Version)->Icons;
    const struct trc_fonts *fonts = &(gamestate->Version)->Fonts;

    static const struct {
        int X, Y;
    } inventoryOffsets[] = {
            {53, 0} /* INVENTORY_SLOT_HEAD */,
            {16, 13} /* INVENTORY_SLOT_AMULET */,
            {90, 13} /* INVENTORY_SLOT_BACKPACK */,
            {53, 37} /* INVENTORY_SLOT_CHEST */,
            {90, 50} /* INVENTORY_SLOT_RIGHTARM */,
            {16, 50} /* INVENTORY_SLOT_LEFTARM */,
            {53, 74} /* INVENTORY_SLOT_LEGS */,
            {53, 111} /* INVENTORY_SLOT_BOOTS */,
            {16, 87} /* INVENTORY_SLOT_RING */,
            {90, 87} /* INVENTORY_SLOT_QUIVER */
    };

    struct trc_pixel foregroundColor;
    uint16_t textLength;
    char textBuffer[32];
    int baseX, baseY;

    baseX = *offsetX;
    baseY = *offsetY;

    _Static_assert(INVENTORY_SLOT_FIRST == INVENTORY_SLOT_HEAD &&
                           INVENTORY_SLOT_LAST == INVENTORY_SLOT_PURSE,
                   "Inventory coordinate array must be up to date");
    for (int slot = INVENTORY_SLOT_HEAD; slot < INVENTORY_SLOT_LAST; slot++) {
        renderer_DrawInventorySlot(gamestate,
                                   (enum TrcInventorySlot)slot,
                                   baseX + inventoryOffsets[slot].X,
                                   baseY + inventoryOffsets[slot].Y,
                                   canvas);
    }

    if (!(gamestate->Version)->Features.IconBar) {
        if (!renderer_DrawIconArea(options, gamestate, canvas, baseX, baseY)) {
            return trc_ReportError("Failed to render the icon area");
        }
    }

    baseY += 124;

    pixel_SetRGB(&foregroundColor, 0xAF, 0xAF, 0xAF);

    if ((gamestate->Version)->Features.IconBar) {
        /* The small client font doesn't do bordering or tinting, so
         * we're not passing any colors. */
        canvas_Draw(canvas,
                    &icons->SecondaryStatBackground,
                    16 + baseX,
                    baseY,
                    icons->SecondaryStatBackground.Width,
                    icons->SecondaryStatBackground.Height);

        textrenderer_DrawCenteredString(&fonts->InterfaceFontSmall,
                                        NULL,
                                        16 + baseX + 17,
                                        baseY + 2,
                                        CONSTSTRLEN("Soul:"),
                                        "Soul:",
                                        canvas);

        textLength = (uint16_t)snprintf(textBuffer,
                                        sizeof(textBuffer),
                                        "%hhu",
                                        gamestate->Player.Stats.SoulPoints);
        textrenderer_DrawCenteredString(&fonts->InterfaceFontLarge,
                                        &foregroundColor,
                                        16 + baseX + 17,
                                        baseY + 10,
                                        textLength,
                                        textBuffer,
                                        canvas);
    }

    canvas_Draw(canvas,
                &icons->SecondaryStatBackground,
                16 + baseX + 74,
                baseY,
                icons->SecondaryStatBackground.Width,
                icons->SecondaryStatBackground.Height);
    textrenderer_DrawCenteredString(&fonts->InterfaceFontSmall,
                                    NULL,
                                    16 + baseX + 90,
                                    baseY + 2,
                                    CONSTSTRLEN("Cap:"),
                                    "Cap:",
                                    canvas);

    uint32_t capacity = gamestate->Player.Stats.Capacity /
                        gamestate->Version->Features.CapacityDivisor;

    textLength =
            (uint16_t)snprintf(textBuffer, sizeof(textBuffer), "%u", capacity);
    textrenderer_DrawCenteredString(&fonts->InterfaceFontLarge,
                                    &foregroundColor,
                                    16 + baseX + 90,
                                    baseY + 10,
                                    textLength,
                                    textBuffer,
                                    canvas);

    /* Update the render position */
    *offsetY = baseY + icons->SecondaryStatBackground.Height + 3;
    return true;
}

bool renderer_DrawContainer(const struct trc_render_options *options,
                            struct trc_game_state *gamestate,
                            struct trc_canvas *canvas,
                            struct trc_container *container,
                            bool collapsed,
                            int maxX,
                            int maxY,
                            int *offsetX,
                            int *offsetY) {
    const struct trc_icons *icons = &(gamestate->Version)->Icons;
    const struct trc_fonts *fonts = &(gamestate->Version)->Fonts;

    struct trc_pixel foregroundColor;
    int baseX, baseY;

    baseX = *offsetX;
    baseY = *offsetY;

    pixel_SetRGB(&foregroundColor, 0xBF, 0xBF, 0xBF);

    textrenderer_DrawProperCaseString(&fonts->InterfaceFontLarge,
                                      &foregroundColor,
                                      baseX,
                                      baseY + 2,
                                      container->NameLength,
                                      container->Name,
                                      canvas);

    baseY += fonts->InterfaceFontLarge.Height;

    if (!collapsed) {
        const int slotSize = (32 + 4);
        const int slotsPerRow = (maxX - baseX) / slotSize;
        int itemIdx;

        for (itemIdx = 0; itemIdx < container->SlotsPerPage; itemIdx++) {
            int slotX, slotY;

            slotX = baseX + (itemIdx % slotsPerRow) * slotSize;
            slotY = baseY + (itemIdx / slotsPerRow) * slotSize;

            if (slotX > maxX || slotY > maxY) {
                break;
            }

            if (itemIdx >= container->ItemCount) {
                if (itemIdx % slotsPerRow == 0 && itemIdx > 0) {
                    break;
                } else {
                    canvas_Draw(canvas,
                                &icons->InventoryBackground,
                                slotX,
                                slotY,
                                icons->InventoryBackground.Width,
                                icons->InventoryBackground.Height);
                }
            } else {
                if (!renderer_DrawInventoryItem(gamestate,
                                                &container->Items[itemIdx],
                                                slotX,
                                                slotY,
                                                canvas)) {
                    return trc_ReportError("Failed to render container item");
                }
            }
        }

        /* Round up to avoid having the next container overdraw this one, in
         * case the container size wasn't a clean multiple of the slot
         * modulus. */
        baseY += ((itemIdx + slotsPerRow - 1) / slotsPerRow) * slotSize;
    }

    *offsetY = baseY;
    return true;
}

void renderer_RenderClientBackground(struct trc_game_state *gamestate,
                                     struct trc_canvas *canvas,
                                     int leftX,
                                     int topY,
                                     int rightX,
                                     int bottomY) {
    const struct trc_icons *icons = &(gamestate->Version)->Icons;
    int height, width;

    height = icons->ClientBackground.Height;
    width = icons->ClientBackground.Height;

    for (int toY = topY; toY < bottomY; toY += height) {
        for (int toX = leftX; toX < rightX; toX += width) {
            canvas_Draw(canvas,
                        &icons->ClientBackground,
                        toX,
                        toY,
                        MIN(width, rightX - toX),
                        MIN(height, bottomY - toY));
        }
    }
}

bool renderer_DumpItem(struct trc_version *version,
                       uint16_t item,
                       struct trc_canvas *canvas) {
    struct trc_position position = {0};
    struct trc_object object = {.Id = item};

    return renderer_DrawItem(version,
                             &object,
                             64,
                             64,
                             0,
                             &position,
                             0,
                             0,
                             1,
                             canvas);
}
