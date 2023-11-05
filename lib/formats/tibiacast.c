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

#include "recordings.h"
#include "versions.h"

#include "utils.h"

struct trc_recording_tibiacast {
    struct trc_recording Base;

    char VersionMajor;
    char VersionMinor;

    struct trc_data_reader RawReader;
    struct trc_data_reader DataReader;
    uint8_t DataBuffer[128 << 10];

    tinfl_decompressor Decompressor;
    size_t DecompressedWritePos;
    size_t DecompressedReadPos;
    uint8_t DecompressionBuffer[128 << 10];
};

enum {
    TIBIACAST_RECORDING_PACKETTYPE_STATECORRECTION = 6,
    TIBIACAST_RECORDING_PACKETTYPE_INITIALIZATION = 7,
    TIBIACAST_RECORDING_PACKETTYPE_TIBIADATA = 8,
    TIBIACAST_RECORDING_PACKETTYPE_OUTGOINGMESSAGE = 9
};

static bool tibiacast_Decompress(struct trc_recording_tibiacast *recording,
                                 size_t desiredCount,
                                 void *buffer) {
    while (recording->DecompressedWritePos <
                   sizeof(recording->DecompressionBuffer) &&
           datareader_Remaining(&recording->RawReader) > 0) {
        size_t in_bytes, out_bytes;
        tinfl_status status;

        out_bytes = sizeof(recording->DecompressionBuffer) -
                    recording->DecompressedWritePos;
        in_bytes = datareader_Remaining(&recording->RawReader);

        status = tinfl_decompress(
                &recording->Decompressor,
                (mz_uint8 *)datareader_RawData(&recording->RawReader),
                &in_bytes,
                (mz_uint8 *)&recording->DecompressionBuffer[0],
                (mz_uint8 *)&recording
                        ->DecompressionBuffer[recording->DecompressedWritePos],
                &out_bytes,
                TINFL_FLAG_HAS_MORE_INPUT);

        ABORT_UNLESS(datareader_Remaining(&recording->RawReader) >= in_bytes);
        datareader_Skip(&recording->RawReader, in_bytes);
        recording->DecompressedWritePos += out_bytes;

        if (status == TINFL_STATUS_DONE) {
            break;
        } else if (status != TINFL_STATUS_NEEDS_MORE_INPUT) {
            return trc_ReportError("Failed to decompress data");
        }
    }

    size_t availableBytes, copyBytes;

    availableBytes =
            recording->DecompressedWritePos - recording->DecompressedReadPos;
    copyBytes = MIN(desiredCount, availableBytes);

    memcpy(buffer,
           &recording->DecompressionBuffer[recording->DecompressedReadPos],
           copyBytes);
    recording->DecompressedReadPos += copyBytes;

    if (availableBytes < desiredCount) {
        if (datareader_Remaining(&recording->RawReader) == 0) {
            return trc_ReportError("The input buffer has run dry");
        } else {
            recording->DecompressedWritePos = 0;
            recording->DecompressedReadPos = 0;

            return tibiacast_Decompress(recording,
                                        desiredCount - availableBytes,
                                        &((char *)buffer)[copyBytes]);
        }
    }
    return true;
}

static bool tibiacast_HandleTibiaData(struct trc_recording_tibiacast *recording,
                                      struct trc_game_state *gamestate) {
    uint16_t subpacketCount;

    if (!datareader_ReadU16(&recording->DataReader, &subpacketCount)) {
        return trc_ReportError("Read failed on subpacketCount");
    }

    for (int subpacketIdx = 0; subpacketIdx < subpacketCount; subpacketIdx++) {
        uint16_t subpacketLength;

        if (!datareader_ReadU16(&recording->DataReader, &subpacketLength)) {
            return trc_ReportError("Read failed on subpacketLength");
        }

        if (subpacketLength > 0) {
            size_t startPosition = datareader_Tell(&recording->DataReader);

            if (!parser_ParsePacket(&recording->DataReader, gamestate)) {
                return trc_ReportError("Failed to parse Tibia data packet");
            }

            if (datareader_Tell(&recording->DataReader) !=
                (startPosition + subpacketLength)) {
                return trc_ReportError("Failed to meet parse target");
            }
        }
    }

    return true;
}

