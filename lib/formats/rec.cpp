/*
 * Copyright 2011-2016 "Silver Squirrel Software Handelsbolag"
 * Copyright 2024-2025 "John Högberg"
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

#include "demuxer.hpp"
#include "parser.hpp"
#include "crypto.hpp"

#include "utils.hpp"

#include <algorithm>
#include <chrono>

namespace trc {
namespace Recordings {
namespace Rec {
static const uint8_t AESKey[32] = {
        0x54, 0x68, 0x79, 0x20, 0x6B, 0x65, 0x79, 0x20, 0x69, 0x73, 0x20,
        0x6D, 0x69, 0x6E, 0x65, 0x20, 0xA9, 0x20, 0x32, 0x30, 0x30, 0x36,
        0x20, 0x47, 0x42, 0x20, 0x4D, 0x6F, 0x6E, 0x61, 0x63, 0x6F};

/* Early .REC versions have 32-bit frame lengths, but I haven't observed any
 * recordings with an actual frame length larger than 64K. */
static constexpr int MaxFrameSize = (64 << 10);

struct State {
    struct {
        std::chrono::milliseconds Timestamp;
        uint32_t Length;
        uint32_t Checksum;

        uint8_t *CipherData;
        uint8_t *PlainData;
    } Fragment;

    struct {
        std::unique_ptr<Crypto::AES_ECB_256> AES;

        bool Checksum = false;
        bool Encrypted = false;
        int32_t Twirl = 0;
    } Obfuscation;

    uint32_t FragmentCount;
    uint32_t FrameLength;

    State(uint32_t containerVersion, uint32_t fragmentCount)
        : Fragment({}), Obfuscation({}) {

        if (containerVersion == 259) {
            FragmentCount = fragmentCount;
            FrameLength = 4;
        } else {
            switch (containerVersion) {
            case 515:
                Obfuscation.Twirl = 5;
                break;
            case 516:
            case 517:
                Obfuscation.Twirl = 8;
                break;
            case 518:
                Obfuscation.Twirl = 6;
                break;
            default:
                throw InvalidDataError();
            }

            if (fragmentCount < 57) {
                throw InvalidDataError();
            }

            FragmentCount = fragmentCount - 57;
            FrameLength = 2;

            Obfuscation.Encrypted = (containerVersion >= 517);
            Obfuscation.Checksum = true;
        }

        if (Obfuscation.Encrypted) {
            Obfuscation.AES = Crypto::AES_ECB_256::Create(AESKey);

            Fragment.PlainData = new uint8_t[MaxFrameSize * 2];
            Fragment.CipherData = &Fragment.PlainData[MaxFrameSize];
        } else {
            Fragment.PlainData = new uint8_t[MaxFrameSize * 1];
            Fragment.CipherData = Fragment.PlainData;
        }
    }

    ~State() {
        delete[] Fragment.PlainData;
    }

    void Deobfuscate() {
        if (Obfuscation.Twirl > 0) {
            const uint32_t key =
                    (Fragment.Length + Fragment.Timestamp.count() + 2) & 0xFF;

            for (uint32_t i = 0; i < Fragment.Length; i++) {
                int32_t alpha, beta;

                alpha = (key + i * 33) & 0xFF;
                alpha -= (alpha > 127) ? 256 : 0;

                beta = alpha % Obfuscation.Twirl;
                beta += (beta < 0) ? Obfuscation.Twirl : 0;

                alpha += (beta != 0) ? (Obfuscation.Twirl - beta) : 0;

                Fragment.CipherData[i] -= alpha;
            }
        }

        Assert(Obfuscation.Encrypted ^
               (Fragment.CipherData == Fragment.PlainData));

        if (Obfuscation.Encrypted) {
            Fragment.Length = Obfuscation.AES->Decrypt(Fragment.CipherData,
                                                       Fragment.Length,
                                                       Fragment.PlainData,
                                                       MaxFrameSize);
        }
    }
};

