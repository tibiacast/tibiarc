/*
 * Copyright 2011-2016 "Silver Squirrel Software Handelsbolag"
 * Copyright 2023-2025 "John Högberg"
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

#include "recordings.hpp"
#include "versions.hpp"

#ifndef DISABLE_ZLIB
extern "C" {
#    include <zlib.h>
}
#endif

#include "utils.hpp"
#include "parser.hpp"

#include <utility>

namespace trc {
namespace Recordings {
namespace Tibiacast {
using namespace trc::Events;

enum class RecordingPacketType {
    StateCorrection = 6,
    Initialization = 7,
    TibiaData = 8,
    OutgoingMessage = 9,

    First = StateCorrection,
    Last = OutgoingMessage
};

template <typename T> static T &AddEvent(Parser::EventList &events) {
    return static_cast<T &>(*events.emplace_back(std::make_unique<T>()));
}

static void ParseTibiaData(DataReader &reader,
                           Parser &parser,
                           Parser::EventList &events) {
    for (auto count = reader.ReadU16(); count > 0; count--) {
        if (auto packetReader = reader.Slice(reader.ReadU16())) {
            events.splice(events.end(), parser.Parse(packetReader));

            if (packetReader.Remaining() > 0) {
                throw InvalidDataError();
            }
        }
    }
}

static Parser::EventList ParseCreatureList(DataReader &reader,
                                           const Version &version) {
    /* HAZY: when was this widened to u16? Assume container version 4. */
    uint16_t creatureCount =
            version.AtLeast(9, 54) ? reader.ReadU16() : reader.ReadU8();
    Parser::EventList creatures;

    while (creatureCount--) {
        auto &event = AddEvent<CreatureSeen>(creatures);

        event.CreatureId = reader.ReadU32();

        if (version.Protocol.CreatureTypes) {
            event.Type = reader.Read<trc::CreatureType>();
        }

        event.Name = reader.ReadString();
        event.Health = reader.ReadU8<0, 100>();

        event.Heading = reader.Read<trc::Creature::Direction>();
        event.Outfit.Id = reader.ReadU16();

        if (event.Outfit.Id == 0) {
            event.Outfit.Item.Id = reader.ReadU16();
        } else {
            event.Outfit.HeadColor = reader.ReadU8();
            event.Outfit.PrimaryColor = reader.ReadU8();
            event.Outfit.SecondaryColor = reader.ReadU8();
            event.Outfit.DetailColor = reader.ReadU8();
            event.Outfit.Addons = reader.ReadU8();
        }

        if (version.Protocol.Mounts) {
            event.Outfit.MountId = reader.ReadU16();
        }

        event.LightIntensity = reader.ReadU8();
        event.LightColor = reader.ReadU8();
        event.Speed = reader.ReadU16();
        event.Skull = reader.Read<trc::CharacterSkull>();
        event.Shield = reader.Read<trc::PartyShield>();

        if (version.Protocol.WarIcon) {
            event.War = reader.Read<trc::WarIcon>();
        }

        if (version.Protocol.CreatureMarks) {
            if (version.Protocol.NPCCategory) {
                event.NPCCategory = reader.Read<trc::NPCCategory>();
            }

            event.Mark = reader.ReadU8();
            event.MarkIsPermanent = reader.ReadU8();
            event.GuildMembersOnline = reader.ReadU16();
        }

        if (version.Protocol.PassableCreatures) {
            event.Impassable = reader.ReadU8();
        }
    }

    return creatures;
}

static void ParseInitialization(DataReader &reader,
                                const Version &version,
                                Parser &parser,
                                Parser::EventList &events) {
    if (version.Protocol.PreviewByte) {
        reader.SkipU8();
    }

    Parser::EventList creatures = ParseCreatureList(reader, version);

    auto subpacketCount = reader.ReadU16<1>();

    /* The first Tibia data packet clears the creature list we've just parsed;
     * handle that and then splice the CreatureSeen events after that.*/
    {
        auto packetReader = reader.Slice(reader.ReadU16());
        events.splice(events.end(), parser.Parse(packetReader));

        for (const auto &event : creatures) {
            auto creatureSeen = static_cast<const CreatureSeen &>(*event);
            parser.MarkCreatureKnown(creatureSeen.CreatureId);
        }

        events.splice(events.end(), creatures);
    }

    for (int subpacketIdx = 1; subpacketIdx < subpacketCount; subpacketIdx++) {
        auto subpacketReader = reader.Slice(reader.ReadU16());
        events.splice(events.end(), parser.Parse(subpacketReader));

        if (subpacketReader.Remaining() > 0) {
            throw InvalidDataError();
        }
    }
}