static bool tibiacast_HandleInitialization(
        struct trc_recording_tibiacast *recording,
        struct trc_game_state *gamestate) {
    const struct trc_version *version = gamestate->Version;
    uint16_t creatureCount;

    if (version->Protocol.PreviewByte) {
        if (!datareader_Skip(&recording->DataReader, 1)) {
            return trc_ReportError("Failed to skip isPreview");
        }
    }

    if (recording->VersionMajor >= 4) {
        /* HAZY: when was this widened to u16? */
        if (!datareader_ReadU16(&recording->DataReader, &creatureCount) ||
            creatureCount == 0) {
            return trc_ReportError("Read failed on creatureCount");
        }
    } else {
        if (!datareader_ReadU8(&recording->DataReader, &creatureCount) ||
            creatureCount == 0) {
            return trc_ReportError("Read failed on creatureCount");
        }
    }

    while (creatureCount--) {
        struct trc_creature *creature;
        uint32_t creatureId;

        if (!datareader_ReadU32(&recording->DataReader, &creatureId)) {
            return trc_ReportError("Read failed on creatureId");
        }

        creaturelist_ReplaceCreature(&gamestate->CreatureList,
                                     creatureId,
                                     0,
                                     &creature);

        if (version->Protocol.CreatureMarks) {
            uint8_t type;
            if (!datareader_ReadU8(&recording->DataReader, &type) ||
                !CHECK_RANGE(type, CREATURE_TYPE_FIRST, CREATURE_TYPE_LAST)) {
                return trc_ReportError("Read failed on Type");
            }
            type = (enum TrcCreatureType)creature->Type;
        }

        if (!datareader_ReadString(&recording->DataReader,
                                   sizeof(creature->Name),
                                   &creature->NameLength,
                                   creature->Name)) {
            return trc_ReportError("Read failed on Name");
        }

        if (!datareader_ReadU8(&recording->DataReader, &creature->Health) ||
            !CHECK_RANGE(creature->Health, 0, 100)) {
            return trc_ReportError("Read failed on Health");
        }

        uint8_t direction;
        if (!datareader_ReadU8(&recording->DataReader, &direction) ||
            !CHECK_RANGE(direction,
                         CREATURE_DIRECTION_FIRST,
                         CREATURE_DIRECTION_LAST)) {
            return trc_ReportError("Read failed on Direction");
        }
        creature->Direction = (enum TrcCreatureDirection)direction;

        if (!datareader_ReadU16(&recording->DataReader, &creature->Outfit.Id)) {
            return trc_ReportError("Read failed on Id");
        }

        if (creature->Outfit.Id == 0) {
            if (!datareader_ReadU16(&recording->DataReader,
                                    &creature->Outfit.Item.Id)) {
                return trc_ReportError("Read failed on ItemId");
            }
        } else {
            if (!datareader_ReadU8(&recording->DataReader,
                                   &creature->Outfit.HeadColor)) {
                return trc_ReportError("Read failed on HeadColor");
            }

            if (!datareader_ReadU8(&recording->DataReader,
                                   &creature->Outfit.PrimaryColor)) {
                return trc_ReportError("Read failed on PrimaryColor");
            }

            if (!datareader_ReadU8(&recording->DataReader,
                                   &creature->Outfit.SecondaryColor)) {
                return trc_ReportError("Read failed on SecondaryColor");
            }

            if (!datareader_ReadU8(&recording->DataReader,
                                   &creature->Outfit.DetailColor)) {
                return trc_ReportError("Read failed on DetailColor");
            }

            if (!datareader_ReadU8(&recording->DataReader,
                                   &creature->Outfit.Addons)) {
                return trc_ReportError("Read failed on Addons");
            }
        }

        if (version->Protocol.Mounts) {
            if (!datareader_ReadU16(&recording->DataReader,
                                    &creature->Outfit.MountId)) {
                return trc_ReportError("Read failed on MountId");
            }
        }

        if (!datareader_ReadU8(&recording->DataReader,
                               &creature->LightIntensity)) {
            return trc_ReportError("Read failed on LightIntensity");
        }

        if (!datareader_ReadU8(&recording->DataReader, &creature->LightColor)) {
            return trc_ReportError("Read failed on LightColor");
        }

        if (!datareader_ReadU16(&recording->DataReader, &creature->Speed)) {
            return trc_ReportError("Read failed on Speed");
        }

        uint8_t skull;
        if (!datareader_ReadU8(&recording->DataReader, &skull) ||
            !CHECK_RANGE(skull, CHARACTER_SKULL_FIRST, CHARACTER_SKULL_LAST)) {
            return trc_ReportError("Read failed on Skull");
        }
        creature->Skull = (enum TrcCharacterSkull)skull;

        uint8_t shield;
        if (!datareader_ReadU8(&recording->DataReader, &shield) ||
            !CHECK_RANGE(shield, PARTY_SHIELD_FIRST, PARTY_SHIELD_LAST)) {
            return trc_ReportError("Read failed on Shield");
        }
        creature->Shield = (enum TrcPartyShield)shield;

        if (version->Protocol.WarIcon) {
            uint8_t warIcon;
            if (!datareader_ReadU8(&recording->DataReader, &warIcon) ||
                !CHECK_RANGE(warIcon, WAR_ICON_FIRST, WAR_ICON_LAST)) {
                return trc_ReportError("Read failed on WarIcon");
            }
            creature->WarIcon = (enum TrcWarIcon)warIcon;
        }

        if (version->Protocol.CreatureMarks) {
            if ((gamestate->Version)->Protocol.NPCCategory) {
                uint8_t category;
                if (!datareader_ReadU8(&recording->DataReader, &category) ||
                    !CHECK_RANGE(category,
                                 NPC_CATEGORY_FIRST,
                                 NPC_CATEGORY_LAST)) {
                    return trc_ReportError("Read failed on NPCCategory");
                }
                creature->NPCCategory = (enum TrcNPCCategory)category;
            }

            if (!datareader_ReadU8(&recording->DataReader, &creature->Mark)) {
                return trc_ReportError("Read failed on Mark");
            }

            if (!datareader_ReadU8(&recording->DataReader,
                                   &creature->MarkIsPermanent)) {
                return trc_ReportError("Read failed on MarkIsPermanent");
            }

            if (!datareader_ReadU16(&recording->DataReader,
                                    &creature->GuildMembersOnline)) {
                return trc_ReportError("Read failed on GuildMembersOnline");
            }
        }

        if (version->Protocol.PassableCreatures) {
            if (!datareader_ReadU8(&recording->DataReader,
                                   &creature->Impassable)) {
                return trc_ReportError("Read failed on Impassable");
            }
        }
    }

    if (!tibiacast_HandleTibiaData(recording, gamestate)) {
        return trc_ReportError("Failed to initial tibia data");
    }

    return true;
}

