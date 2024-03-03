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

#ifndef DISABLE_ZLIB
#    include <zlib.h>
#endif

#include "utils.h"

struct trc_recording_tibiacast {
    struct trc_recording Base;

    char VersionMajor;
    char VersionMinor;

    struct trc_data_reader Reader;
    uint8_t *Uncompressed;
};

enum {
    TIBIACAST_RECORDING_PACKETTYPE_STATECORRECTION = 6,
    TIBIACAST_RECORDING_PACKETTYPE_INITIALIZATION = 7,
    TIBIACAST_RECORDING_PACKETTYPE_TIBIADATA = 8,
    TIBIACAST_RECORDING_PACKETTYPE_OUTGOINGMESSAGE = 9
};

static bool tibiacast_HandleTibiaData(struct trc_recording_tibiacast *recording,
                                      struct trc_game_state *gamestate) {
    uint16_t subpacketCount;

    if (!datareader_ReadU16(&recording->Reader, &subpacketCount)) {
        return trc_ReportError("Read failed on subpacketCount");
    }

    for (int subpacketIdx = 0; subpacketIdx < subpacketCount; subpacketIdx++) {
        uint16_t subpacketLength;

        if (!datareader_ReadU16(&recording->Reader, &subpacketLength)) {
            return trc_ReportError("Read failed on subpacketLength");
        }

        if (subpacketLength > 0) {
            struct trc_data_reader packetReader;

            if (!datareader_Slice(&recording->Reader,
                                  subpacketLength,
                                  &packetReader)) {
                return trc_ReportError("Failed to read subpacket data");
            }

            if (!parser_ParsePacket(&packetReader, gamestate)) {
                return trc_ReportError("Failed to parse Tibia data");
            }

            if (datareader_Remaining(&packetReader) > 0) {
                return trc_ReportError("Failed to meet parse target");
            }
        }
    }

    return true;
}

