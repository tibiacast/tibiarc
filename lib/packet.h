/*
 * Copyright 2024 "John Högberg"
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

#ifndef __TRC_PACKET_H__
#define __TRC_PACKET_H__

#include <stdint.h>

struct trc_packet {
    struct trc_packet *Next;

    uint32_t Timestamp;
    uint32_t Length;
    uint8_t *Data;
};

struct trc_packet *packet_Create(uint32_t timestamp,
                                 uint32_t length,
                                 uint8_t *data);
void packet_Free(struct trc_packet *packet);

#endif