static bool tibiacast_HandleOutgoingMessage(
        struct trc_recording_tibiacast *recording) {
    if (!datareader_SkipString(&recording->DataReader)) {
        return trc_ReportError("Could not skip sender name");
    }

    if (!datareader_SkipString(&recording->DataReader)) {
        return trc_ReportError("Could not skip message contents");
    }

    return true;
}

static bool tibiacast_HandleStateCorrection(
        struct trc_recording_tibiacast *recording) {
    uint8_t correctionType;

    if (!datareader_ReadU8(&recording->DataReader, &correctionType)) {
        return trc_ReportError("Read failed on correctionType");
    }

    switch (correctionType) {
    case 0: /* player trade closed */
        break;
    case 1: /* npc trade closed */
        break;
    default:
        return trc_ReportError("Unknown correction type");
    }

    return true;
}

static bool tibiacast_DecompressPacket(
        struct trc_recording_tibiacast *recording) {
    const size_t headerLength =
            (recording->VersionMajor >= 4 ? sizeof(uint32_t)
                                          : sizeof(uint16_t));
    uint32_t packetLength;

    if (!tibiacast_Decompress(recording, headerLength, recording->DataBuffer)) {
        return trc_ReportError("Could not decompress packet header");
    }

    recording->DataReader.Length = headerLength;
    recording->DataReader.Position = 0;

    if (recording->VersionMajor >= 4) {
        /* ABORT_UNLESS silences a clang analyzer warning */
        ABORT_UNLESS(datareader_ReadU32(&recording->DataReader, &packetLength));
    } else {
        ABORT_UNLESS(datareader_ReadU16(&recording->DataReader, &packetLength));
    }

    recording->DataReader.Length = packetLength;
    recording->DataReader.Position = 0;

    if (packetLength > 0) {
        uint32_t timestamp;
        uint8_t buffer[4];

        if (!tibiacast_Decompress(recording,
                                  packetLength,
                                  recording->DataBuffer)) {
            return trc_ReportError("Could not decompress packet data");
        }

        if (!tibiacast_Decompress(recording, sizeof(buffer), &buffer)) {
            return trc_ReportError("Could not decompress packet timestamp");
        }

        timestamp =
                ((uint32_t)buffer[0]) << 0x00 | ((uint32_t)buffer[1]) << 0x08 |
                ((uint32_t)buffer[2]) << 0x10 | ((uint32_t)buffer[3]) << 0x18;
        if (timestamp < recording->Base.NextPacketTimestamp) {
            return trc_ReportError("Invalid packet timestamp");
        }

        recording->Base.NextPacketTimestamp = timestamp;
    }

    recording->Base.HasReachedEnd = (packetLength == 0);
    recording->DataReader.Length = packetLength;
    recording->DataReader.Position = 0;

    return true;
}

