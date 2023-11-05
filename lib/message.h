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

#ifndef __TRC_MESSAGE_H__
#define __TRC_MESSAGE_H__

#include <stdbool.h>
#include <stdint.h>

#include "position.h"

#define MESSAGE_DISPLAY_TIME 3000
#define MESSAGE_MAX_TEXT_LENGTH 256
#define MESSAGE_MAX_AUTHOR_LENGTH 64

/* Priority-ordered canonical message modes, lower values have higher display
 * precedence. */
enum TrcMessageMode {
    MESSAGEMODE_PRIVATE_IN = 0,
    MESSAGEMODE_PRIVATE_OUT,

    MESSAGEMODE_SAY,
    MESSAGEMODE_WHISPER,
    MESSAGEMODE_YELL,

    MESSAGEMODE_CHANNEL_WHITE,
    MESSAGEMODE_CHANNEL_YELLOW,
    MESSAGEMODE_CHANNEL_ORANGE,

    MESSAGEMODE_SPELL,

    MESSAGEMODE_NPC_START,
    MESSAGEMODE_NPC_CONTINUED,
    MESSAGEMODE_PLAYER_TO_NPC,

    MESSAGEMODE_BROADCAST,

    MESSAGEMODE_CHANNEL_RED,

    MESSAGEMODE_GM_TO_PLAYER,
    MESSAGEMODE_PLAYER_TO_GM,

    MESSAGEMODE_LOGIN,
    MESSAGEMODE_ADMIN,
    MESSAGEMODE_GAME,
    MESSAGEMODE_FAILURE,

    MESSAGEMODE_LOOK,

    MESSAGEMODE_DAMAGE_DEALT,

    MESSAGEMODE_DAMAGE_RECEIVED,
    MESSAGEMODE_HEALING,
    MESSAGEMODE_EXPERIENCE,

    MESSAGEMODE_DAMAGE_RECEIVED_OTHERS,
    MESSAGEMODE_HEALING_OTHERS,
    MESSAGEMODE_EXPERIENCE_OTHERS,

    MESSAGEMODE_STATUS,
    MESSAGEMODE_LOOT,
    MESSAGEMODE_NPC_TRADE,
    MESSAGEMODE_GUILD,

    MESSAGEMODE_PARTY_WHITE,

    MESSAGEMODE_PARTY,

    MESSAGEMODE_MONSTER_SAY,
    MESSAGEMODE_MONSTER_YELL,

    MESSAGEMODE_REPORT,
    MESSAGEMODE_HOTKEY,

    MESSAGEMODE_TUTORIAL,
    MESSAGEMODE_THANK_YOU,
    MESSAGEMODE_MARKET,

    MESSAGEMODE_MANA,

    MESSAGEMODE_WARNING
};

struct trc_message_list {
    struct trc_message_list *Next, *Previous;
};

struct trc_message {
    struct trc_message_list Chain;

    uint32_t StartTick;
    uint32_t EndTick;

    struct trc_position Position;
    enum TrcMessageMode Type;

    uint16_t AuthorLength;
    char Author[MESSAGE_MAX_AUTHOR_LENGTH];

    uint16_t TextLength;
    char Text[MESSAGE_MAX_TEXT_LENGTH];
};

/* Iterates and prunes the message list of expired messages. */
bool messagelist_Sweep(struct trc_message_list *sentinel,
                       uint32_t tick,
                       struct trc_message **iterator);

void messagelist_AddMessage(struct trc_message_list *sentinel,
                            struct trc_position *position,
                            uint32_t tick,
                            enum TrcMessageMode type,
                            uint16_t nameLength,
                            const char *name,
                            uint16_t textLength,
                            const char *text);
void messagelist_QueryNext(struct trc_message_list *sentinel,
                           struct trc_message *current,
                           uint32_t tick,
                           int *preserveCoordinates,
                           int *canMerge);

void messagelist_Initialize(struct trc_message_list *sentinel);
void messagelist_Free(struct trc_message_list *sentinel);

#endif /* __TRC_MESSAGE_H__ */