static bool tibiacast_ParseCreatureList(
        struct trc_recording_tibiacast *recording,
        const struct trc_version *version,
        struct trc_creature **creatureList) {
    uint16_t creatureCount;

    if (recording->VersionMajor >= 4) {
        /* HAZY: when was this widened to u16? */
        if (!datareader_ReadU16(&recording->Reader, &creatureCount) ||
            creatureCount == 0) {
            return trc_ReportError("Read failed on creatureCount");
        }
    } else {
        if (!datareader_ReadU8(&recording->Reader, &creatureCount) ||
            creatureCount == 0) {
            return trc_ReportError("Read failed on creatureCount");
        }
    }

    while (creatureCount--) {
        struct trc_creature *creature;
        uint32_t creatureId;

        if (!datareader_ReadU32(&recording->Reader, &creatureId)) {
            return trc_ReportError("Read failed on creatureId");
        }

        creaturelist_ReplaceCreature(creatureList, creatureId, 0, &creature);

        if (version->Protocol.CreatureMarks) {
            uint8_t type;
            if (!datareader_ReadU8(&recording->Reader, &type) ||
                !CHECK_RANGE(type, CREATURE_TYPE_FIRST, CREATURE_TYPE_LAST)) {
                return trc_ReportError("Read failed on Type");
            }
            type = (enum TrcCreatureType)creature->Type;
        }

        if (!datareader_ReadString(&recording->Reader,
                                   sizeof(creature->Name),
                                   &creature->NameLength,
                                   creature->Name)) {
            return trc_ReportError("Read failed on Name");
        }

        if (!datareader_ReadU8(&recording->Reader, &creature->Health) ||
            !CHECK_RANGE(creature->Health, 0, 100)) {
            return trc_ReportError("Read failed on Health");
        }

        uint8_t direction;
        if (!datareader_ReadU8(&recording->Reader, &direction) ||
            !CHECK_RANGE(direction,
                         CREATURE_DIRECTION_FIRST,
                         CREATURE_DIRECTION_LAST)) {
            return trc_ReportError("Read failed on Direction");
        }
        creature->Direction = (enum TrcCreatureDirection)direction;

        if (!datareader_ReadU16(&recording->Reader, &creature->Outfit.Id)) {
            return trc_ReportError("Read failed on Id");
        }

        if (creature->Outfit.Id == 0) {
            if (!datareader_ReadU16(&recording->Reader,
                                    &creature->Outfit.Item.Id)) {
                return trc_ReportError("Read failed on ItemId");
            }
        } else {
            if (!datareader_ReadU8(&recording->Reader,
                                   &creature->Outfit.HeadColor)) {
                return trc_ReportError("Read failed on HeadColor");
            }

            if (!datareader_ReadU8(&recording->Reader,
                                   &creature->Outfit.PrimaryColor)) {
                return trc_ReportError("Read failed on PrimaryColor");
            }

            if (!datareader_ReadU8(&recording->Reader,
                                   &creature->Outfit.SecondaryColor)) {
                return trc_ReportError("Read failed on SecondaryColor");
            }

            if (!datareader_ReadU8(&recording->Reader,
                                   &creature->Outfit.DetailColor)) {
                return trc_ReportError("Read failed on DetailColor");
            }

            if (!datareader_ReadU8(&recording->Reader,
                                   &creature->Outfit.Addons)) {
                return trc_ReportError("Read failed on Addons");
            }
        }

        if (version->Protocol.Mounts) {
            if (!datareader_ReadU16(&recording->Reader,
                                    &creature->Outfit.MountId)) {
                return trc_ReportError("Read failed on MountId");
            }
        }

        if (!datareader_ReadU8(&recording->Reader, &creature->LightIntensity)) {
            return trc_ReportError("Read failed on LightIntensity");
        }

        if (!datareader_ReadU8(&recording->Reader, &creature->LightColor)) {
            return trc_ReportError("Read failed on LightColor");
        }

        if (!datareader_ReadU16(&recording->Reader, &creature->Speed)) {
            return trc_ReportError("Read failed on Speed");
        }

        uint8_t skull;
        if (!datareader_ReadU8(&recording->Reader, &skull) ||
            !CHECK_RANGE(skull, CHARACTER_SKULL_FIRST, CHARACTER_SKULL_LAST)) {
            return trc_ReportError("Read failed on Skull");
        }
        creature->Skull = (enum TrcCharacterSkull)skull;

        uint8_t shield;
        if (!datareader_ReadU8(&recording->Reader, &shield) ||
            !CHECK_RANGE(shield, PARTY_SHIELD_FIRST, PARTY_SHIELD_LAST)) {
            return trc_ReportError("Read failed on Shield");
        }
        creature->Shield = (enum TrcPartyShield)shield;

        if (version->Protocol.WarIcon) {
            uint8_t warIcon;
            if (!datareader_ReadU8(&recording->Reader, &warIcon) ||
                !CHECK_RANGE(warIcon, WAR_ICON_FIRST, WAR_ICON_LAST)) {
                return trc_ReportError("Read failed on WarIcon");
            }
            creature->WarIcon = (enum TrcWarIcon)warIcon;
        }

        if (version->Protocol.CreatureMarks) {
            if (version->Protocol.NPCCategory) {
                uint8_t category;
                if (!datareader_ReadU8(&recording->Reader, &category) ||
                    !CHECK_RANGE(category,
                                 NPC_CATEGORY_FIRST,
                                 NPC_CATEGORY_LAST)) {
                    return trc_ReportError("Read failed on NPCCategory");
                }
                creature->NPCCategory = (enum TrcNPCCategory)category;
            }

            if (!datareader_ReadU8(&recording->Reader, &creature->Mark)) {
                return trc_ReportError("Read failed on Mark");
            }

            if (!datareader_ReadU8(&recording->Reader,
                                   &creature->MarkIsPermanent)) {
                return trc_ReportError("Read failed on MarkIsPermanent");
            }

            if (!datareader_ReadU16(&recording->Reader,
                                    &creature->GuildMembersOnline)) {
                return trc_ReportError("Read failed on GuildMembersOnline");
            }
        }

        if (version->Protocol.PassableCreatures) {
            if (!datareader_ReadU8(&recording->Reader, &creature->Impassable)) {
                return trc_ReportError("Read failed on Impassable");
            }
        }
    }

    return true;
}

/* This is essentially equal to `tibiacast_HandleTibiaData`, but since we're
 * clearing the creature list in `parser_ParseInitialization` packets as they
 * can appear several times in non-Tibiacast recordings, we have to set the
 * stored creature list after the initialization packet. */