static void tibiacast_Rewind(struct trc_recording_tibiacast *recording,
                             const struct trc_data_reader *reader) {
    recording->Base.NextPacketTimestamp = 0;
    recording->Base.HasReachedEnd = 0;

    tinfl_init(&recording->Decompressor);
    recording->DecompressedWritePos = 0;
    recording->DecompressedReadPos = 0;
    recording->RawReader = *reader;
}

static bool tibiacast_ProcessNextPacket(
        struct trc_recording_tibiacast *recording,
        struct trc_game_state *gamestate) {
    uint8_t packetType;

    if (!tibiacast_DecompressPacket(recording)) {
        return trc_ReportError("Could not decompress packet");
    }

    if (!recording->Base.HasReachedEnd) {
        if (!datareader_ReadU8(&recording->DataReader, &packetType)) {
            return trc_ReportError("Could not read packet type");
        }

        switch (packetType) {
        case TIBIACAST_RECORDING_PACKETTYPE_INITIALIZATION:
            return tibiacast_HandleInitialization(recording, gamestate);
        case TIBIACAST_RECORDING_PACKETTYPE_STATECORRECTION:
            return tibiacast_HandleStateCorrection(recording);
        case TIBIACAST_RECORDING_PACKETTYPE_TIBIADATA:
            return tibiacast_HandleTibiaData(recording, gamestate);
        case TIBIACAST_RECORDING_PACKETTYPE_OUTGOINGMESSAGE:
            return tibiacast_HandleOutgoingMessage(recording);
        }

        return trc_ReportError("Unhandled packet");
    }

    return true;
}

static bool tibiacast_QueryTibiaVersion(const struct trc_data_reader *file,
                                        int *major,
                                        int *minor,
                                        int *preview) {
    uint8_t containerMajor, containerMinor;
    struct trc_data_reader reader = *file;

    if (!(datareader_ReadU8(&reader, &containerMajor) &&
          datareader_ReadU8(&reader, &containerMinor))) {
        return trc_ReportError("Failed to read container version");
    }

    if (containerMajor == 3) {
        if (containerMinor < 5) {
            *major = 8;
            *minor = 55;
        } else if (containerMinor < 6) {
            *major = 8;
            *minor = 60;
        } else if (containerMinor < 8) {
            *major = 8;
            *minor = 61;
        } else if (containerMinor < 11) {
            *major = 8;
            *minor = 62;
        } else if (containerMinor < 15) {
            *major = 8;
            *minor = 71;
        } else if (containerMinor < 22) {
            *major = 9;
            *minor = 31;
        } else if (containerMinor < 26) {
            *major = 9;
            *minor = 40;
        } else if (containerMinor < 28) {
            *major = 9;
            *minor = 53;
        }
    } else if (containerMajor == 4) {
        if (containerMinor < 3) {
            *major = 9;
            *minor = 54;
        } else if (containerMinor < 5) {
            *major = 9;
            *minor = 61;
        } else if (containerMinor < 6) {
            *major = 9;
            *minor = 71;
        } else if (containerMinor < 9) {
            *major = 9;
            *minor = 80;
        } else if (containerMinor < 12) {
            /* TODO: When *minor is < 10, this is supposed to be "9.83 old,"
             * and we'll need a way to differentiate them. */
            *major = 9;
            *minor = 83;
        } else if (containerMinor < 13) {
            *major = 9;
            *minor = 86;
        } else if (containerMinor < 17) {
            *major = 10;
            *minor = 0;
        } else if (containerMinor < 20) {
            *major = 10;
            *minor = 34;
        } else if (containerMinor < 21) {
            *major = 10;
            *minor = 35;
        } else if (containerMinor < 22) {
            *major = 10;
            *minor = 37;
        } else if (containerMinor < 24) {
            *major = 10;
            *minor = 51;
        } else if (containerMinor < 25) {
            *major = 10;
            *minor = 52;
        } else if (containerMinor < 26) {
            *major = 10;
            *minor = 53;
        } else if (containerMinor < 27) {
            *major = 10;
            *minor = 54;
        } else if (containerMinor < 28) {
            *major = 10;
            *minor = 57;
        } else if (containerMinor < 29) {
            *major = 10;
            *minor = 58;
        } else if (containerMinor < 30) {
            *major = 10;
            *minor = 64;
        } else if (containerMinor < 31) {
            *major = 10;
            *minor = 94;
        }
    }

    if ((containerMajor > 4) || (containerMajor == 4 && containerMinor >= 5)) {
        if (!datareader_Skip(&reader, 4)) {
            return trc_ReportError("Failed to skip runtime");
        }
    }

    *preview = 0;
    if ((containerMajor > 4) || (containerMajor == 4 && containerMinor >= 6)) {
        if (!datareader_ReadU8(&reader, preview)) {
            return trc_ReportError("Failed to read preview flag");
        }
    }

    if (containerMajor == 4 && containerMinor < 10) {
        *preview = 0;
    }

    return true;
}

