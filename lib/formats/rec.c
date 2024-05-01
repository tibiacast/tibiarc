/*
 * Copyright 2011-2016 "Silver Squirrel Software Handelsbolag"
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

#include "recordings.h"
#include "versions.h"

#include "utils.h"

#ifndef DISABLE_OPENSSL_CRYPTO
#    include <openssl/evp.h>
#endif

/* Early .REC versions have 32-bit frame lengths, but I haven't observed any
 * recordings with an actual frame length larger than 64K. */
#define TRC_REC_MAX_FRAME_SIZE (64 << 10)

#define TRC_REC_OBFUSCATION_DIVISOR_MASK 0x0F
#define TRC_REC_OBFUSCATION_TWIRL(Divisor) (Divisor)
#define TRC_REC_OBFUSCATION_FRAME_COUNT_OFFSET (1 << 4)
#define TRC_REC_OBFUSCATION_U16_FRAME_LENGTHS (1 << 5)
#define TRC_REC_OBFUSCATION_CHECKSUM (1 << 6)
#define TRC_REC_OBFUSCATION_AES_DATA (1 << 7)

struct trc_recording_rec {
    struct trc_recording Base;

    uint32_t PacketNumber;
    uint32_t PacketCount;

    size_t Size;
    uint8_t *Data;

    struct trc_data_reader Reader;
};

static bool rec_ProcessNextPacket(struct trc_recording_rec *recording,
                                  struct trc_game_state *gamestate) {
    recording->PacketNumber++;
    recording->Base.HasReachedEnd =
            (recording->PacketNumber == recording->PacketCount);

    uint16_t packetLength;
    if (!datareader_ReadU16(&recording->Reader, &packetLength)) {
        return trc_ReportError("Could not read packet length");
    }

    struct trc_data_reader packetReader;
    if (!datareader_Slice(&recording->Reader, packetLength, &packetReader)) {
        return trc_ReportError("Bad packet length");
    }

    while (datareader_Remaining(&packetReader) > 0) {
        if (!parser_ParsePacket(&packetReader, gamestate)) {
            return trc_ReportError("Failed to parse Tibia data packet");
        }
    }

    if (!recording->Base.HasReachedEnd) {
        uint32_t timestamp;

        if (!datareader_ReadU32(&recording->Reader, &timestamp)) {
            return trc_ReportError("Could not read packet timestamp");
        }

        if (timestamp < recording->Base.NextPacketTimestamp) {
            return trc_ReportError("Invalid packet timestamp");
        }

        recording->Base.NextPacketTimestamp = timestamp;
        return true;
    }

    return datareader_Remaining(&recording->Reader) == 0;
}

static bool rec_QueryTibiaVersion(const struct trc_data_reader *file,
                                  int *major,
                                  int *minor,
                                  int *preview) {
    (void)file;
    (void)major;
    (void)minor;
    (void)preview;

    return trc_ReportError("TibiCAM .rec captures don't store Tibia version");
}

struct trc_rec_consolidate_state {
    uint32_t Obfuscation;

#ifndef DISABLE_OPENSSL_CRYPTO
    EVP_CIPHER_CTX *AES;
#endif

    struct {
        enum { TRC_UNPACK_STATE_HEADER, TRC_UNPACK_STATE_PAYLOAD } State;
        uint32_t Timestamp;

        uint32_t Emitted;
        uint32_t Remaining;
    } Packet;

    struct {
        uint32_t Timestamp;
        uint32_t Length;
        uint32_t Checksum;

        uint8_t *CipherData;
        uint8_t *PlainData;
    } Frame;
};

static void rec_ExpandBuffer(struct trc_recording_rec *recording,
                             struct trc_rec_consolidate_state *state,
                             size_t need) {
    ABORT_UNLESS(state->Packet.Emitted <= recording->Size);

    if ((recording->Size - state->Packet.Emitted) < need) {
        uint8_t *data = checked_allocate(2, recording->Size);

        memcpy(data, recording->Data, state->Packet.Emitted);
        checked_deallocate(recording->Data);

        recording->Data = data;
        recording->Size *= 2;
    }
}