static bool tibiacast_ParseInitialPackets(
        struct trc_recording_tibiacast *recording,
        struct trc_game_state *gamestate,
        struct trc_creature *creatureList) {
    uint16_t subpacketCount, subpacketLength;
    uint8_t packetType;

    if (!datareader_ReadU16(&recording->Reader, &subpacketCount)) {
        return trc_ReportError("Read failed on subpacketCount");
    }

    if (subpacketCount < 1) {
        return trc_ReportError("Initialization packet must contain at "
                               "least one subpacket");
    }

    if (!datareader_ReadU16(&recording->Reader, &subpacketLength)) {
        return trc_ReportError("Read failed on subpacketLength");
    }

    struct trc_data_reader packetReader;
    if (!datareader_Slice(&recording->Reader, subpacketLength, &packetReader)) {
        return trc_ReportError("Failed to read subpacket data");
    }

    if (!parser_ParsePacket(&packetReader, gamestate)) {
        return trc_ReportError("Failed to parse initialization packet");
    }

    gamestate->CreatureList = creatureList;

    for (int subpacketIdx = 1; subpacketIdx < subpacketCount; subpacketIdx++) {
        if (!datareader_ReadU16(&recording->Reader, &subpacketLength)) {
            return trc_ReportError("Read failed on subpacketLength");
        }

        if (!datareader_Slice(&recording->Reader,
                              subpacketLength,
                              &packetReader)) {
            return trc_ReportError("Failed to read subpacket data");
        }

        if (!parser_ParsePacket(&packetReader, gamestate)) {
            return trc_ReportError("Failed to parse Tibia data");
        }

        if (datareader_Remaining(&packetReader) > 0) {
            return trc_ReportError("Failed to meet parse target");
        }
    }

    return true;
}

static bool tibiacast_HandleInitialization(
        struct trc_recording_tibiacast *recording,
        struct trc_game_state *gamestate) {
    const struct trc_version *version = gamestate->Version;
    struct trc_creature *initialCreatureList = NULL;

    if (version->Protocol.PreviewByte) {
        if (!datareader_Skip(&recording->Reader, 1)) {
            return trc_ReportError("Failed to skip isPreview");
        }
    }

    if (!tibiacast_ParseCreatureList(recording,
                                     gamestate->Version,
                                     &initialCreatureList)) {
        return trc_ReportError("Failed to parse initial creature list");
    }

    if (!tibiacast_ParseInitialPackets(recording,
                                       gamestate,
                                       initialCreatureList)) {
        return trc_ReportError("Failed to parse initial packets");
    }

    return true;
}

static bool tibiacast_HandleOutgoingMessage(
        struct trc_recording_tibiacast *recording) {
    if (!datareader_SkipString(&recording->Reader)) {
        return trc_ReportError("Could not skip sender name");
    }

    if (!datareader_SkipString(&recording->Reader)) {
        return trc_ReportError("Could not skip message contents");
    }

    return true;
}

