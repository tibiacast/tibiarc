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

#include "message.h"

#include <string.h>

#include "utils.h"

static int messagelist_CompareTypes(uint8_t messageType, uint8_t compareType) {
    switch (messageType) {
    case MESSAGEMODE_SAY:
    case MESSAGEMODE_WHISPER:
    case MESSAGEMODE_YELL:
    case MESSAGEMODE_SPELL:
        messageType = MESSAGEMODE_SAY;
    }

    switch (compareType) {
    case MESSAGEMODE_SAY:
    case MESSAGEMODE_WHISPER:
    case MESSAGEMODE_YELL:
    case MESSAGEMODE_SPELL:
        compareType = MESSAGEMODE_SAY;
    }

    if (messageType < compareType) {
        return 1;
    } else if (messageType > compareType) {
        return -1;
    }

    return 0;
}

static int messagelist_SortFunction(const struct trc_message *message,
                                    const struct trc_message *compareTo) {
    int authorCompare, typeCompare;

    typeCompare = messagelist_CompareTypes(message->Type, compareTo->Type);
    if (typeCompare != 0) {
        return typeCompare;
    }

    if (message->Position.X < compareTo->Position.X) {
        return -1;
    } else if (message->Position.X > compareTo->Position.X) {
        return 1;
    }

    if (message->Position.Y < compareTo->Position.Y) {
        return -1;
    } else if (message->Position.Y > compareTo->Position.Y) {
        return 1;
    }

    if (message->Position.Z < compareTo->Position.Z) {
        return -1;
    } else if (message->Position.Z > compareTo->Position.Z) {
        return 1;
    }

    authorCompare = memcmp(message->Author,
                           compareTo->Author,
                           MIN(message->AuthorLength, compareTo->AuthorLength));
    if (authorCompare != 0) {
        return authorCompare;
    }

    return -0;
}

void messagelist_QueryNext(struct trc_message_list *sentinel,
                           struct trc_message *message,
                           uint32_t tick,
                           int *preserveCoordinates,
                           int *canMerge) {
    const struct trc_message *next = (struct trc_message *)message->Chain.Next;

    (*preserveCoordinates) = 0;
    (*canMerge) = 0;

    if (&next->Chain != sentinel && next->EndTick >= tick) {
        if (message->Position.X == next->Position.X &&
            message->Position.Y == next->Position.Y &&
            message->Position.Z == next->Position.Z) {
            (*preserveCoordinates) =
                    !messagelist_CompareTypes(message->Type, next->Type);

            (*canMerge) =
                    (*preserveCoordinates) &&
                    (message->Type != MESSAGEMODE_PRIVATE_IN) &&
                    (message->AuthorLength == next->AuthorLength) &&
                    (!memcmp(message->Author,
                             next->Author,
                             MIN(message->AuthorLength, next->AuthorLength)));
        }
    }
}

bool messagelist_Sweep(struct trc_message_list *sentinel,
                       uint32_t tick,
                       struct trc_message **iterator) {
    struct trc_message_list **next_p, *prev;

    next_p = *iterator ? &(*iterator)->Chain.Next : &sentinel->Next;
    prev = *iterator ? (*iterator)->Chain.Previous : sentinel;

    while (*next_p != sentinel &&
           tick > ((struct trc_message *)*next_p)->EndTick) {
        struct trc_message_list *pruned = *next_p;

        ASSERT(prev != pruned);
        *next_p = pruned->Next;
        prev->Next = *next_p;
        (*next_p)->Previous = prev;

        checked_deallocate(pruned);
    }

    *iterator = (struct trc_message *)*next_p;
    return *next_p != sentinel;
}

void messagelist_AddMessage(struct trc_message_list *sentinel,
                            struct trc_position *position,
                            uint32_t tick,
                            enum TrcMessageMode type,
                            uint16_t nameLength,
                            const char *name,
                            uint16_t textLength,
                            const char *text) {
    struct trc_message_list **next_p = &sentinel->Next;
    struct trc_message_list **prev_p = &sentinel->Previous;
    struct trc_message *message;

    message =
            (struct trc_message *)checked_allocate(1,
                                                   sizeof(struct trc_message));

    /* Make sure everything is C-string compatible.
     *
     * I love it how C string formatting doesn't support variable width. */
    message->AuthorLength = MIN(nameLength, MESSAGE_MAX_AUTHOR_LENGTH - 1);
    message->TextLength = MIN(textLength, MESSAGE_MAX_TEXT_LENGTH - 1);

    if (nameLength > 0 && name != NULL) {
        memcpy(&message->Author, name, message->AuthorLength);
        message->Author[message->AuthorLength] = '\0';
    }

    memcpy(&message->Text, text, message->TextLength);
    message->Text[message->TextLength] = '\0';

    message->Position.X = position ? position->X : 0;
    message->Position.Y = position ? position->Y : 0;
    message->Position.Z = position ? position->Z : 0;

    message->EndTick = tick + MESSAGE_DISPLAY_TIME;
    message->StartTick = tick;
    message->Type = type;

    if (message->Type != MESSAGEMODE_PRIVATE_IN) {
        /* Ordinary messages are added front to back, simplifying the
         * renderer's message merge logic. */
        while (*next_p != sentinel) {
            if (messagelist_SortFunction(message,
                                         (struct trc_message *)*next_p) < 0) {
                prev_p = &(*next_p)->Previous;
                break;
            }

            ASSERT((*next_p)->Previous != (*next_p));
            next_p = &(*next_p)->Next;
        }
    } else {
        /* Private messages are added back to front, with their display time
         * bumped longer than the ordinary client so that messages sent in
         * quick succession can be seen in full. */
        while (*prev_p != sentinel) {
            if (messagelist_SortFunction(message,
                                         (struct trc_message *)*prev_p) < 0) {
                next_p = &(*prev_p)->Next;
                break;
            }

            ASSERT((*prev_p)->Previous != (*prev_p));
            prev_p = &(*prev_p)->Previous;
        }

        if (*prev_p != sentinel) {
            message->StartTick =
                    MAX(tick, ((struct trc_message *)*prev_p)->EndTick);
            message->EndTick = message->StartTick + MESSAGE_DISPLAY_TIME;
        }
    }

    ASSERT((*prev_p == sentinel && *next_p == sentinel) ^ (*prev_p != *next_p));
    ASSERT(&message->Chain != (*prev_p) && &message->Chain != (*next_p));
    message->Chain.Previous = *prev_p;
    message->Chain.Next = *next_p;
    *next_p = &message->Chain;
    *prev_p = &message->Chain;
}

void messagelist_Initialize(struct trc_message_list *sentinel) {
    sentinel->Next = sentinel;
    sentinel->Previous = sentinel;
}

void messagelist_Free(struct trc_message_list *sentinel) {
    struct trc_message_list *iterator = sentinel->Next;

    while (iterator != sentinel) {
        struct trc_message_list *message = iterator;
        iterator = iterator->Next;

        checked_deallocate(message);
    }
}
