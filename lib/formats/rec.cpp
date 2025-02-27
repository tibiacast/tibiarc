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

#include "utils.hpp"

#ifndef DISABLE_OPENSSL_CRYPTO
extern "C" {
#    include <openssl/evp.h>
}
#endif

namespace trc {
namespace Recordings {
namespace Rec {

#ifndef DISABLE_OPENSSL_CRYPTO
static const uint8_t AESKey[32] = {
        0x54, 0x68, 0x79, 0x20, 0x6B, 0x65, 0x79, 0x20, 0x69, 0x73, 0x20,
        0x6D, 0x69, 0x6E, 0x65, 0x20, 0xA9, 0x20, 0x32, 0x30, 0x30, 0x36,
        0x20, 0x47, 0x42, 0x20, 0x4D, 0x6F, 0x6E, 0x61, 0x63, 0x6F};
#endif

/* Early .REC versions have 32-bit frame lengths, but I haven't observed any
 * recordings with an actual frame length larger than 64K. */
static constexpr int MaxFrameSize = (64 << 10);

struct State {
    struct {
        uint32_t Timestamp;
        uint32_t Length;
        uint32_t Checksum;

        uint8_t *CipherData;
        uint8_t *PlainData;
    } Fragment;

    struct {
#ifndef DISABLE_OPENSSL_CRYPTO
        EVP_CIPHER_CTX *AES;
#endif

        bool Checksum;
        bool Encrypted;
        int32_t Twirl;
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
#ifndef DISABLE_OPENSSL_CRYPTO
            Obfuscation.AES = EVP_CIPHER_CTX_new();
            EVP_CIPHER_CTX_init(Obfuscation.AES);

            if (!EVP_DecryptInit(Obfuscation.AES,
                                 EVP_aes_256_ecb(),
                                 AESKey,
                                 NULL)) {
                EVP_CIPHER_CTX_free(Obfuscation.AES);
                throw NotSupportedError();
            }

            Fragment.PlainData = new uint8_t[MaxFrameSize * 2];
            Fragment.CipherData = &Fragment.PlainData[MaxFrameSize];
#else
            throw NotSupportedError();
#endif
        } else {
            Fragment.PlainData = new uint8_t[MaxFrameSize * 1];
            Fragment.CipherData = Fragment.PlainData;
        }
    }

    ~State() {
#ifndef DISABLE_OPENSSL_CRYPTO
        if (Obfuscation.AES != nullptr) {
            EVP_CIPHER_CTX_free(Obfuscation.AES);
        }
#endif

        delete[] Fragment.PlainData;
    }

    void Deobfuscate() {
        if (Obfuscation.Twirl > 0) {
            const uint32_t key =
                    (Fragment.Length + Fragment.Timestamp + 2) & 0xFF;

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

#ifndef DISABLE_OPENSSL_CRYPTO
        if (Obfuscation.Encrypted) {
            int plainLength, outLength;

            if (Fragment.Length % 16) {
                throw InvalidDataError();
            }

            AbortUnless(EVP_DecryptInit_ex(Obfuscation.AES,
                                           NULL,
                                           NULL,
                                           NULL,
                                           NULL));

            plainLength = MaxFrameSize;
            if (!EVP_DecryptUpdate(Obfuscation.AES,
                                   Fragment.PlainData,
                                   &plainLength,
                                   Fragment.CipherData,
                                   Fragment.Length)) {
                throw InvalidDataError();
            }

            Assert(plainLength < MaxFrameSize);
            outLength = MaxFrameSize - plainLength;

            if (!EVP_DecryptFinal_ex(Obfuscation.AES,
                                     &Fragment.PlainData[plainLength],
                                     &outLength)) {
                throw InvalidDataError();
            }

            Assert(outLength < 16);

            Fragment.Length = plainLength + outLength;
        }
#endif
    }
};

bool QueryTibiaVersion([[maybe_unused]] const DataReader &file,
                       [[maybe_unused]] int &major,
                       [[maybe_unused]] int &minor,
                       [[maybe_unused]] int &preview) {
    return false;
}

std::unique_ptr<Recording> Read(const DataReader &file,
                                const Version &version,
                                Recovery recovery) {
    DataReader reader = file;

    auto containerVersion = reader.ReadU16();
    auto fragmentCount = reader.ReadS32();

    struct State state(containerVersion, fragmentCount);
    auto recording = std::make_unique<Recording>();

    try {
        Parser parser(version, recovery == Recovery::Repair);
        Demuxer demuxer(2);

        for (uint32_t i = 0; i < state.FragmentCount; i++) {
            if (state.FrameLength == 2) {
                state.Fragment.Length = reader.ReadU16();
            } else {
                Assert(state.FrameLength == 4);
                state.Fragment.Length = reader.ReadU32<0, MaxFrameSize>();
            }

            state.Fragment.Timestamp = reader.ReadU32();
            reader.Copy(state.Fragment.Length, state.Fragment.CipherData);

            state.Deobfuscate();

            DataReader fragment(state.Fragment.Length,
                                state.Fragment.PlainData);
            demuxer.Submit(state.Fragment.Timestamp,
                           fragment,
                           [&](DataReader packetReader, uint32_t timestamp) {
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
        if (recovery != Recovery::PartialReturn) {
            throw;
        }
    }

    recording->Runtime =
            std::max(recording->Runtime, recording->Frames.back().Timestamp);

    return recording;
}

} // namespace Rec
} // namespace Recordings
} // namespace trc
