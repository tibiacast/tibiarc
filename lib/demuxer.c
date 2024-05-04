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

#include "demuxer.h"

#include "utils.h"

bool demuxer_Submit(struct trc_demuxer *demuxer,
                    uint32_t timestamp,
                    struct trc_data_reader *reader) {
    while (datareader_Remaining(reader) > 0) {
        if (demuxer->Remaining == 0) {
            switch (demuxer->State) {
            case TRC_DEMUXER_STATE_PAYLOAD:
                struct trc_packet *packet = packet_Create(demuxer->Timestamp,
                                                          demuxer->Used,
                                                          demuxer->Buffer);

                *demuxer->NextP = packet;
                demuxer->NextP = &packet->Next;

                demuxer->State = TRC_DEMUXER_STATE_HEADER;
                demuxer->Remaining = demuxer->HeaderSize;
                demuxer->Used = 0;
                break;
            case TRC_DEMUXER_STATE_HEADER:
                const uint8_t *header;

                ABORT_UNLESS(demuxer->Used == demuxer->HeaderSize);
                header = &demuxer->Buffer[0];
                demuxer->Used = 0;

                demuxer->Remaining = ((uint16_t)header[0] << 0x00) |
                                     ((uint16_t)header[1] << 0x08);
                if (demuxer->HeaderSize == 4) {
                    demuxer->Remaining |= ((uint32_t)header[2] << 0x10) |
                                          ((uint32_t)header[3] << 0x18);
                }

                if (demuxer->Remaining > TRC_DEMUXER_BUFFER_SIZE) {
                    return trc_ReportError("Packet size too large");
                }

                demuxer->State = TRC_DEMUXER_STATE_PAYLOAD;
                demuxer->Timestamp = timestamp;
                break;
            }
        }

        uint32_t to_copy =
                MIN(datareader_Remaining(reader), demuxer->Remaining);

        ABORT_UNLESS(datareader_ReadRaw(reader,
                                        to_copy,
                                        &demuxer->Buffer[demuxer->Used]));

        demuxer->Remaining -= to_copy;
        demuxer->Used += to_copy;
    }

    return true;
}

bool demuxer_Finish(struct trc_demuxer *demuxer,
                    uint32_t *runtime,
                    struct trc_packet **packets) {
    if (demuxer->State == TRC_DEMUXER_STATE_PAYLOAD &&
        demuxer->Remaining == 0 && demuxer->First != NULL) {
        *runtime = demuxer->Timestamp;
        *packets = demuxer->First;
        demuxer->First = NULL;
        return true;
    }

    return false;
}

void demuxer_Initialize(struct trc_demuxer *demuxer, size_t headerSize) {
    ASSERT(headerSize == 2 || headerSize == 4);
    demuxer->HeaderSize = headerSize;
    demuxer->Remaining = headerSize;

    demuxer->NextP = &demuxer->First;
    demuxer->State = TRC_DEMUXER_STATE_HEADER;
}

void demuxer_Free(struct trc_demuxer *demuxer) {
    struct trc_packet *iterator = demuxer->First;

    while (iterator != NULL) {
        struct trc_packet *prev = iterator;
        iterator = iterator->Next;
        packet_Free(prev);
    }
}