static bool rec_DeobfuscateFrame(struct trc_recording_rec *recording,
                                 struct trc_rec_consolidate_state *state) {
    uint8_t divisor = (state->Obfuscation & TRC_REC_OBFUSCATION_DIVISOR_MASK);

    if (divisor) {
        uint32_t key = state->Frame.Length + state->Frame.Timestamp + 2;

        for (uint32_t i = 0; i < state->Frame.Length; i++) {
            uint32_t beta, alpha;

            alpha = (key + i * 33) & 0xFF;

            if ((divisor & (divisor - 1)) == 0) {
                /* Versions with power-of-2 divisors use unsigned remainder
                 * instead of signed remainder.
                 *
                 * This is most likely a side-effect of the original author
                 * doing all the transformations on signed chars, and their
                 * compiler deducing that they cannot be negative because of
                 * the previous operations, giving it free rein to optimize
                 * this into a bitwise-and. */
                beta = alpha & (divisor - 1);
            } else {
                beta = ((alpha - (alpha >> 7)) % divisor);
            }

            state->Frame.CipherData[i] -= alpha + (beta ? divisor - beta : 0);
        }
    }

    ASSERT(!!(state->Obfuscation & TRC_REC_OBFUSCATION_AES_DATA) ^
           state->Frame.CipherData == state->Frame.PlainData);

#ifndef DISABLE_OPENSSL_CRYPTO
    if (state->Obfuscation & TRC_REC_OBFUSCATION_AES_DATA) {
        int plainLength, outLength;

        if (state->Frame.Length % 16) {
            return trc_ReportError("Frame is not block-aligned");
        }

        ABORT_UNLESS(EVP_DecryptInit_ex(state->AES, NULL, NULL, NULL, NULL));

        plainLength = TRC_REC_MAX_FRAME_SIZE;
        if (!EVP_DecryptUpdate(state->AES,
                               state->Frame.PlainData,
                               &plainLength,
                               state->Frame.CipherData,
                               state->Frame.Length)) {
            return trc_ReportError("Failed to decrypt frame data (update)");
        }

        ASSERT(plainLength < TRC_REC_MAX_FRAME_SIZE);
        outLength = TRC_REC_MAX_FRAME_SIZE - plainLength;

        if (!EVP_DecryptFinal_ex(state->AES,
                                 &state->Frame.PlainData[plainLength],
                                 &outLength)) {
            return trc_ReportError("Failed to decrypt frame data (final)");
        }

        ASSERT(outLength < 16);

        state->Frame.Length = plainLength + outLength;
    }
#endif

    return true;
}

static bool rec_UnpackFrame(struct trc_recording_rec *recording,
                            struct trc_rec_consolidate_state *state) {
    struct trc_data_reader reader = {.Length = state->Frame.Length,
                                     .Position = 0,
                                     .Data = state->Frame.PlainData};

    while (datareader_Remaining(&reader) > 0) {
        if (state->Packet.Remaining == 0) {
            switch (state->Packet.State) {
            case TRC_UNPACK_STATE_PAYLOAD:
                /* We've got a full packet (or just started), emit its
                 * timestamp. */
                uint8_t *writer;

                rec_ExpandBuffer(recording, state, 4);

                writer = &recording->Data[state->Packet.Emitted];
                writer[0] = (state->Packet.Timestamp >> 0x00);
                writer[1] = (state->Packet.Timestamp >> 0x08);
                writer[2] = (state->Packet.Timestamp >> 0x10);
                writer[3] = (state->Packet.Timestamp >> 0x18);
                state->Packet.Emitted += 4;

                /* Read the length of the next packet into the datastream to
                 * simplify the annoyingly common case of packet lengths being
                 * split over two frames.
                 *
                 * The header logic will need to read the _produced_ data
                 * determine the length of the next packet. */
                state->Packet.State = TRC_UNPACK_STATE_HEADER;
                state->Packet.Remaining = 2;

                recording->PacketCount++;
                break;
            case TRC_UNPACK_STATE_HEADER:
                uint8_t *header;

                ABORT_UNLESS(state->Packet.Emitted >= 6);
                header = &recording->Data[state->Packet.Emitted - 2];

                state->Packet.State = TRC_UNPACK_STATE_PAYLOAD;
                state->Packet.Timestamp = state->Frame.Timestamp;
                state->Packet.Remaining = ((uint16_t)header[0] << 0x00) |
                                          ((uint16_t)header[1] << 0x08);
                break;
            }
        }

        uint32_t to_copy =
                MIN(datareader_Remaining(&reader), state->Packet.Remaining);

        rec_ExpandBuffer(recording, state, to_copy);

        if (!datareader_ReadRaw(&reader,
                                to_copy,
                                &recording->Data[state->Packet.Emitted])) {
            return trc_ReportError("Failed to read frame data");
        }

        state->Packet.Remaining -= to_copy;
        state->Packet.Emitted += to_copy;
    }

    return true;
}

