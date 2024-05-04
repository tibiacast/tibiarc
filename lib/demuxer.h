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

#ifndef __TRC_DEMUXER_H__
#define __TRC_DEMUXER_H__

#include "datareader.h"
#include "packet.h"

#include <stdint.h>
#include <stdlib.h>

#define TRC_DEMUXER_BUFFER_SIZE (128 << 10)

struct trc_demuxer {
    enum { TRC_DEMUXER_STATE_HEADER, TRC_DEMUXER_STATE_PAYLOAD } State;
    uint8_t HeaderSize;
    uint32_t Remaining;
    uint32_t Timestamp;
    uint32_t Used;

    struct trc_packet *First;
    struct trc_packet **NextP;

    uint8_t Buffer[TRC_DEMUXER_BUFFER_SIZE];
};

bool demuxer_Submit(struct trc_demuxer *demuxer,
                    uint32_t timestamp,
                    struct trc_data_reader *reader);

bool demuxer_Finish(struct trc_demuxer *demuxer,
                    uint32_t *runtime,
                    struct trc_packet **packets);

void demuxer_Initialize(struct trc_demuxer *demuxer, size_t headerSize);
void demuxer_Free(struct trc_demuxer *demuxer);

#endif
