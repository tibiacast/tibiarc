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

#include "container.h"

#include "utils.h"

void containerlist_CloseContainer(struct trc_container **containerList,
                                  uint32_t id) {
    struct trc_container *container;

    HASH_FIND_INT((*containerList), &id, container);

    if (container != NULL) {
        HASH_DEL((*containerList), container);

        checked_deallocate(container);
    }
}

void containerlist_OpenContainer(struct trc_container **containerList,
                                 uint32_t id,
                                 struct trc_container **result) {
    struct trc_container *container;

    HASH_FIND_INT((*containerList), &id, container);

    if (container == NULL) {
        container = (struct trc_container *)checked_allocate(
                1,
                sizeof(struct trc_container));

        container->Id = id;

        HASH_ADD_INT((*containerList), Id, container);
    }

    (*result) = container;
}

bool containerlist_GetContainer(struct trc_container **containerList,
                                uint32_t id,
                                struct trc_container **result) {
    struct trc_container *container;

    HASH_FIND_INT((*containerList), &id, container);
    (*result) = container;

    return container != NULL;
}

void containerlist_Free(struct trc_container **containerList) {
    struct trc_container *container, *tmp;

    HASH_ITER(hh, (*containerList), container, tmp) {
        HASH_DEL((*containerList), container);
        checked_deallocate(container);
    }
}