static bool tibiacast_HandleStateCorrection(
        struct trc_recording_tibiacast *recording) {
    uint8_t correctionType;

    if (!datareader_ReadU8(&recording->Reader, &correctionType)) {
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

static bool tibiacast_ProcessNextPacket(
        struct trc_recording_tibiacast *recording,
        struct trc_game_state *gamestate) {
    uint32_t packetLength;
    uint32_t timestamp;

    if (recording->VersionMajor >= 4) {
        if (!datareader_ReadU32(&recording->Reader, &packetLength)) {
            return trc_ReportError("Could not read packet length");
        }
    } else {
        if (!datareader_ReadU16(&recording->Reader, &packetLength)) {
            return trc_ReportError("Could not read packet length");
        }
    }

    recording->Base.HasReachedEnd = (packetLength == 0);

    if (!recording->Base.HasReachedEnd) {
        uint8_t packetType;
        bool success;

        if (!datareader_ReadU8(&recording->Reader, &packetType)) {
            return trc_ReportError("Could not read packet type");
        }

        switch (packetType) {
        case TIBIACAST_RECORDING_PACKETTYPE_INITIALIZATION:
            success = tibiacast_HandleInitialization(recording, gamestate);
            break;
        case TIBIACAST_RECORDING_PACKETTYPE_STATECORRECTION:
            success = tibiacast_HandleStateCorrection(recording);
            break;
        case TIBIACAST_RECORDING_PACKETTYPE_TIBIADATA:
            success = tibiacast_HandleTibiaData(recording, gamestate);
            break;
        case TIBIACAST_RECORDING_PACKETTYPE_OUTGOINGMESSAGE:
            success = tibiacast_HandleOutgoingMessage(recording);
            break;
        default:
            return trc_ReportError("Unhandled packet");
        }

        if (!success) {
            return false;
        }

        if (!datareader_ReadU32(&recording->Reader, &timestamp)) {
            return trc_ReportError("Could not read next packet timestamp");
        }

        if (timestamp < recording->Base.NextPacketTimestamp) {
            return trc_ReportError("Invalid packet timestamp");
        }

        recording->Base.NextPacketTimestamp = timestamp;
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

static bool tibiacast_Uncompress(struct trc_recording_tibiacast *recording,
                                 const struct trc_data_reader *reader) {
#ifdef DISABLE_ZLIB
    return trc_ReportError("Failed to decompress, zlib library missing");
#else
    z_stream stream;
    size_t size;
    int error;

    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;

    if (inflateInit2(&stream, -15) != Z_OK) {
        return trc_ReportError("Failed to initialize zlib");
    }

    stream.next_in = (Bytef *)datareader_RawData(reader);
    stream.avail_in = datareader_Remaining(reader);
    size = 0;

    /* The Tibiacast format lacks a size marker, determine actual size by
     * decompressing until EOF. */
    do {
        do {
            Bytef outBuffer[4 << 10];

            stream.next_out = outBuffer;
            stream.avail_out = sizeof(outBuffer);

            error = inflate(&stream, Z_NO_FLUSH);

            size += sizeof(outBuffer) - stream.avail_out;
        } while (stream.avail_out == 0);
    } while (error == Z_OK);

    if (inflateEnd(&stream) != Z_OK) {
        return trc_ReportError("Internal zlib state corrupt");
    }

    if (error == Z_STREAM_END) {
        /* We successfully determined the size and can move on with
         * decompression, it should not be possible to fail from here on. */
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;
        stream.avail_in = 0;
        stream.next_in = Z_NULL;

        ABORT_UNLESS(inflateInit2(&stream, -15) == Z_OK);

        recording->Uncompressed = checked_allocate(1, size);
        stream.next_in = (Bytef *)datareader_RawData(reader);
        stream.avail_in = datareader_Remaining(reader);
        stream.next_out = (Bytef *)recording->Uncompressed;
        stream.avail_out = size;

        ABORT_UNLESS(inflate(&stream, Z_FINISH) == Z_STREAM_END);
        ABORT_UNLESS(stream.avail_in == 0 && stream.avail_out == 0);
        ABORT_UNLESS(inflateEnd(&stream) == Z_OK);

        recording->Reader.Data = recording->Uncompressed;
        recording->Reader.Length = size;
        recording->Reader.Position = 0;

        return true;
    }

    return trc_ReportError("Failed to decompress data %i", error);
#endif
}

static bool tibiacast_GuessRuntime(struct trc_recording_tibiacast *recording) {
    struct trc_data_reader scanner = recording->Reader;

    do {
        uint32_t packetLength;
        uint32_t timestamp;

        if (recording->VersionMajor >= 4) {
            if (!datareader_ReadU32(&scanner, &packetLength)) {
                return trc_ReportError("Could not read packet length");
            }
        } else {
            if (!datareader_ReadU16(&scanner, &packetLength)) {
                return trc_ReportError("Could not read packet length");
            }
        }

        if (packetLength > 0) {
            if (!datareader_Skip(&scanner, packetLength)) {
            }

            if (!datareader_ReadU32(&scanner, &timestamp)) {
                return trc_ReportError("Could not read next timestamp");
            }

            if (timestamp < recording->Base.Runtime) {
                return trc_ReportError("Corrupt packet timestamp");
            }

            recording->Base.Runtime = timestamp;
        }
    } while (datareader_Remaining(&scanner) > 0);

    return true;
}

static bool tibiacast_Open(struct trc_recording_tibiacast *recording,
                           const struct trc_data_reader *file,
                           struct trc_version *version) {
    struct trc_data_reader reader = *file;

    if (!(datareader_ReadU8(&reader, &recording->VersionMajor) &&
          datareader_ReadU8(&reader, &recording->VersionMinor))) {
        return trc_ReportError("Failed to read container version");
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
        /* Tibiacast generated buggy initialization packets for recordings for a
         * short while. */
        version->Protocol.TibiacastBuggedInitialization = 1;
    }

    if (!tibiacast_Uncompress(recording, &reader)) {
        return trc_ReportError("Could not determine decompressed size");
    }

    uint32_t initial_timestamp;
    if (!datareader_ReadU32(&recording->Reader, &initial_timestamp)) {
        return trc_ReportError("Could not read initial timestamp");
    }

    if (recording->Base.Runtime == 0) {
        if (!tibiacast_GuessRuntime(recording)) {
            return trc_ReportError("Could not guess runtime");
        }
    }

    recording->Base.NextPacketTimestamp = initial_timestamp;
    recording->Base.Version = version;

    return true;
}

static void tibiacast_Free(struct trc_recording_tibiacast *recording) {
    checked_deallocate(recording->Uncompressed);
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

    return (struct trc_recording *)recording;
}
