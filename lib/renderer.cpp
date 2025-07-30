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

#include "renderer.hpp"
#include "versions.hpp"

#include "canvas.hpp"
#include "creature.hpp"
#include "effect.hpp"
#include "fonts.hpp"
#include "icons.hpp"
#include "missile.hpp"
#include "pictures.hpp"
#include "textrenderer.hpp"
#include "types.hpp"

#include <cmath>
#include <cstdlib>
#include <format>
#include <initializer_list>
#include <tuple>

#include "utils.hpp"

namespace trc {

#define MAX_HEIGHT_DISPLACEMENT 24

namespace Renderer {

static Pixel Convert8BitColor(uint8_t color) {
    return Pixel(((color / 36) * 51),
                 (((color / 6) % 6) * 51),
                 ((color % 6) * 51));
}

static void UpdateWalkOffset(Gamestate &gamestate, Creature &creature) {
    if (creature.MovementInformation.LastUpdateTick < gamestate.CurrentTick) {
        uint32_t startTick, endTick;

        creature.MovementInformation.LastUpdateTick = gamestate.CurrentTick;

        startTick = creature.MovementInformation.WalkStartTick;
        endTick = creature.MovementInformation.WalkEndTick;
        Assert(startTick <= endTick);

        if ((endTick > gamestate.CurrentTick) && ((endTick - startTick) != 0)) {
            float const walkProgress =
                    ((float)(gamestate.CurrentTick - startTick) /
                     (float)(endTick - startTick));

            creature.MovementInformation.WalkOffsetX =
                    (int)((creature.MovementInformation.Target.X -
                           creature.MovementInformation.Origin.X) *
                          (walkProgress - 1) * 32);
            creature.MovementInformation.WalkOffsetY =
                    (int)((creature.MovementInformation.Target.Y -
                           creature.MovementInformation.Origin.Y) *
                          (walkProgress - 1) * 32);
        } else {
            creature.MovementInformation.WalkOffsetX = 0;
            creature.MovementInformation.WalkOffsetY = 0;
        }
    }
}

static Pixel GetCreatureInfoColor(int healthPercentage, int isObscured) {
    if (isObscured) {
        return Pixel(192, 192, 192);
    } else if (healthPercentage < 4) {
        return Pixel(96, 0, 0);
    } else if (healthPercentage < 10) {
        return Pixel(192, 0, 0);
    } else if (healthPercentage < 30) {
        return Pixel(192, 48, 48);
    } else if (healthPercentage < 60) {
        return Pixel(192, 192, 0);
    } else if (healthPercentage < 95) {
        return Pixel(96, 192, 96);
    }

    return Pixel(0, 192, 0);
}

static bool GetTileUnlookable(const Version &version, const Tile &tile) {
    for (int objectIdx = 0; objectIdx < tile.ObjectCount; objectIdx++) {
        const auto &object = tile.Objects[objectIdx];

        if (!object.IsCreature()) {
            const auto &properties = version.GetItem(object.Id).Properties;

            if (properties.Unlookable) {
                return true;
            }
        }
    }

    return false;
}

static bool GetTileUpdateRenderHeight(const Version &version,
                                      const Tile &tile) {
    for (int objectIdx = 0; objectIdx < tile.ObjectCount; objectIdx++) {
        const auto &object = tile.Objects[objectIdx];

        if (!object.IsCreature()) {
            const auto &properties = version.GetItem(object.Id).Properties;

            if (!properties.DontHide && properties.StackPriority == 0) {
                return true;
            }
        }
    }

    return false;
}

static bool GetTileBlocksPlayerVision(const Version &version,
                                      const Tile &tile) {
    for (int objectIdx = 0; objectIdx < tile.ObjectCount; objectIdx++) {
        const auto &object = tile.Objects[objectIdx];

        if (!object.IsCreature()) {
            const auto &properties = version.GetItem(object.Id).Properties;

            /* Things with a stack priority of 0 (ground) and 2 (some railings)
             * count as solids and cannot be seen through. */
            if (!properties.DontHide && (properties.StackPriority == 0 ||
                                         properties.StackPriority == 2)) {
                return true;
            }
        }
    }

    return false;
}

static int GetTopVisibleFloor(const Gamestate &gamestate) {
    int minZ = 0;

    for (int xIdx = gamestate.Map.Position.X - 1;
         xIdx <= gamestate.Map.Position.X + 1;
         xIdx++) {
        for (int yIdx = gamestate.Map.Position.Y - 1;
             yIdx <= gamestate.Map.Position.Y + 1;
             yIdx++) {
            if (!GetTileUnlookable(
                        gamestate.Version,
                        gamestate.Map.Tile(xIdx,
                                           yIdx,
                                           gamestate.Map.Position.Z)) &&
                !(xIdx != gamestate.Map.Position.X &&
                  yIdx != gamestate.Map.Position.Y)) {
                for (int zIdx = gamestate.Map.Position.Z - 1; zIdx >= minZ;
                     zIdx--) {
                    if (GetTileBlocksPlayerVision(
                                gamestate.Version,
                                gamestate.Map.Tile(
                                        xIdx + (gamestate.Map.Position.Z -
                                                zIdx),
                                        yIdx + (gamestate.Map.Position.Z -
                                                zIdx),
                                        zIdx))) {
                        minZ = zIdx + 1;
                        break;
                    }

                    if (GetTileBlocksPlayerVision(
                                gamestate.Version,
                                gamestate.Map.Tile(xIdx, yIdx, zIdx))) {
                        minZ = zIdx + 1;
                        break;
                    }
                }
            }
        }
    }

    return minZ;
}

static void DrawTypeBounded(const EntityType::FrameGroup &frameGroup,
                            int rightX,
                            int bottomY,
                            int layer,
                            int xMod,
                            int yMod,
                            int zMod,
                            int frame,
                            int maxWidth,
                            int maxHeight,
                            Canvas &canvas) {
    const auto &sprites = frameGroup.Sprites;
    int heightLeft, widthLeft;
    unsigned spriteIndex;

    spriteIndex = (layer + (xMod + (yMod + (zMod + (frame)*frameGroup.ZDiv) *
                                                   frameGroup.YDiv) *
                                           frameGroup.XDiv) *
                                   frameGroup.LayerCount);
    spriteIndex *= (frameGroup.SizeX * frameGroup.SizeY);

    heightLeft = maxHeight;
    widthLeft = maxWidth;

    for (int yIdx = 0; yIdx < frameGroup.SizeY && heightLeft > 0; yIdx++) {
        const int adjustedHeight = std::min(32, heightLeft);

        for (int xIdx = 0; xIdx < frameGroup.SizeX && widthLeft > 0; xIdx++) {
            const int adjustedWidth = std::min(32, widthLeft);

            /* Not an error; creatures like bears, for instance, only have 2
             * sprites in each direction, and we're not supposed to draw the
             * empty sprites. */
            if (spriteIndex >= sprites.size()) {
                return;
            }

            canvas.Draw(*sprites[spriteIndex],
                        rightX - xIdx * 32 - adjustedWidth,
                        bottomY - yIdx * 32 - adjustedHeight,
                        adjustedWidth,
                        adjustedHeight);

            widthLeft -= std::min(32, adjustedWidth);
            spriteIndex++;
        }

        heightLeft -= std::min(32, adjustedHeight);
    }
}

static void DrawType(const EntityType::FrameGroup &frameGroup,
                     int rightX,
                     int bottomY,
                     int layer,
                     int xMod,
                     int yMod,
                     int zMod,
                     int frame,
                     Canvas &canvas) {
    const auto &sprites = frameGroup.Sprites;
    unsigned spriteIndex;

    spriteIndex = (layer + (xMod + (yMod + (zMod + frame * frameGroup.ZDiv) *
                                                   frameGroup.YDiv) *
                                           frameGroup.XDiv) *
                                   frameGroup.LayerCount);
    spriteIndex *= (frameGroup.SizeX * frameGroup.SizeY);

    for (int yIdx = 0; yIdx < frameGroup.SizeY; yIdx++) {
        for (int xIdx = 0; xIdx < frameGroup.SizeX; xIdx++) {
            if (spriteIndex >= sprites.size()) {
                return;
            }

            canvas.Draw(*sprites[spriteIndex],
                        rightX - xIdx * 32 - 32,
                        bottomY - yIdx * 32 - 32,
                        32,
                        32);

            spriteIndex++;
        }
    }
}

static void TintType(const EntityType::FrameGroup &frameGroup,
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
                     Canvas &canvas) {
    const auto &sprites = frameGroup.Sprites;
    unsigned spriteIndex;

    spriteIndex = (layer + (xMod + (yMod + (zMod + frame * frameGroup.ZDiv) *
                                                   frameGroup.YDiv) *
                                           frameGroup.XDiv) *
                                   frameGroup.LayerCount);
    spriteIndex *= (frameGroup.SizeX * frameGroup.SizeY);

    for (int yIdx = 0; yIdx < frameGroup.SizeY; yIdx++) {
        for (int xIdx = 0; xIdx < frameGroup.SizeX; xIdx++) {
            if (spriteIndex >= sprites.size()) {
                return;
            }

            canvas.Tint(*sprites[spriteIndex],
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
}

static void DrawGraphicalEffect(const Version &version,
                                const GraphicalEffect &effect,
                                const Position &position,
                                int rightX,
                                int bottomY,
                                uint32_t tick,
                                Canvas &canvas) {
    const auto &type = version.GetEffect(effect.Id);
    const auto &frameGroup =
            type.FrameGroups[std::to_underlying(FrameGroupIndex::Default)];

    if ((effect.StartTick + 100 * frameGroup.FrameCount) > tick) {
        auto frame = std::min<uint32_t>((tick - effect.StartTick) / 100,
                                        frameGroup.FrameCount - 1);

        for (int layerIdx = 0; layerIdx < frameGroup.LayerCount; layerIdx++) {
            DrawType(frameGroup,
                     rightX,
                     bottomY,
                     layerIdx,
                     position.X % frameGroup.XDiv,
                     position.Y % frameGroup.YDiv,
                     position.Z % frameGroup.ZDiv,
                     frame,
                     canvas);
        }
    }
}

static void DrawMissile(const Missile &missile,
                        const EntityType &type,
                        int rightX,
                        int bottomY,
                        Canvas &canvas) {
    float directionalRatio;
    float deltaX, deltaY;
    int direction;

    deltaX = (float)(missile.Origin.X - missile.Target.X);
    deltaY = (float)(missile.Origin.Y - missile.Target.Y);

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

    const auto &frameGroup =
            type.FrameGroups[std::to_underlying(FrameGroupIndex::Default)];
    for (int layerIdx = 0; layerIdx < frameGroup.LayerCount; layerIdx++) {
        DrawType(frameGroup,
                 rightX,
                 bottomY,
                 layerIdx,
                 direction % frameGroup.XDiv,
                 (direction / frameGroup.XDiv) % frameGroup.YDiv,
                 0,
                 0,
                 canvas);
    }
}

static bool DrawOutfit(const Creature &creature,
                       const EntityType &type,
                       int isMounted,
                       int rightX,
                       int bottomY,
                       uint32_t tick,
                       Canvas &canvas) {
    int directionMod, frame = 0;
    FrameGroupIndex groupId;

    directionMod = static_cast<int>(creature.Heading);

    if (creature.MovementInformation.WalkEndTick > tick) {
        groupId = FrameGroupIndex::Walking;
    } else {
        groupId = FrameGroupIndex::Idle;
    }

    const auto &frameGroup = type.FrameGroups[std::to_underlying(groupId)];

    if (!isMounted) {
        /* Mounted outfits do not use any offsets. */
        rightX -= type.Properties.DisplacementX;
        bottomY -= type.Properties.DisplacementY;
    }

    if (type.Properties.AnimateIdle) {
        frame = (tick / 500) % frameGroup.FrameCount;
    } else {
        if (creature.MovementInformation.WalkEndTick > tick) {
            int xDifference, yDifference;

            /* When a creature has less than 3 frames, the first is used for
             * animation, otherwise it isn't. */
            if (frameGroup.FrameCount <= 2) {
                frame = ((creature.MovementInformation.WalkStartTick - tick) /
                         100) %
                        frameGroup.FrameCount;
            } else {
                frame = (tick / 100) % (frameGroup.FrameCount - 1) + 1;
            }

            /* In case a creature's direction gets updated while walking (Fairly
             * common), we don't want to see the thing moonwalk. */
            xDifference = creature.MovementInformation.Target.X -
                          creature.MovementInformation.Origin.X;
            yDifference = creature.MovementInformation.Target.Y -
                          creature.MovementInformation.Origin.Y;

            if (yDifference < 0) {
                directionMod = 0;
            } else if (yDifference > 0) {
                directionMod = 2;
            }

            if (xDifference < 0) {
                directionMod = 3;
            } else if (xDifference > 0) {
                directionMod = 1;
            }
        }
    }

    for (int addonIdx = 0; addonIdx < frameGroup.YDiv; addonIdx++) {
        if ((addonIdx == 0) ||
            (creature.Outfit.Addons & (1 << (addonIdx - 1)))) {
            DrawType(frameGroup,
                     rightX,
                     bottomY,
                     0,
                     directionMod,
                     addonIdx,
                     isMounted,
                     isMounted ? (frame % 3) : frame,
                     canvas);

            if (frameGroup.LayerCount == 2) {
                TintType(frameGroup,
                         creature.Outfit.HeadColor,
                         creature.Outfit.PrimaryColor,
                         creature.Outfit.SecondaryColor,
                         creature.Outfit.DetailColor,
                         rightX,
                         bottomY,
                         1,
                         directionMod,
                         addonIdx,
                         isMounted,
                         isMounted ? (frame % 3) : frame,
                         canvas);
            }
        }
    }

    return true;
}

/* FIXME: Phase ticks should NOT modify the item! */
static void DrawItem(const Version &version,
                     Object &item,
                     const EntityType &type,
                     int rightX,
                     int bottomY,
                     uint32_t tick,
                     const Position &position,
                     int horizontal,
                     int vertical,
                     int isInInventory,
                     Canvas &canvas) {
    int frame, xMod, yMod, zMod;

    const auto &frameGroup =
            type.FrameGroups[std::to_underlying(FrameGroupIndex::Default)];

    if (type.Properties.Animated) {
        /* Phases were introduced in 10.50, otherwise let each animation frame
         * take 500ms. */
        if (version.Features.AnimationPhases) {
            /* FIXME: This is a lazy and incorrect implementation as the
             * animation time is effectively reset every time an item bounces
             * in and out of the viewport. I remember it being especially
             * noticeable with magic walls back in the day, but didn't have the
             * time to implement it properly back then. Revisit this once we've
             * got some >=10.50 recordings to test with. */
            uint32_t minTime = frameGroup.Phases[item.Animation].Minimum;

            if (tick >= (item.PhaseTick + minTime)) {
                item.Animation = (item.Animation + 1) % frameGroup.FrameCount;
                item.PhaseTick = tick;
            }

            frame = item.Animation;
        } else {
            frame = (tick / 500) % frameGroup.FrameCount;
        }
    } else {
        frame = 0;
    }

    /* For some reason, items in inventory ignore the displacement of their
     * types. */
    if (!isInInventory) {
        rightX -= type.Properties.DisplacementX;
        bottomY -= type.Properties.DisplacementY;
    }

    if (type.Properties.Hangable) {
        int picture = 0;

        if (vertical) {
            picture = 1;
        } else if (horizontal) {
            picture = 2;
        }

        xMod = picture % frameGroup.XDiv;
        yMod = 0;
        zMod = 0;
    } else if (type.Properties.LiquidContainer || type.Properties.LiquidPool) {
        int picture = version.TranslateFluidColor(item.ExtraByte);

        xMod = picture % frameGroup.XDiv;
        yMod = (picture / frameGroup.XDiv) % frameGroup.YDiv;
        zMod = 0;
    } else if (type.Properties.Stackable) {
        int picture;

        if (item.ExtraByte == 0) {
            picture = 0;
        } else if (item.ExtraByte < 5) {
            picture = item.ExtraByte - 1;
        } else if (item.ExtraByte < 10) {
            picture = 4;
        } else if (item.ExtraByte < 25) {
            picture = 5;
        } else if (item.ExtraByte < 50) {
            picture = 6;
        } else {
            picture = 7;
        }

        xMod = picture % frameGroup.XDiv;
        yMod = (picture / frameGroup.XDiv) % frameGroup.YDiv;
        zMod = 0;
    } else {
        xMod = position.X % frameGroup.XDiv;
        yMod = position.Y % frameGroup.YDiv;
        zMod = position.Z % frameGroup.ZDiv;
    }

    for (int layerIdx = 0; layerIdx < frameGroup.LayerCount; layerIdx++) {
        if (isInInventory) {
            DrawTypeBounded(frameGroup,
                            rightX,
                            bottomY,
                            layerIdx,
                            xMod,
                            yMod,
                            zMod,
                            frame,
                            32,
                            32,
                            canvas);
        } else {
            DrawType(frameGroup,
                     rightX,
                     bottomY,
                     layerIdx,
                     xMod,
                     yMod,
                     zMod,
                     frame,
                     canvas);
        }
    }
}

static void DrawCreature(const Version &version,
                         const Creature &creature,
                         int rightX,
                         int bottomY,
                         uint32_t tick,
                         Canvas &canvas) {
    if (creature.Outfit.Id == 0) {
        if (creature.Outfit.Item.Id != 0) {
            /* Render item */
            Position position;
            Object item;

            item.Id = creature.Outfit.Item.Id;
            item.ExtraByte = creature.Outfit.Item.ExtraByte;

            position.Y = 0;
            position.Y = 0;
            position.Z = 0;

            DrawItem(version,
                     item,
                     version.GetItem(item.Id),
                     rightX,
                     bottomY,
                     tick,
                     position,
                     0,
                     0,
                     0,
                     canvas);
        } else if (creature.Type == CreatureType::Player) {
            /* Invisible players should be rendered as a shimmer, while
             * invisible monsters are ignored altogether. */
            GraphicalEffect shimmerEffect;

            shimmerEffect.Id = 0x0D;

            /* Draw the effect at the default creature offset */
            rightX -= 8;
            bottomY -= 8;

            /* TODO: find out the proper cycling. */
            shimmerEffect.StartTick = std::max(0u, tick - 500);

            DrawGraphicalEffect(version,
                                shimmerEffect,
                                Position(),
                                rightX,
                                bottomY,
                                std::max(0u, tick - tick % 500),
                                canvas);
        }
    } else {
        const auto &outfit = version.GetOutfit(creature.Outfit.Id);

        if (creature.Outfit.MountId == 0) {
            DrawOutfit(creature, outfit, 0, rightX, bottomY, tick, canvas);
        } else {
            const auto &mount = version.GetOutfit(creature.Outfit.MountId);

            DrawOutfit(creature, mount, 0, rightX, bottomY, tick, canvas);

            DrawOutfit(creature, outfit, 1, rightX, bottomY, tick, canvas);
        }
    }
}

static void DrawMovingCreatures(Gamestate &gamestate,
                                const Position &position,
                                int heightDisplacement,
                                int rightX,
                                int bottomY,
                                uint32_t tick,
                                Canvas &canvas) {
    /* Check for and draw creatures which might overlap with this tile. */
    for (int yIdx = -1; yIdx <= 1; yIdx++) {
        for (int xIdx = -1; xIdx <= 1; xIdx++) {
            auto &neighborTile = gamestate.Map.Tile(position.X + xIdx,
                                                    position.Y + yIdx,
                                                    position.Z);

            for (int objectIdx = 0; objectIdx < neighborTile.ObjectCount;
                 objectIdx++) {
                const auto &object = neighborTile.Objects[objectIdx];

                if (!object.IsCreature()) {
                    continue;
                }

                /* Strangely, the official client is okay with non-existent
                 * creatures here, simply skipping them. */
                if (auto creature = gamestate.FindCreature(object.CreatureId)) {
                    int offsetRelativeThisX, offsetRelativeThisY;

                    UpdateWalkOffset(gamestate, *creature);

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
                        DrawCreature(gamestate.Version,
                                     *creature,
                                     rightX - heightDisplacement +
                                             offsetRelativeThisX,
                                     bottomY - heightDisplacement +
                                             offsetRelativeThisY,
                                     tick,
                                     canvas);
                    }
                }
            }
        }
    }
}

/* Check for and draw all projectiles which might overlap with this tile. */
static void DrawMissiles(Gamestate &gamestate,
                         const Position &position,
                         int heightDisplacement,
                         int rightX,
                         int bottomY,
                         uint32_t tick,
                         Canvas &canvas) {
    const Version &version = gamestate.Version;
    unsigned missileIdx = gamestate.MissileIndex;

    do {
        missileIdx = (missileIdx - 1) % Gamestate::MaxMissiles;

        const auto &missile = gamestate.MissileList[missileIdx];
        uint32_t endTick = missile.StartTick + 200;

        if (endTick < tick) {
            break;
        } else if (missile.Id != 0 && missile.Origin.Z == position.Z) {
            const auto &type = version.GetMissile(missile.Id);

            int relativeThisX, relativeThisY, globalX, globalY;
            float progress;

            progress = (endTick != missile.StartTick)
                               ? (float)(tick - missile.StartTick) /
                                         (float)(endTick - missile.StartTick)
                               : 0.0f;

            globalX = (int)((missile.Origin.X +
                             (missile.Target.X - missile.Origin.X) * progress) *
                            32);
            globalY = (int)((missile.Origin.Y +
                             (missile.Target.Y - missile.Origin.Y) * progress) *
                            32);
            relativeThisX = globalX - position.X * 32;
            relativeThisY = globalY - position.Y * 32;

            if ((relativeThisX <= 0) && (relativeThisY <= 0) &&
                (relativeThisX >= -31) && (relativeThisY >= -31)) {
                DrawMissile(missile,
                            type,
                            rightX - heightDisplacement + relativeThisX,
                            bottomY - heightDisplacement + relativeThisY,
                            canvas);
            }
        }
    } while (missileIdx != gamestate.MissileIndex);
}

static void DrawTile(const Options &options,
                     Gamestate &gamestate,
                     const Position &position,
                     int viewOffsetX,
                     int viewOffsetY,
                     uint32_t tick,
                     bool *redrawNearbyTop,
                     Canvas &canvas) {
    const Version &version = gamestate.Version;

    int heightDisplacement, horizontal, vertical, rightX, bottomY;

    heightDisplacement = 0;
    horizontal = 0;
    vertical = 0;

    rightX = position.X * 32 + viewOffsetX;
    bottomY = position.Y * 32 + viewOffsetY;

    auto &tile = gamestate.Map.Tile(position);

    if (tile.ObjectCount > 0 &&
        GetTileUpdateRenderHeight(gamestate.Version, tile)) {
        gamestate.Map.UpdateRenderHeight(rightX, bottomY, position.Z);
    }

    if (*redrawNearbyTop) {
        /* We're only supposed to redraw the top items, so just calculate the
         * proper height displacement and then get on with it. */
        for (int objectIdx = 0; objectIdx < tile.ObjectCount; objectIdx++) {
            if (tile.Objects[objectIdx].IsCreature()) {
                // objectIdx = tile.creatureEndIndex;
                continue;
            } else {
                const auto &item = tile.Objects[objectIdx];
                const auto &type = version.GetItem(item.Id);

                if (type.Properties.StackPriority != 3) {
                    heightDisplacement = std::min(
                            MAX_HEIGHT_DISPLACEMENT,
                            heightDisplacement + type.Properties.Height);
                    horizontal = horizontal || type.Properties.Horizontal;
                    vertical = vertical || type.Properties.Vertical;
                }
            }
        }
    } else {
        /* Draw things as they were sent by the Tibia server until we hit a
         * creature or stack priority 5.
         *
         * This is not pixel-perfect but it's the only method that offers decent
         * performance in areas with a ton of crap on the ground. */
        for (int objectIdx = 0; objectIdx < tile.ObjectCount &&
                                !tile.Objects[objectIdx].IsCreature();
             objectIdx++) {
            auto &item = tile.Objects[objectIdx];
            const auto &type = version.GetItem(item.Id);

            if (type.Properties.StackPriority > 2) {
                break;
            }

            DrawItem(gamestate.Version,
                     item,
                     type,
                     rightX - heightDisplacement,
                     bottomY - heightDisplacement,
                     tick,
                     position,
                     horizontal,
                     vertical,
                     0,
                     canvas);

            heightDisplacement =
                    std::min(MAX_HEIGHT_DISPLACEMENT,
                             heightDisplacement + type.Properties.Height);
            horizontal = horizontal || type.Properties.Horizontal;
            vertical = vertical || type.Properties.Vertical;
        }

        if (!options.SkipRenderingItems) {
            /* Draw stack priority 5. Unlike the other priorities, its items are
             * to be drawn in reversed order.
             *
             * We can skip drawing once we hit a creature or an item with a
             * lower stack priority is found, and since the Tibia server will
             * ALWAYS handle stack priority 5 correctly this is pixel-perfect
             * by default. */
            for (int objectIdx = tile.ObjectCount - 1;
                 objectIdx >= 0 && !tile.Objects[objectIdx].IsCreature();
                 objectIdx--) {
                auto &item = tile.Objects[objectIdx];
                const auto &type = version.GetItem(item.Id);

                if (type.Properties.StackPriority != 5) {
                    break;
                }

                DrawItem(gamestate.Version,
                         item,
                         type,
                         rightX - heightDisplacement,
                         bottomY - heightDisplacement,
                         tick,
                         position,
                         horizontal,
                         vertical,
                         0,
                         canvas);

                heightDisplacement = std::min<int>(
                        MAX_HEIGHT_DISPLACEMENT,
                        heightDisplacement + type.Properties.Height);
                horizontal = horizontal || type.Properties.Horizontal;
                vertical = vertical || type.Properties.Vertical;

                if (type.Properties.RedrawNearbyTop) {
                    (*redrawNearbyTop) = true;
                }
            }
        }

        if (!options.SkipRenderingCreatures) {
            DrawMovingCreatures(gamestate,
                                position,
                                heightDisplacement,
                                rightX,
                                bottomY,
                                tick,
                                canvas);
        }
    }

    if (!options.SkipRenderingCreatures) {
        /* Draw creatures that are standing on this tile. */
        for (int objectIdx = 0; objectIdx < tile.ObjectCount; objectIdx++) {
            const auto &object = tile.Objects[objectIdx];

            if (!object.IsCreature()) {
                continue;
            }

            /* Strangely, the official client is okay with non-existent
             * creatures here, simply skipping them. */
            if (auto creature = gamestate.FindCreature(object.CreatureId)) {
                UpdateWalkOffset(gamestate, *creature);

                if (creature->MovementInformation.WalkEndTick <= tick) {
                    DrawCreature(
                            gamestate.Version,
                            *creature,
                            rightX - heightDisplacement +
                                    creature->MovementInformation.WalkOffsetX,
                            bottomY - heightDisplacement +
                                    creature->MovementInformation.WalkOffsetY,
                            tick,
                            canvas);
                }
            }
        }
    }

    if (!options.SkipRenderingGraphicalEffects) {
        /* Render all effects on current tile. */
        for (int effectIdx = 0; effectIdx < Tile::MaxEffects; effectIdx++) {
            const auto &effect = tile.GraphicalEffects[effectIdx];

            if (effect.Id > 0) {
                DrawGraphicalEffect(gamestate.Version,
                                    effect,
                                    position,
                                    rightX - heightDisplacement,
                                    bottomY - heightDisplacement,
                                    tick,
                                    canvas);
            }
        }
    }

    if (!options.SkipRenderingMissiles) {
        DrawMissiles(gamestate,
                     position,
                     heightDisplacement,
                     rightX,
                     bottomY,
                     tick,
                     canvas);
    }

    if (!options.SkipRenderingItems) {
        /* Draw stack priority 3. */
        for (int objectIdx = 0; objectIdx < tile.ObjectCount &&
                                !tile.Objects[objectIdx].IsCreature();
             objectIdx++) {
            auto &item = tile.Objects[objectIdx];
            const auto &type = version.GetItem(item.Id);

            if (type.Properties.StackPriority > 3) {
                break;
            } else if (type.Properties.StackPriority == 3) {
                DrawItem(gamestate.Version,
                         item,
                         type,
                         rightX,
                         bottomY,
                         tick,
                         position,
                         horizontal,
                         vertical,
                         0,
                         canvas);

                horizontal = horizontal || type.Properties.Horizontal;
                vertical = vertical || type.Properties.Vertical;
            }
        }
    }
}

static void DrawInventoryItem(Gamestate &gamestate,
                              Object &item,
                              int X,
                              int Y,
                              Canvas &canvas) {
    const Version &version = gamestate.Version;

    const auto &sprite = version.Icons.InventoryBackground;
    canvas.Draw(sprite, X, Y, sprite.Width, sprite.Height);

    if (item.Id != 0) {
        const auto &type = version.GetItem(item.Id);

        DrawItem(gamestate.Version,
                 item,
                 type,
                 X + 32,
                 Y + 32,
                 gamestate.CurrentTick,
                 Position(),
                 0,
                 0,
                 1,
                 canvas);

        if ((type.Properties.Stackable || type.Properties.Rune) &&
            item.ExtraByte > 1) {
            TextRenderer::DrawRightAlignedString(
                    version.Fonts.Game,
                    Pixel(0xBF, 0xBF, 0xBF),
                    X + 32,
                    Y + 22,
                    std::format("{}", item.ExtraByte),
                    canvas);
        }
    }
}

static void DrawInventorySlot(Gamestate &gamestate,
                              InventorySlot slot,
                              int X,
                              int Y,
                              Canvas &canvas) {
    const Version &version = gamestate.Version;

    /* FIXME: C++ migration. */
    Object &object = gamestate.Player.Inventory(slot);

    DrawInventoryItem(gamestate, object, X, Y, canvas);

    if (object.Id == 0) {
        const auto &sprite = version.Icons.GetInventorySlot(slot);
        canvas.Draw(sprite, X, Y, sprite.Width, sprite.Height);
    }
}

void DrawGamestate(const Options &options,
                   Gamestate &gamestate,
                   Canvas &canvas) noexcept {
    int bottomVisibleFloor, topVisibleFloor;
    int viewOffsetX, viewOffsetY;

    auto &playerCreature = gamestate.GetCreature(gamestate.Player.Id);

    /* Force a small amount of light around the player like the Tibia client
     * does. */
    playerCreature.LightIntensity =
            std::max<uint8_t>(playerCreature.LightIntensity, 1);

    UpdateWalkOffset(gamestate, playerCreature);

    viewOffsetX = (8 - gamestate.Map.Position.X) * 32 -
                  playerCreature.MovementInformation.WalkOffsetX;
    viewOffsetY = (6 - gamestate.Map.Position.Y) * 32 -
                  playerCreature.MovementInformation.WalkOffsetY;

    if (gamestate.Map.Position.Z > 7) {
        bottomVisibleFloor = std::min<int>(15, gamestate.Map.Position.Z + 2);
        topVisibleFloor = gamestate.Map.Position.Z;
    } else {
        if (!options.SkipRenderingUpperFloors) {
            topVisibleFloor = GetTopVisibleFloor(gamestate);
        } else {
            topVisibleFloor = gamestate.Map.Position.Z;
        }

        bottomVisibleFloor = 7;
    }

    for (int zIdx = bottomVisibleFloor; zIdx >= topVisibleFloor; zIdx--) {
        int xyOffset = gamestate.Map.Position.Z - zIdx;

        for (int xIdx = 0; xIdx <= 17; xIdx++) {
            for (int yIdx = 0; yIdx <= 13; yIdx++) {
                Position position;
                bool redrawNearbyTop;

                position.X = gamestate.Map.Position.X - 8 + xIdx + xyOffset;
                position.Y = gamestate.Map.Position.Y - 6 + yIdx + xyOffset;
                position.Z = zIdx;

                redrawNearbyTop = false;

                DrawTile(options,
                         gamestate,
                         position,
                         viewOffsetX - xyOffset * 32,
                         viewOffsetY - xyOffset * 32,
                         gamestate.CurrentTick,
                         &redrawNearbyTop,
                         canvas);

                if (redrawNearbyTop) {
                    if (xIdx > 0) {
                        position.X--;

                        DrawTile(options,
                                 gamestate,
                                 position,
                                 viewOffsetX - xyOffset * 32,
                                 viewOffsetY - xyOffset * 32,
                                 gamestate.CurrentTick,
                                 &redrawNearbyTop,
                                 canvas);
                    }

                    if (yIdx > 0) {
                        position.Y--;

                        DrawTile(options,
                                 gamestate,
                                 position,
                                 viewOffsetX - xyOffset * 32,
                                 viewOffsetY - xyOffset * 32,
                                 gamestate.CurrentTick,
                                 &redrawNearbyTop,
                                 canvas);

                        position.X++;

                        DrawTile(options,
                                 gamestate,
                                 position,
                                 viewOffsetX - xyOffset * 32,
                                 viewOffsetY - xyOffset * 32,
                                 gamestate.CurrentTick,
                                 &redrawNearbyTop,
                                 canvas);

                        position.Y++;
                    }

                    DrawTile(options,
                             gamestate,
                             position,
                             viewOffsetX - xyOffset * 32,
                             viewOffsetY - xyOffset * 32,
                             gamestate.CurrentTick,
                             &redrawNearbyTop,
                             canvas);
                }
            }
        }
    }
}

static void DrawNumericalEffects(Gamestate &gamestate,
                                 Canvas &canvas,
                                 int viewOffsetX,
                                 int viewOffsetY,
                                 float scaleX,
                                 float scaleY,
                                 Position &position,
                                 const Tile &tile) {
    unsigned effectShuntX, effectShuntY;
    unsigned effectIdx;

    effectShuntX = 0;
    effectShuntY = 0;

    effectIdx = tile.NumericalIndex;
    do {
        effectIdx = ((unsigned)(effectIdx)-1) % Tile::MaxEffects;
        const auto &effect = tile.NumericalEffects[effectIdx];

        if ((effect.StartTick + 750) < gamestate.CurrentTick) {
            break;
        } else if (effect.Value != 0) {
            unsigned textCenterX, textCenterY;

            textCenterX = ((position.X * 32 + viewOffsetX - 16) * scaleX) + 2 +
                          effectShuntX;
            textCenterY = ((position.Y * 32 + viewOffsetY - 32) * scaleY) + 2 -
                          effectShuntY;
            textCenterY -=
                    (((gamestate.CurrentTick - effect.StartTick) / 750.0f) *
                     32.0f);

            /* Yes, they actually do get shunted this far. */
            effectShuntX += 2 + (int)(scaleX * 9.0f);
            effectShuntY += 0;

            TextRenderer::DrawCenteredString(gamestate.Version.Fonts.Game,
                                             Convert8BitColor(effect.Color),
                                             textCenterX,
                                             textCenterY,
                                             std::format("{}", effect.Value),
                                             canvas);
        }
    } while (effectIdx != tile.NumericalIndex);
}

static void DrawCreatureOverlay(const Options &options,
                                Gamestate &gamestate,
                                Canvas &canvas,
                                int isObscured,
                                int heightDisplacement,
                                int rightX,
                                int bottomY,
                                float scaleX,
                                float scaleY,
                                Creature &creature) {
    const Version &version = gamestate.Version;
    int creatureRX, creatureBY;

    auto infoColor = GetCreatureInfoColor(creature.Health, isObscured);

    creatureRX = rightX - heightDisplacement;
    creatureBY = bottomY - heightDisplacement;

    if (creature.MovementInformation.WalkEndTick >= gamestate.CurrentTick) {
        creatureRX += creature.MovementInformation.WalkOffsetX;
        creatureBY += creature.MovementInformation.WalkOffsetY;
    }

    if (creature.Outfit.Id != 0) {
        if (creature.Outfit.MountId == 0) {
            const auto &outfit = version.GetOutfit(creature.Outfit.Id);

            creatureRX -= outfit.Properties.DisplacementX;
            creatureBY -= outfit.Properties.DisplacementY;
        } else {
            const auto &mount = version.GetOutfit(creature.Outfit.MountId);

            creatureRX -= mount.Properties.DisplacementX;
            creatureBY -= mount.Properties.DisplacementY;
        }
    } else if (creature.Outfit.Item.Id == 0 &&
               creature.Type != CreatureType::Player) {
        /* Invisible monster, skip it altogether. These stopped being sent
         * altogether in recent versions to prevent client-side cheats. */
        return;
    } else {
        creatureRX -= 8;
        creatureBY -= 8;
    }

    if (!(options.SkipRenderingPlayerNames &&
          creature.Type == CreatureType::Player) &&
        !(options.SkipRenderingNonPlayerNames &&
          creature.Type != CreatureType::Player)) {
        int nameCenterX, nameCenterY;

        nameCenterX = std::max(2.f, (creatureRX - 32) * scaleX + (16 * scaleX));
        nameCenterY = std::max(2.f, (creatureBY - 32) * scaleY - 16);

        TextRenderer::DrawCenteredProperCaseString(version.Fonts.Game,
                                                   infoColor,
                                                   nameCenterX,
                                                   nameCenterY,
                                                   creature.Name,
                                                   canvas);
    }

    if (!options.SkipRenderingCreatureHealthBars) {
        int healthBarAbsoluteX, healthBarAbsoluteY;

        healthBarAbsoluteX =
                std::max(2.f, (creatureRX - 32) * scaleX + (16 * scaleX) - 14);
        healthBarAbsoluteY = std::max(14.f, (creatureBY - 32) * scaleY - 4);

        canvas.DrawRectangle(Pixel(0, 0, 0),
                             healthBarAbsoluteX + 0,
                             healthBarAbsoluteY + 0,
                             27,
                             4);
        canvas.DrawRectangle(infoColor,
                             healthBarAbsoluteX + 1,
                             healthBarAbsoluteY + 1,
                             creature.Health / 4,
                             2);
    }

    if (!options.SkipRenderingCreatureIcons) {
        int iconAbsoluteX, iconAbsoluteY;

        iconAbsoluteX =
                std::max(2.f, (creatureRX - 32) * scaleX + (16 * scaleX) + 9);
        iconAbsoluteY = std::max(2.f, (creatureBY - 32) * scaleY + 1);

        switch (creature.Type) {
        case CreatureType::NPC:
            /* TODO: render those newfangled NPC icon thingies.
             *
             * Cycle time between dual icons seems to be about a second or
             * something. */
            break;
        case CreatureType::SummonOthers:
        case CreatureType::SummonOwn: {
            /* Currently only these creature types have a type sprite. */
            const auto &sprite = version.Icons.GetCreatureType(creature.Type);
            canvas.Draw(sprite,
                        iconAbsoluteX,
                        iconAbsoluteY,
                        sprite.Width,
                        sprite.Height);
            break;
        }
        case CreatureType::Player:
            /* Only players have this kind of stuff. */

            if (creature.Shield != PartyShield::None) {
                switch (creature.Shield) {
                case PartyShield::YellowNoSharedExpBlink:
                case PartyShield::BlueNoSharedExpBlink:
                    if (gamestate.CurrentTick % 1000 >= 500) {
                        break;
                    }
                    [[fallthrough]];
                default:
                    const auto &sprite =
                            version.Icons.GetPartyShield(creature.Shield);

                    /* If a skull is present, it will push the shield to the
                     * left. */
                    auto iconOffset =
                            (creature.Skull != CharacterSkull::None) ? -13 : 0;

                    canvas.Draw(sprite,
                                iconAbsoluteX + iconOffset,
                                iconAbsoluteY,
                                sprite.Width,
                                sprite.Height);

                    break;
                }
            }

            if (creature.Skull != CharacterSkull::None) {
                const auto &sprite =
                        version.Icons.GetCharacterSkull(creature.Skull);
                canvas.Draw(sprite,
                            iconAbsoluteX,
                            iconAbsoluteY,
                            sprite.Width,
                            sprite.Height);
            }

            if (creature.Shield != PartyShield::None ||
                creature.Skull != CharacterSkull::None) {
                iconAbsoluteY += 13;
            }

            if (creature.War != WarIcon::None) {
                const auto &sprite = version.Icons.GetWarIcon(creature.War);

                canvas.Draw(sprite,
                            iconAbsoluteX,
                            iconAbsoluteY,
                            sprite.Width,
                            sprite.Height);

                iconAbsoluteY += 13;
            }

            if (creature.GuildMembersOnline >= 5) {
                const auto &sprite = version.Icons.RiskyIcon;
                canvas.Draw(sprite,
                            iconAbsoluteX,
                            iconAbsoluteY,
                            sprite.Width,
                            sprite.Height);
            }
        case CreatureType::Monster:
            break;
        }
    }
}

static bool DrawTileOverlay(const Options &options,
                            Gamestate &gamestate,
                            Canvas &canvas,
                            int isObscured,
                            int rightX,
                            int bottomY,
                            float scaleX,
                            float scaleY,
                            Tile &tile) {
    const Version &version = gamestate.Version;
    int heightDisplacement = 0;

    /* Get the height displacement of the current tile */
    for (int objectIdx = 0; objectIdx < tile.ObjectCount; objectIdx++) {
        const auto &object = tile.Objects[objectIdx];

        if (object.IsCreature()) {
            continue;
        }

        const auto &item = version.GetItem(object.Id);

        if (item.Properties.StackPriority != 3) {
            heightDisplacement =
                    std::min(MAX_HEIGHT_DISPLACEMENT,
                             heightDisplacement + item.Properties.Height);
        }
    }

    /* Draw info of creatures that belong on this tile */
    for (int objectIdx = 0; objectIdx < tile.ObjectCount; objectIdx++) {
        const auto &object = tile.Objects[objectIdx];

        if (!object.IsCreature()) {
            continue;
        }

        /* Strangely, the official client is okay with non-existent
         * creatures here, simply skipping them. */
        if (auto creature = gamestate.FindCreature(object.CreatureId)) {
            if (creature->Health > 0) {
                DrawCreatureOverlay(options,
                                    gamestate,
                                    canvas,
                                    isObscured,
                                    heightDisplacement,
                                    rightX,
                                    bottomY,
                                    scaleX,
                                    scaleY,
                                    *creature);
            }
        }
    }

    return true;
}

static bool DrawMapOverlay(const Options &options,
                           Gamestate &gamestate,
                           Canvas &canvas,
                           int viewOffsetX,
                           int viewOffsetY,
                           float scaleX,
                           float scaleY) {
    for (int xIdx = 0; xIdx < Map::TileBufferWidth; xIdx++) {
        for (int yIdx = 0; yIdx < Map::TileBufferHeight; yIdx++) {
            int isObscured, rightX, bottomY;
            Position position;

            position.X = gamestate.Map.Position.X - 8 + xIdx;
            position.Y = gamestate.Map.Position.Y - 6 + yIdx;
            position.Z = gamestate.Map.Position.Z;

            rightX = position.X * 32 + viewOffsetX;
            bottomY = position.Y * 32 + viewOffsetY;

            if (rightX > (NativeResolutionX + 32) ||
                bottomY > (NativeResolutionY + 32)) {
                continue;
            } else if (rightX <= -32 || bottomY <= -32) {
                continue;
            }

            auto &tile = gamestate.Map.Tile(position);

            /* Only show status for creatures on tiles that are completely
             * visible. */
            if ((yIdx > 0 && yIdx < (Map::TileBufferHeight - 2)) &&
                (xIdx > 0 && xIdx < (Map::TileBufferWidth - 1))) {
                if (!options.SkipRenderingUpperFloors && position.Z <= 7) {
                    int renderHeight =
                            gamestate.Map.GetRenderHeight(rightX, bottomY);

                    isObscured = renderHeight < position.Z;
                } else {
                    isObscured = 0;
                }

                DrawTileOverlay(options,
                                gamestate,
                                canvas,
                                isObscured,
                                rightX,
                                bottomY,
                                scaleX,
                                scaleY,
                                tile);
            }

            if (!options.SkipRenderingNumericalEffects) {
                DrawNumericalEffects(gamestate,
                                     canvas,
                                     viewOffsetX,
                                     viewOffsetY,
                                     scaleX,
                                     scaleY,
                                     position,
                                     tile);
            }
        }
    }

    return true;
}

static int MessageColor(MessageMode mode) {
    switch (mode) {
    case MessageMode::Say:
    case MessageMode::Spell:
    case MessageMode::Whisper:
    case MessageMode::Yell:
        return 210;
    case MessageMode::MonsterSay:
#ifdef DEBUG
        /* Helps distinguish between monster say/yell when figuring out speak
         * types. */
        return 10;
#endif
    case MessageMode::MonsterYell:
        return 192;
    case MessageMode::NPCStart:
        /* Light-blue, above creature */
        return 35;
    case MessageMode::Game:
        /* White, center screen */
        return 215;
    case MessageMode::PrivateIn:
        /* Light-blue, top-center screen */
        return 35;
    case MessageMode::Warning:
        /* Red, center screen */
        return 194;
    case MessageMode::Hotkey:
#ifdef DEBUG
        /* Helps distinguish between hotkey/look when figuring out speak
         * types. */
        return 10;
#endif
    case MessageMode::NPCTrade:
    case MessageMode::Guild:
    case MessageMode::Loot:
    case MessageMode::Look:
        /* Green, center screen */
        return 30;
    case MessageMode::Failure:
    case MessageMode::Status:
    case MessageMode::Login:
        /* White, bottom-center screen */
        return 215;
    default:
        /* Return something fugly so it gets reported. */
        return 10;
    }
}

static bool DrawMessages(const Options &options,
                         Gamestate &gamestate,
                         Canvas &canvas,
                         int viewOffsetX,
                         int viewOffsetY,
                         float scaleX,
                         float scaleY) {
    const Version &version = gamestate.Version;
    Pixel messageColor(0, 0, 0);

    bool preserveCoordinates, canMerge;
    bool drawnWhiteNotification;
    bool drawnGreenNotification;
    bool drawnRedNotification;
    bool drawnPrivateMessage;
    bool drawnStatusMessage;
    int drawnMessages;
    int centerX, bottomY;

    drawnWhiteNotification = 0;
    drawnGreenNotification = 0;
    drawnRedNotification = 0;
    drawnPrivateMessage = 0;
    drawnStatusMessage = 0;
    preserveCoordinates = 0;
    drawnMessages = 0;

    centerX = 0;
    bottomY = 0;

    for (auto message = gamestate.Messages.begin();
         message != gamestate.Messages.end();
         message = std::next(message)) {
        TextTransform transform = TextTransform::None;
        int lineMaxLength = 39;

        switch (message->Type) {
        case MessageMode::Game:
            if (drawnWhiteNotification) {
                preserveCoordinates = 0;
                continue;
            }

            drawnWhiteNotification = true;
            break;
        case MessageMode::Warning:
            if (drawnRedNotification) {
                preserveCoordinates = 0;
                continue;
            }

            drawnRedNotification = true;
            break;
        case MessageMode::Spell:
            if (options.SkipRenderingSpellMessages) {
                continue;
            }
            break;
        case MessageMode::Hotkey:
            if (options.SkipRenderingHotkeyMessages || drawnGreenNotification) {
                preserveCoordinates = 0;
                continue;
            }

            drawnGreenNotification = true;
            break;
        case MessageMode::Loot:
            if (options.SkipRenderingLootMessages || drawnGreenNotification) {
                preserveCoordinates = 0;
                continue;
            }

            drawnGreenNotification = true;
            break;
        case MessageMode::NPCTrade:
        case MessageMode::Guild:
        case MessageMode::Look:
            if (drawnGreenNotification) {
                preserveCoordinates = 0;
                continue;
            }

            drawnGreenNotification = true;
            break;
        case MessageMode::PrivateIn:
            if (options.SkipRenderingPrivateMessages || drawnPrivateMessage) {
                preserveCoordinates = 0;
                continue;
            }

            drawnPrivateMessage = true;
            break;
        case MessageMode::Failure:
        case MessageMode::Status:
        case MessageMode::Login:
            if (options.SkipRenderingStatusMessages || drawnStatusMessage) {
                preserveCoordinates = 0;
                continue;
            }

            drawnStatusMessage = true;
            break;
        case MessageMode::Yell:
            AbortUnless(transform == TextTransform::None);
            transform = TextTransform::UpperCase;
            break;
        case MessageMode::NPCStart:
            AbortUnless(transform == TextTransform::None);
            transform = TextTransform::Highlight;
            break;
        default:
            break;
        }

        drawnMessages++;

        if (preserveCoordinates) {
            if (drawnMessages > 8) {
                preserveCoordinates = 0;
                continue;
            }
        } else {
            messageColor = Pixel::TextColor(MessageColor(message->Type));

            switch (message->Type) {
            case MessageMode::NPCStart:
            case MessageMode::Say:
            case MessageMode::Spell:
            case MessageMode::Whisper:
            case MessageMode::Yell:
                centerX = std::min(
                        std::max(2.f,
                                 (message->Position.X * 32 + viewOffsetX - 16) *
                                         scaleX),
                        (NativeResolutionX * scaleX));
                bottomY = std::min(
                        std::max(2.f,
                                 (message->Position.Y * 32 + viewOffsetY - 32) *
                                         scaleY),
                        (NativeResolutionY * scaleY));

                /* Shift the message a bit above the health bar. */
                bottomY -= (int)(8 * scaleY);
                break;
            case MessageMode::MonsterSay:
            case MessageMode::MonsterYell:
                centerX = std::min(
                        std::max(2.f,
                                 (message->Position.X * 32 + viewOffsetX - 16) *
                                         scaleX),
                        (NativeResolutionX * scaleX));
                bottomY = std::min(
                        std::max(2.f,
                                 (message->Position.Y * 32 + viewOffsetY - 32) *
                                         scaleY),
                        (NativeResolutionY * scaleY));
                break;
            case MessageMode::Game:
                /* White, center screen above player character */
                centerX = std::min(std::max(2.f, (scaleX * 32) * 15 / 2),
                                   (NativeResolutionX * scaleX));
                bottomY = std::min(std::max(2.f, (scaleY * 32) * 11 / 2) - 32,
                                   (NativeResolutionY * scaleY));
                break;
            case MessageMode::PrivateIn:
                /* Light-blue, top-center screen */
                centerX = std::min(std::max(2.f, (scaleX * 32) * 15 / 2),
                                   (NativeResolutionX * scaleX));
                bottomY = std::min(std::max(2.f, (scaleY * 32) * 11 / 2) -
                                           (scaleY * 128),
                                   (NativeResolutionY * scaleY));
                break;
            case MessageMode::Warning:
            case MessageMode::Hotkey:
            case MessageMode::NPCTrade:
            case MessageMode::Guild:
            case MessageMode::Loot:
            case MessageMode::Look:
                /* Center screen */
                centerX = std::min(std::max(2.f, (scaleX * 32) * 15 / 2),
                                   (NativeResolutionX * scaleX));
                bottomY = std::min(std::max(2.f, (scaleY * 32) * 11 / 2),
                                   (NativeResolutionY * scaleY));
                break;
            case MessageMode::Failure:
            case MessageMode::Status:
            case MessageMode::Login:
                /* Bottom-center screen */
                centerX = std::min(std::max(2.f, (scaleX * 32) * 15 / 2),
                                   (NativeResolutionX * scaleX));
                bottomY = NativeResolutionY * scaleY;
                lineMaxLength = INT16_MAX;
                break;
            default:
                break;
            }
        }

        auto [textWidth, textHeight] =
                TextRenderer::MeasureBounds(version.Fonts.Game,
                                            transform,
                                            lineMaxLength,
                                            message->Text);

        bottomY -= textHeight;

        TextRenderer::Render(version.Fonts.Game,
                             TextAlignment::Center,
                             transform,
                             messageColor,
                             centerX,
                             bottomY,
                             lineMaxLength,
                             message->Text,
                             canvas);

        std::tie(preserveCoordinates, canMerge) =
                gamestate.Messages.QueryNext(message);

        if (!canMerge) {
            const char *messagePrefix = NULL;

            switch (message->Type) {
            case MessageMode::Whisper:
                messagePrefix = "whispers";
                break;
            case MessageMode::Yell:
                if (options.SkipRenderingYellingMessages) {
                    break;
                }

                messagePrefix = "yells";
                break;
            case MessageMode::NPCStart:
            case MessageMode::PrivateIn:
            case MessageMode::Say:
            case MessageMode::Spell:
                messagePrefix = "says";
                break;
            default:
                break;
            }

            if (messagePrefix != NULL) {
                bottomY -= version.Fonts.Game.Height;

                TextRenderer::DrawCenteredString(
                        version.Fonts.Game,
                        messageColor,
                        centerX,
                        bottomY,
                        std::format("{} {}:", message->Author, messagePrefix),
                        canvas);
            }
        }
    }

    return true;
}

void DrawOverlay(const Options &options,
                 Gamestate &gamestate,
                 Canvas &canvas) noexcept {
    int viewOffsetX, viewOffsetY;
    float scaleX, scaleY;

    auto &playerCreature = gamestate.GetCreature(gamestate.Player.Id);

    viewOffsetX = (8 - gamestate.Map.Position.X) * 32 -
                  playerCreature.MovementInformation.WalkOffsetX;
    viewOffsetY = (6 - gamestate.Map.Position.Y) * 32 -
                  playerCreature.MovementInformation.WalkOffsetY;

    scaleX = canvas.Width / (float)NativeResolutionX;
    scaleY = canvas.Height / (float)NativeResolutionY;

    if (!options.SkipRenderingCreatures) {
        DrawMapOverlay(options,
                       gamestate,
                       canvas,
                       viewOffsetX,
                       viewOffsetY,
                       scaleX,
                       scaleY);
    }

    if (!options.SkipRenderingMessages) {
        DrawMessages(options,
                     gamestate,
                     canvas,
                     viewOffsetX,
                     viewOffsetY,
                     scaleX,
                     scaleY);
    }
}

void DrawIconBar(Gamestate &gamestate,
                 Canvas &canvas,
                 int &offsetX,
                 int &offsetY) noexcept {
    const Icons &icons = gamestate.Version.Icons;

    int baseX = offsetX, baseY = offsetY;
    int iconOffset;

    auto &playerCreature = gamestate.GetCreature(gamestate.Player.Id);

    canvas.Draw(icons.IconBarBackground,
                baseX + 16,
                baseY,
                icons.IconBarBackground.Width,
                icons.IconBarBackground.Height);
    iconOffset = baseX + 2 + 16;

    for (auto icon : StatusIcons) {
        if ((gamestate.Player.Icons & icon) == icon) {
            /* Don't render both swords and pz block at the same time. */
            if (icon == StatusIcon::Swords &&
                ((gamestate.Player.Icons & StatusIcon::PZBlock) ==
                 StatusIcon::PZBlock)) {
                continue;
            }

            const auto &sprite = icons.GetStatusIcon(icon);

            canvas.Draw(sprite,
                        iconOffset,
                        baseY + 2,
                        sprite.Width,
                        sprite.Height);

            iconOffset += sprite.Width;
        }
    }

    /* Draw party/skulls/waricon, if any. */
    if (playerCreature.Skull != CharacterSkull::None) {
        const auto &sprite = icons.GetCharacterSkull(playerCreature.Skull);

        canvas.Draw(sprite, iconOffset, baseY + 2, sprite.Width, sprite.Height);

        iconOffset += sprite.Width;
    }

    if (playerCreature.War == WarIcon::Ally) {
        const auto &sprite = icons.IconBarWar;

        canvas.Draw(sprite, iconOffset, baseY + 2, sprite.Width, sprite.Height);

        /* iconOffset += sprite.Width; */
    }

    offsetY = baseY + 2 + icons.IconBarBackground.Height;
}

static void DrawIconArea(Gamestate &gamestate,
                         Canvas &canvas,
                         int offsetX,
                         int offsetY) {
    const Icons &icons = gamestate.Version.Icons;

    int iconAreaLimitX, iconAreaX, iconAreaY;
    int iconOffsetX, iconOffsetY;

    auto &playerCreature = gamestate.GetCreature(gamestate.Player.Id);

    iconAreaX = 16 + offsetX;
    iconAreaY = offsetY + 125;
    iconAreaLimitX = iconAreaX + icons.SecondaryStatBackground.Width;

    canvas.Draw(icons.SecondaryStatBackground,
                iconAreaX,
                iconAreaY,
                icons.SecondaryStatBackground.Width,
                icons.SecondaryStatBackground.Height);
    iconAreaX += 1;
    iconAreaY += 1;

    iconOffsetX = iconAreaX;
    iconOffsetY = iconAreaY;

    for (auto icon : StatusIcons) {
        if ((gamestate.Player.Icons & icon) == icon) {
            /* Don't render both swords and pz block at the same time. */
            if (icon == StatusIcon::Swords &&
                ((gamestate.Player.Icons & StatusIcon::PZBlock) ==
                 StatusIcon::PZBlock)) {
                continue;
            }

            const auto &sprite = icons.GetStatusIcon(icon);

            canvas.Draw(sprite,
                        iconOffsetX,
                        iconOffsetY,
                        sprite.Width,
                        sprite.Height);

            iconOffsetX += sprite.Width + 2;

            if ((iconOffsetX + sprite.Width) >= iconAreaLimitX) {
                iconOffsetX = iconAreaX;
                iconOffsetY += sprite.Height;
            }
        }
    }

    /* Draw party/skulls, if any. */
    if (playerCreature.Skull != CharacterSkull::None) {
        const auto &sprite = icons.GetIconBarSkull(playerCreature.Skull);

        canvas.Draw(sprite,
                    iconOffsetX,
                    iconOffsetY,
                    sprite.Width,
                    sprite.Height);
    }

    /* We'll skip war icon since it's not present in the versions using the
     * icon area. */
}

void DrawStatusBars(Gamestate &gamestate,
                    Canvas &canvas,
                    int &offsetX,
                    int &offsetY) noexcept {
    const Version &version = gamestate.Version;
    const Icons &icons = version.Icons;

    int baseX = offsetX, baseY = offsetY;
    int statusBarX, statusBarY;

    statusBarX = baseX + 24;
    statusBarY = baseY;

    canvas.Draw(icons.HealthIcon,
                baseX,
                baseY + 2,
                icons.HealthIcon.Width,
                icons.HealthIcon.Height);
    canvas.Draw(icons.ManaIcon,
                baseX,
                baseY + 15,
                icons.ManaIcon.Width,
                icons.ManaIcon.Height);

    /* Draw health and mana bar background. */
    canvas.Draw(icons.EmptyStatusBar,
                statusBarX + 2,
                statusBarY + 2,
                icons.EmptyStatusBar.Width,
                icons.EmptyStatusBar.Height);
    canvas.Draw(icons.EmptyStatusBar,
                statusBarX + 2,
                statusBarY + 15,
                icons.EmptyStatusBar.Width,
                icons.EmptyStatusBar.Height);

    if (gamestate.Player.Stats.MaxHealth > 0 &&
        gamestate.Player.Stats.Health <= gamestate.Player.Stats.MaxHealth) {
        canvas.Draw(icons.HealthBar,
                    statusBarX + 2,
                    statusBarY + 2,
                    (icons.HealthBar.Width * gamestate.Player.Stats.Health) /
                            gamestate.Player.Stats.MaxHealth,
                    11);
    }

    if (gamestate.Player.Stats.MaxMana > 0 &&
        gamestate.Player.Stats.Mana <= gamestate.Player.Stats.MaxMana) {
        canvas.Draw(icons.ManaBar,
                    statusBarX + 2,
                    statusBarY + 15,
                    (icons.ManaBar.Width * gamestate.Player.Stats.Mana) /
                            gamestate.Player.Stats.MaxMana,
                    11);
    }

    TextRenderer::DrawCenteredString(
            version.Fonts.Game,
            Pixel(0xFF, 0xFF, 0xFF),
            statusBarX + 2 + icons.HealthBar.Width / 2,
            statusBarY + 2,
            std::format("{} / {}",
                        gamestate.Player.Stats.Health,
                        gamestate.Player.Stats.MaxHealth),
            canvas);

    TextRenderer::DrawCenteredString(
            version.Fonts.Game,
            Pixel(0xFF, 0xFF, 0xFF),
            statusBarX + 2 + icons.ManaBar.Width / 2,
            statusBarY + 15,
            std::format("{} / {}",
                        gamestate.Player.Stats.Mana,
                        gamestate.Player.Stats.MaxMana),
            canvas);

    /* Update the render position */
    baseY += 18 + icons.EmptyStatusBar.Height;

    offsetY = baseY;
}

void DrawInventoryArea(Gamestate &gamestate,
                       Canvas &canvas,
                       int &offsetX,
                       int &offsetY) noexcept {
    const Version &version = gamestate.Version;
    const Icons &icons = version.Icons;

    int baseX = offsetX, baseY = offsetY;

    for (auto [slot, x, y] :
         std::initializer_list<std::tuple<InventorySlot, int, int>>{
                 {InventorySlot::Head, 53, 0},
                 {InventorySlot::Amulet, 16, 13},
                 {InventorySlot::Backpack, 90, 13},
                 {InventorySlot::Chest, 53, 37},
                 {InventorySlot::RightArm, 90, 50},
                 {InventorySlot::LeftArm, 16, 50},
                 {InventorySlot::Legs, 53, 74},
                 {InventorySlot::Boots, 53, 111},
                 {InventorySlot::Ring, 16, 87},
                 {InventorySlot::Quiver, 90, 87},
         }) {
        DrawInventorySlot(gamestate, slot, baseX + x, baseY + y, canvas);
    }

    if (!version.Features.IconBar) {
        DrawIconArea(gamestate, canvas, baseX, baseY);
    }

    baseY += 124;

    if (version.Features.IconBar) {
        /* The small client font doesn't do bordering or tinting, so
         * we're not passing any colors. */
        canvas.Draw(icons.SecondaryStatBackground,
                    16 + baseX,
                    baseY,
                    icons.SecondaryStatBackground.Width,
                    icons.SecondaryStatBackground.Height);

        TextRenderer::DrawCenteredString(version.Fonts.InterfaceSmall,
                                         Pixel(0xFF, 0xFF, 0xFF),
                                         16 + baseX + 17,
                                         baseY + 2,
                                         "Soul:",
                                         canvas);

        TextRenderer::DrawCenteredString(
                version.Fonts.InterfaceLarge,
                Pixel(0xAF, 0xAF, 0xAF),
                16 + baseX + 17,
                baseY + 10,
                std::format("{}", gamestate.Player.Stats.SoulPoints),
                canvas);
    }

    canvas.Draw(icons.SecondaryStatBackground,
                16 + baseX + 74,
                baseY,
                icons.SecondaryStatBackground.Width,
                icons.SecondaryStatBackground.Height);
    TextRenderer::DrawCenteredString(version.Fonts.InterfaceSmall,
                                     Pixel(0xFF, 0xFF, 0xFF),
                                     16 + baseX + 90,
                                     baseY + 2,
                                     "Cap:",
                                     canvas);

    uint32_t capacity =
            gamestate.Player.Stats.Capacity / version.Features.CapacityDivisor;
    TextRenderer::DrawCenteredString(version.Fonts.InterfaceLarge,
                                     Pixel(0xAF, 0xAF, 0xAF),
                                     16 + baseX + 90,
                                     baseY + 10,
                                     std::format("{}", capacity),
                                     canvas);

    /* Update the render position */
    offsetY = baseY + icons.SecondaryStatBackground.Height + 3;
}

void DrawContainer(Gamestate &gamestate,
                   Canvas &canvas,
                   Container &container,
                   bool collapsed,
                   int maxX,
                   int maxY,
                   int &offsetX,
                   int &offsetY) noexcept {
    const Version &version = gamestate.Version;
    const Icons &icons = version.Icons;

    int baseX = offsetX, baseY = offsetY;

    TextRenderer::DrawProperCaseString(version.Fonts.InterfaceLarge,
                                       Pixel(0xBF, 0xBF, 0xBF),
                                       baseX,
                                       baseY + 2,
                                       container.Name,
                                       canvas);

    baseY += version.Fonts.InterfaceLarge.Height;

    if (!collapsed) {
        const int slotSize = (32 + 4);
        const int slotsPerRow = (maxX - baseX) / slotSize;
        auto itemIdx = 0u;

        for (; itemIdx < container.SlotsPerPage; itemIdx++) {
            int slotX, slotY;

            slotX = baseX + (itemIdx % slotsPerRow) * slotSize;
            slotY = baseY + (itemIdx / slotsPerRow) * slotSize;

            if (slotX > maxX || slotY > maxY) {
                break;
            }

            if (itemIdx >= container.Items.size()) {
                if (itemIdx % slotsPerRow == 0 && itemIdx > 0) {
                    break;
                } else {
                    canvas.Draw(icons.InventoryBackground,
                                slotX,
                                slotY,
                                icons.InventoryBackground.Width,
                                icons.InventoryBackground.Height);
                }
            } else {
                DrawInventoryItem(gamestate,
                                  container.Items[itemIdx],
                                  slotX,
                                  slotY,
                                  canvas);
            }
        }

        /* Round up to avoid having the next container overdraw this one, in
         * case the container size wasn't a clean multiple of the slot
         * modulus. */
        baseY += ((itemIdx + slotsPerRow - 1) / slotsPerRow) * slotSize;
    }

    offsetY = baseY;
}

void DrawClientBackground(Gamestate &gamestate,
                          Canvas &canvas,
                          int leftX,
                          int topY,
                          int rightX,
                          int bottomY) noexcept {
    const auto &sprite = gamestate.Version.Icons.ClientBackground;

    for (int toY = topY; toY < bottomY; toY += sprite.Height) {
        for (int toX = leftX; toX < rightX; toX += sprite.Width) {
            canvas.Draw(sprite,
                        toX,
                        toY,
                        std::min(sprite.Width, rightX - toX),
                        std::min(sprite.Height, bottomY - toY));
        }
    }
}

void DumpItem(Version &version, uint16_t item, Canvas &canvas) noexcept {
    Object object(item);
    const auto &type = version.GetItem(item);

    object.PhaseTick = 0;
    object.ExtraByte = 1;
    object.Mark = 0;

    DrawItem(version,
             object,
             type,
             64,
             64,
             0,
             Position(1024, 1024, 7),
             0,
             0,
             1,
             canvas);
}

} // namespace Renderer
} // namespace trc