static bool tibiacast_Open(struct trc_recording_tibiacast *recording,
                           const struct trc_data_reader *file,
                           struct trc_version *version) {
    struct trc_data_reader reader = *file;

    if (!datareader_Skip(&reader, 2)) {
        return trc_ReportError("Failed to skip container version");
    }

    recording->Base.Runtime = 0;
    if ((recording->VersionMajor > 4) ||
        (recording->VersionMajor == 4 && recording->VersionMinor >= 5)) {
        if (!datareader_ReadU32(&reader, &recording->Base.Runtime)) {
            return trc_ReportError("Failed to read runtime");
        }
    }

    if ((recording->VersionMajor > 4) ||
        (recording->VersionMajor == 4 && recording->VersionMinor >= 6)) {
        if (!datareader_Skip(&reader, 1)) {
            return trc_ReportError("Failed to skip preview flag");
        }
    }

    if ((version->Major == 9 && version->Minor == 80)) {
        /* Tibiacast generated buggy intialization packets for recordings for a
         * short while. */
        version->Protocol.TibiacastBuggedInitialization = 1;
    }

    tibiacast_Rewind(recording, &reader);

    if (!tibiacast_Decompress(recording,
                              sizeof(uint32_t),
                              recording->DataBuffer)) {
        return trc_ReportError("Could not decompress initial packet timestamp");
    }

    recording->DataReader.Length = 4;
    recording->DataReader.Position = 0;

    uint32_t initial_timestamp;
    (void)datareader_ReadU32(&recording->DataReader, &initial_timestamp);

    if (recording->Base.Runtime == 0) {
        /* No explicit runtime, determine it manually instead. */
        do {
            if (!tibiacast_DecompressPacket(recording)) {
                return trc_ReportError("Failed to determine length of file");
            }

            recording->Base.Runtime = MAX(recording->Base.Runtime,
                                          recording->Base.NextPacketTimestamp);
        } while (recording->DataReader.Length > 0);

        tibiacast_Rewind(recording, &reader);

        if (!tibiacast_Decompress(recording,
                                  sizeof(uint32_t),
                                  recording->DataBuffer)) {
            return trc_ReportError("Could not decompress initial packet "
                                   "timestamp");
        }
    }

    recording->Base.NextPacketTimestamp = initial_timestamp;
    recording->Base.Version = version;

    return true;
}

static void tibiacast_Free(struct trc_recording_tibiacast *recording) {
    checked_deallocate(recording);
}

struct trc_recording *tibiacast_Create() {
    struct trc_recording_tibiacast *recording =
            (struct trc_recording_tibiacast *)
                    checked_allocate(1, sizeof(struct trc_recording_tibiacast));

    recording->Base.QueryTibiaVersion =
            (bool (*)(const struct trc_data_reader *, int *, int *, int *))
                    tibiacast_QueryTibiaVersion;
    recording->Base.Open = (bool (*)(struct trc_recording *,
                                     const struct trc_data_reader *,
                                     struct trc_version *))tibiacast_Open;
    recording->Base.ProcessNextPacket =
            (bool (*)(struct trc_recording *,
                      struct trc_game_state *))tibiacast_ProcessNextPacket;
    recording->Base.Free = (void (*)(struct trc_recording *))tibiacast_Free;

    recording->DataReader.Data = &recording->DataBuffer[0];

    return (struct trc_recording *)recording;
}
