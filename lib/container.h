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

#ifndef __TRC_CONTAINER_H__
#define __TRC_CONTAINER_H__

#include <stdbool.h>
#include <stdint.h>

#include "deps/uthash.h"
#include "object.h"

#include "characterset.h"

struct trc_container {
    UT_hash_handle hh;

    uint32_t Id;

    uint16_t NameLength;
    char Name[64];

    uint16_t ItemId;
    uint8_t Mark;
    uint8_t Animation;
    uint8_t SlotsPerPage;
    uint8_t HasParent;

    uint8_t DragAndDrop;
    uint8_t Pagination;

    uint16_t StartIndex;
    uint16_t TotalObjects;

    uint8_t ItemCount;

    struct trc_object Items[0x20];
};

/* FIXME: Sort the container list after each access, putting the most recently
 * used container up front so that it's easier to show relevant containers,
 * it's pretty horrible in pre-hotkey versions.
 *
 * Perhaps keep the first two containers stable to prevent bouncing? */
bool containerlist_GetContainer(struct trc_container **containerList,
                                uint32_t id,
                                struct trc_container **result);

void containerlist_OpenContainer(struct trc_container **containerList,
                                 uint32_t id,
                                 struct trc_container **result);
void containerlist_CloseContainer(struct trc_container **containerList,
                                  uint32_t id);

void containerlist_Free(struct trc_container **containerList);

#endif /* __TRC_CONTAINER_H__ */