bool QueryTibiaVersion([[maybe_unused]] const DataReader &file,
                       [[maybe_unused]] VersionTriplet &triplet) {
    return false;
}

/* TibiCAM doesn't seem to have cared about what state things were in when
 * dumping things into the recording, freely mixing game and login packets; the
 * latter can appear at any time!
 *
 * Apparently, the Tibia client doesn't choke on this, so neither should we. */
class RecParser : public Parser {
public:
    RecParser(const Version &version, bool repair) : Parser(version, repair) {
    }

    Parser::EventList ParseLogin(DataReader &reader) {
        while (reader.Remaining() > 0) {
            try {
                auto peek = reader.Peek<uint32_t>();

                if ((peek & 0xFF) == 0x0A) {
                    auto stringLength = (peek >> 8) & 0xFFFF;
                    auto firstChar = peek >> 24;

                    if ((stringLength + 3) == reader.Remaining() &&
                        CheckRange(firstChar, 'A', 'Z')) {
                        DataReader lookAhead = reader;

                        lookAhead.Skip(1);
                        auto string = lookAhead.ReadString();

                        if (std::all_of(string.cbegin(),
                                        string.cend(),
                                        [](auto character) {
                                            return character == '\n' ||
                                                   character >= ' ';
                                        })) {
                            reader = lookAhead;
                            continue;
                        }
                    }
                }
            } catch ([[maybe_unused]] const InvalidDataError &e) {
                /* */
            }

            return Parser::Parse(reader);
        }

        return {};
    }

    Parser::EventList Parse(DataReader &reader) {
        const auto backtrack = reader;

        try {
            return Parser::Parse(reader);
        } catch ([[maybe_unused]] const InvalidDataError &e) {
            /* This is either a legit parse error or an unexpected login-state
             * packet; try to recover by handling the latter. */
            reader = backtrack;

            return ParseLogin(reader);
        }
    }
};

std::pair<std::unique_ptr<Recording>, bool> Read(const DataReader &file,
                                                 const Version &version,
                                                 Recovery recovery) {
    DataReader reader = file;

    auto containerVersion = reader.ReadU16();
    auto fragmentCount = reader.ReadS32();

    auto recording = std::make_unique<Recording>();
    bool partialReturn = false;

    try {
        RecParser parser(version, recovery == Recovery::Repair);
        State state(containerVersion, fragmentCount);
        Demuxer demuxer(2);

        for (uint32_t i = 0; i < state.FragmentCount; i++) {
            /* Consider recordings that are truncated exactly at the last frame
             * boundary; this appears to be a common enough failure that I
             * suspect it's some sort of race condition in the recorder. */
            if (i == (state.FragmentCount - 1) && reader.Remaining() == 0) {
                break;
            }

            if (state.FrameLength == 2) {
                state.Fragment.Length = reader.ReadU16();
            } else {
                Assert(state.FrameLength == 4);
                state.Fragment.Length = reader.ReadU32<0, MaxFrameSize>();
            }

            state.Fragment.Timestamp =
                    std::chrono::milliseconds(reader.ReadU32());
            reader.Copy(state.Fragment.Length, state.Fragment.CipherData);

            state.Deobfuscate();

            DataReader fragment(state.Fragment.Length,
                                state.Fragment.PlainData);
            demuxer.Submit(state.Fragment.Timestamp,
                           fragment,
                           [&](DataReader packetReader, auto timestamp) {
                               recording->Frames.emplace_back(
                                       timestamp,
                                       parser.Parse(packetReader));
                           });

            if (state.Obfuscation.Checksum) {
                reader.SkipU32();
            }
        }

        demuxer.Finish();
    } catch ([[maybe_unused]] const InvalidDataError &e) {
        partialReturn = true;
    }

    recording->Runtime =
            std::max(recording->Runtime, recording->Frames.back().Timestamp);

    return std::make_pair(std::move(recording), partialReturn);
}

} // namespace Rec
} // namespace Recordings
} // namespace trc