static void ParseOutgoingMessage(DataReader &reader) {
    /* Name of receiver (so we know which private chat tab to put it in) */
    reader.SkipString();
    /* Message contents */
    reader.SkipString();
}

static void ParseStateCorrection(DataReader &reader) {
    /* 0 = player trade closed, 1 = npc trade closed */
    reader.ReadU8<0, 1>();
}

static bool ParsePacket(DataReader &reader,
                        const Version &version,
                        Parser &parser,
                        Recording &recording) {
    std::chrono::milliseconds timestamp;
    uint32_t packetLength;

    timestamp = std::chrono::milliseconds(reader.ReadU32());

    if (version.AtLeast(9, 54)) {
        packetLength = reader.ReadU32();
    } else {
        packetLength = reader.ReadU16();
    }

    if (packetLength > 0) {
        switch (reader.Read<RecordingPacketType>()) {
        case RecordingPacketType::Initialization: {
            auto &frame = recording.Frames.emplace_back();
            frame.Timestamp = timestamp;

            ParseInitialization(reader, version, parser, frame.Events);
            break;
        }
        case RecordingPacketType::TibiaData: {
            auto &frame = recording.Frames.emplace_back();
            frame.Timestamp = timestamp;

            ParseTibiaData(reader, parser, frame.Events);
            break;
        }
        case RecordingPacketType::StateCorrection:
            ParseStateCorrection(reader);
            break;
        case RecordingPacketType::OutgoingMessage:
            ParseOutgoingMessage(reader);
            break;
        default:
            throw InvalidDataError();
        }

        return true;
    }

    return false;
}

bool QueryTibiaVersion(const DataReader &file, VersionTriplet &triplet) {
    trc::DataReader reader = file;

    auto containerMajor = reader.ReadU8();
    auto containerMinor = reader.ReadU8();

    if (containerMajor == 3) {
        if (containerMinor < 5) {
            triplet.Major = 8;
            triplet.Minor = 55;
        } else if (containerMinor < 6) {
            triplet.Major = 8;
            triplet.Minor = 60;
        } else if (containerMinor < 8) {
            triplet.Major = 8;
            triplet.Minor = 61;
        } else if (containerMinor < 11) {
            triplet.Major = 8;
            triplet.Minor = 62;
        } else if (containerMinor < 15) {
            triplet.Major = 8;
            triplet.Minor = 71;
        } else if (containerMinor < 22) {
            triplet.Major = 9;
            triplet.Minor = 31;
        } else if (containerMinor < 26) {
            triplet.Major = 9;
            triplet.Minor = 40;
        } else if (containerMinor < 28) {
            triplet.Major = 9;
            triplet.Minor = 53;
        }
    } else if (containerMajor == 4) {
        if (containerMinor < 3) {
            triplet.Major = 9;
            triplet.Minor = 54;
        } else if (containerMinor < 5) {
            triplet.Major = 9;
            triplet.Minor = 61;
        } else if (containerMinor < 6) {
            triplet.Major = 9;
            triplet.Minor = 71;
        } else if (containerMinor < 9) {
            triplet.Major = 9;
            triplet.Minor = 80;
        } else if (containerMinor < 12) {
            /* TODO: When minor is < 10, this is supposed to be "9.83 old,"
             * and we'll need a way to differentiate them. */
            triplet.Major = 9;
            triplet.Minor = 83;
        } else if (containerMinor < 13) {
            triplet.Major = 9;
            triplet.Minor = 86;
        } else if (containerMinor < 17) {
            triplet.Major = 10;
            triplet.Minor = 0;
        } else if (containerMinor < 20) {
            triplet.Major = 10;
            triplet.Minor = 34;
        } else if (containerMinor < 21) {
            triplet.Major = 10;
            triplet.Minor = 35;
        } else if (containerMinor < 22) {
            triplet.Major = 10;
            triplet.Minor = 37;
        } else if (containerMinor < 24) {
            triplet.Major = 10;
            triplet.Minor = 51;
        } else if (containerMinor < 25) {
            triplet.Major = 10;
            triplet.Minor = 52;
        } else if (containerMinor < 26) {
            triplet.Major = 10;
            triplet.Minor = 53;
        } else if (containerMinor < 27) {
            triplet.Major = 10;
            triplet.Minor = 54;
        } else if (containerMinor < 28) {
            triplet.Major = 10;
            triplet.Minor = 57;
        } else if (containerMinor < 29) {
            triplet.Major = 10;
            triplet.Minor = 58;
        } else if (containerMinor < 30) {
            triplet.Major = 10;
            triplet.Minor = 64;
        } else if (containerMinor < 31) {
            triplet.Major = 10;
            triplet.Minor = 94;
        }
    }

    if ((containerMajor > 4) || (containerMajor == 4 && containerMinor >= 5)) {
        reader.SkipU32();
    }

    triplet.Preview = 0;
    if ((containerMajor > 4) || (containerMajor == 4 && containerMinor >= 6)) {
        triplet.Preview = reader.ReadU8();
    }

    if (containerMajor == 4 && containerMinor < 10) {
        triplet.Preview = 0;
    }

    return true;
}

