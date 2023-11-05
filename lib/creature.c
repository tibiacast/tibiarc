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

#include "creature.h"

#include "utils.h"

void creaturelist_ReplaceCreature(struct trc_creature **creatureList,
                                  uint32_t newId,
                                  uint32_t oldId,
                                  struct trc_creature **result) {
    struct trc_creature *creature;

    HASH_FIND_INT((*creatureList), &oldId, creature);

    if (creature == NULL) {
        creature = (struct trc_creature *)checked_allocate(
                1,
                sizeof(struct trc_creature));
    } else if (newId != oldId) {
        HASH_DEL((*creatureList), creature);
    }

    creature->Id = newId;

    if (newId != oldId) {
        HASH_ADD_INT((*creatureList), Id, creature);
    }

    (*result) = creature;
}

bool creaturelist_GetCreature(struct trc_creature **creatureList,
                              uint32_t id,
                              struct trc_creature **result) {
    struct trc_creature *creature;

    HASH_FIND_INT((*creatureList), &id, creature);

    (*result) = creature;

    /* Referencing a non-existent creature is not a protocol violation outside
     * of movement and serialization. */
    return creature != NULL;
}

void creaturelist_Free(struct trc_creature **creatureList) {
    struct trc_creature *creature, *tmp;

    HASH_ITER(hh, (*creatureList), creature, tmp) {
        HASH_DEL((*creatureList), creature);
        checked_deallocate(creature);
    }
}