static bool rec_Consolidate(struct trc_recording_rec *recording,
                            struct trc_data_reader *reader,
                            struct trc_rec_consolidate_state *state,
                            uint32_t frameCount) {

    for (int32_t i = 0; i < frameCount; i++) {
        if (!((state->Obfuscation & TRC_REC_OBFUSCATION_U16_FRAME_LENGTHS)
                      ? datareader_ReadU16(reader, &state->Frame.Length)
                      : datareader_ReadU32(reader, &state->Frame.Length))) {
            return trc_ReportError("Failed to read frame length");
        }

        if (state->Frame.Length > TRC_REC_MAX_FRAME_SIZE) {
            return trc_ReportError("Frame length out of bounds");
        }

        if (!datareader_ReadU32(reader, &state->Frame.Timestamp)) {
            return trc_ReportError("Failed to read frame time");
        }

        if (!datareader_ReadRaw(reader,
                                state->Frame.Length,
                                state->Frame.CipherData)) {
            return trc_ReportError("Failed to read frame data");
        }

        if (!rec_DeobfuscateFrame(recording, state)) {
            return trc_ReportError("Failed to deobfuscate frame");
        }

        if (!rec_UnpackFrame(recording, state)) {
            return trc_ReportError("Failed to unpack frame");
        }

        if (state->Obfuscation & TRC_REC_OBFUSCATION_CHECKSUM) {
            if (!datareader_Skip(reader, 4)) {
                return trc_ReportError("Failed to skip frame checksum");
            }
        }
    }

    recording->Base.Runtime = state->Frame.Timestamp;

    recording->Reader.Position = 0;
    recording->Reader.Length = state->Packet.Emitted;
    recording->Reader.Data = recording->Data;

    return true;
}