static std::unique_ptr<uint8_t[]> Uncompress(const trc::DataReader &reader,
                                             size_t &size) {
#ifdef DISABLE_ZLIB
    throw NotSupportedError();
#else
    z_stream stream;
    int error;

    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;

    AbortUnless(inflateInit2(&stream, -15) == Z_OK);

    stream.next_in = (Bytef *)reader.RawData();
    stream.avail_in = reader.Remaining();
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
        throw InvalidDataError();
    }

    if (error != Z_STREAM_END) {
        throw InvalidDataError();
    }

    /* We successfully determined the size and can move on with
     * decompression, it should not be possible to fail from here on. */
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;

    AbortUnless(inflateInit2(&stream, -15) == Z_OK);

    auto buffer = std::make_unique<uint8_t[]>(size);

    stream.next_in = (Bytef *)reader.RawData();
    stream.avail_in = reader.Remaining();
    stream.next_out = (Bytef *)buffer.get();
    stream.avail_out = size;

    AbortUnless(inflate(&stream, Z_FINISH) == Z_STREAM_END);
    AbortUnless(stream.avail_in == 0 && stream.avail_out == 0);
    AbortUnless(inflateEnd(&stream) == Z_OK);

    return buffer;
#endif
}

std::pair<std::unique_ptr<Recording>, bool> Read(const DataReader &file,
                                                 const Version &version,
                                                 Recovery recovery) {
    auto recording = std::make_unique<Recording>();
    bool partialReturn = false;
    DataReader reader = file;

    reader.SkipU8();
    reader.SkipU8();

    if (version.AtLeast(9, 54)) {
        recording->Runtime = std::chrono::milliseconds(reader.ReadU32());
    }

    if (version.AtLeast(9, 80)) {
        /* Preview flag. */
        reader.SkipU8();
    }

    size_t size;
    auto buffer = Uncompress(reader, size);
    DataReader uncompressed(size, buffer.get());

    try {
        Parser parser(version, recovery == Recovery::Repair);

        while (ParsePacket(uncompressed, version, parser, *recording)) {
            /* */
        }

        if (recording->Frames.empty()) {
            throw InvalidDataError();
        }
    } catch ([[maybe_unused]] const InvalidDataError &e) {
        partialReturn = true;
    }

    if (!version.AtLeast(9, 54)) {
        recording->Runtime = recording->Frames.back().Timestamp;
    }

    return std::make_pair(std::move(recording), partialReturn);
}

} // namespace Tibiacast
} // namespace Recordings
} // namespace trc
