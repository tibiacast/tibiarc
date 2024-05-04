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

#include "packet.h"
#include "utils.h"

struct trc_packet *packet_Create(uint32_t timestamp,
                                 uint32_t length,
                                 uint8_t *data) {
    struct trc_packet *packet = (struct trc_packet *)checked_allocate(
            1,
            sizeof(struct trc_packet) + length);
    packet->Timestamp = timestamp;
    packet->Length = length;
    packet->Data = (uint8_t *)&packet[1];
    memcpy(packet->Data, data, length);

    return packet;
}

void packet_Free(struct trc_packet *packet) {
    checked_deallocate(packet);
}