static bool rec_Open(struct trc_recording_rec *recording,
                     const struct trc_data_reader *file,
                     struct trc_version *version) {
    struct trc_data_reader reader = *file;

    uint16_t containerVersion;
    uint32_t obfuscation;
    int32_t frameCount;

    if (!datareader_ReadU16(&reader, &containerVersion)) {
        return trc_ReportError("Failed to read container version");
    }

    if (!datareader_ReadI32(&reader, &frameCount)) {
        return trc_ReportError("Failed to read frame count");
    }

    obfuscation = 0;

    switch (containerVersion) {
    case 259:
        break;
    case 515:
        obfuscation |= TRC_REC_OBFUSCATION_TWIRL(5);
        break;
    case 516:
    case 517:
        obfuscation |= TRC_REC_OBFUSCATION_TWIRL(8);
        break;
    case 518:
        obfuscation |= TRC_REC_OBFUSCATION_TWIRL(6);
        break;
    default:
        return trc_ReportError("Unsupported container version");
    }

    if (containerVersion > 259) {
        obfuscation |= TRC_REC_OBFUSCATION_FRAME_COUNT_OFFSET;
        obfuscation |= TRC_REC_OBFUSCATION_U16_FRAME_LENGTHS;

        frameCount -= 57;
    }

    if (frameCount <= 0) {
        return trc_ReportError("Invalid frame count");
    }

    if (containerVersion >= 515) {
        obfuscation |= TRC_REC_OBFUSCATION_CHECKSUM;
    }

    if (containerVersion >= 517) {
        obfuscation |= TRC_REC_OBFUSCATION_AES_DATA;
    }

    struct trc_rec_consolidate_state state = {
            .Obfuscation = obfuscation,
            .Packet = {.State = TRC_UNPACK_STATE_PAYLOAD,
                       .Emitted = 0,
                       .Remaining = 0,
                       .Timestamp = 0}};
    bool success;

    if (obfuscation & TRC_REC_OBFUSCATION_AES_DATA) {
#ifdef DISABLE_OPENSSL_CRYPTO
        return trc_ReportError("Cannot load recording, crypto library "
                               "missing");
#else
        uint8_t key[32] = {0x54, 0x68, 0x79, 0x20, 0x6B, 0x65, 0x79, 0x20,
                           0x69, 0x73, 0x20, 0x6D, 0x69, 0x6E, 0x65, 0x20,
                           0xA9, 0x20, 0x32, 0x30, 0x30, 0x36, 0x20, 0x47,
                           0x42, 0x20, 0x4D, 0x6F, 0x6E, 0x61, 0x63, 0x6F};

        state.AES = EVP_CIPHER_CTX_new();
        EVP_CIPHER_CTX_init(state.AES);

        if (!EVP_DecryptInit(state.AES, EVP_aes_256_ecb(), key, NULL)) {
            EVP_CIPHER_CTX_free(state.AES);
            return trc_ReportError("Failed to set up cipher context");
        }

        state.Frame.PlainData = checked_allocate(2, TRC_REC_MAX_FRAME_SIZE);
        state.Frame.CipherData = &state.Frame.PlainData[TRC_REC_MAX_FRAME_SIZE];
#endif
    } else {
        state.Frame.PlainData = checked_allocate(1, TRC_REC_MAX_FRAME_SIZE);
        state.Frame.CipherData = state.Frame.PlainData;
    }

    /* Conservatively allocate a buffer twice the size of the incoming
     * recording.
     *
     * We may still need to grow it later as we prefix every Tibia
     * packet with a 4-byte timestamp. */
    recording->Size = datareader_Remaining(&reader);
    recording->Data = checked_allocate(2, recording->Size);

    success = rec_Consolidate(recording, &reader, &state, frameCount);
    checked_deallocate(state.Frame.PlainData);

#ifdef DISABLE_OPENSSL_CRYPTO
    if (obfuscation & TRC_REC_OBFUSCATION_AES_DATA) {
        EVP_CIPHER_CTX_free(state.AES);
    }
#endif

    if (!success) {
        return trc_ReportError("Failed to consolidate recording");
    }

    ABORT_UNLESS(datareader_ReadU32(&recording->Reader,
                                    &recording->Base.NextPacketTimestamp));

    recording->Base.Version = version;
    recording->Base.NextPacketTimestamp = 0;
    recording->Base.HasReachedEnd = 0;

    return true;
}

static void rec_Rewind(struct trc_recording_rec *recording) {
    recording->Reader.Position = 0;
    recording->PacketNumber = 0;

    ABORT_UNLESS(datareader_ReadU32(&recording->Reader,
                                    &recording->Base.NextPacketTimestamp));
}

static void rec_Free(struct trc_recording_rec *recording) {
    checked_deallocate(recording->Data);
    checked_deallocate(recording);
}

struct trc_recording *rec_Create() {
    struct trc_recording_rec *recording = (struct trc_recording_rec *)
            checked_allocate(1, sizeof(struct trc_recording_rec));

    recording->Base.QueryTibiaVersion =
            (bool (*)(const struct trc_data_reader *, int *, int *, int *))
                    rec_QueryTibiaVersion;
    recording->Base.Open = (bool (*)(struct trc_recording *,
                                     const struct trc_data_reader *,
                                     struct trc_version *))rec_Open;
    recording->Base.Rewind = (void (*)(struct trc_recording *))rec_Rewind;
    recording->Base.ProcessNextPacket =
            (bool (*)(struct trc_recording *,
                      struct trc_game_state *))rec_ProcessNextPacket;
    recording->Base.Free = (void (*)(struct trc_recording *))rec_Free;

    return (struct trc_recording *)recording;
}
