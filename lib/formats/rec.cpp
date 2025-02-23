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

#include "recordings.hpp"
#include "versions.hpp"

#include "demuxer.hpp"
#include "parser.hpp"

#include "utils.hpp"

#ifndef DISABLE_OPENSSL_CRYPTO
extern "C" {
#    include <openssl/evp.h>
}

static uint8_t aes_key[32] = {0x54, 0x68, 0x79, 0x20, 0x6B, 0x65, 0x79, 0x20,
                              0x69, 0x73, 0x20, 0x6D, 0x69, 0x6E, 0x65, 0x20,
                              0xA9, 0x20, 0x32, 0x30, 0x30, 0x36, 0x20, 0x47,
                              0x42, 0x20, 0x4D, 0x6F, 0x6E, 0x61, 0x63, 0x6F};
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

namespace trc {
namespace Recordings {
namespace Rec {
struct State {
#ifndef DISABLE_OPENSSL_CRYPTO
    EVP_CIPHER_CTX *AES;
#endif

    uint32_t Obfuscation;
    uint32_t FragmentCount;

    struct {
        uint32_t Timestamp;
        uint32_t Length;
        uint32_t Checksum;

        uint8_t *CipherData;
        uint8_t *PlainData;
    } Fragment;

    State(uint32_t obfuscation, uint32_t fragmentCount)
        : Obfuscation(obfuscation), FragmentCount(fragmentCount), Fragment({}) {
        if (obfuscation & TRC_REC_OBFUSCATION_AES_DATA) {
#ifndef DISABLE_OPENSSL_CRYPTO
            AES = EVP_CIPHER_CTX_new();
            EVP_CIPHER_CTX_init(AES);

            if (!EVP_DecryptInit(AES, EVP_aes_256_ecb(), aes_key, NULL)) {
                EVP_CIPHER_CTX_free(AES);
                throw NotSupportedError();
            }

            Fragment.PlainData = new uint8_t[TRC_REC_MAX_FRAME_SIZE * 2];
            Fragment.CipherData = &Fragment.PlainData[TRC_REC_MAX_FRAME_SIZE];
#else
            throw NotSupportedError();
#endif
        } else {
            Fragment.PlainData = new uint8_t[TRC_REC_MAX_FRAME_SIZE * 1];
            Fragment.CipherData = Fragment.PlainData;
            AES = nullptr;
        }
    }

    ~State() {
#ifndef DISABLE_OPENSSL_CRYPTO
        if (AES != nullptr) {
            EVP_CIPHER_CTX_free(AES);
        }
#endif
        delete[] Fragment.PlainData;
    }

    void Deobfuscate() {
        const int32_t divisor =
                (Obfuscation & TRC_REC_OBFUSCATION_DIVISOR_MASK);

        if (divisor > 0) {
            const uint32_t key =
                    (Fragment.Length + Fragment.Timestamp + 2) & 0xFF;

            for (uint32_t i = 0; i < Fragment.Length; i++) {
                int32_t alpha, beta;

                alpha = (key + i * 33) & 0xFF;
                alpha -= (alpha > 127) ? 256 : 0;

                beta = alpha % divisor;
                beta += (beta < 0) ? divisor : 0;

                alpha += (beta != 0) ? (divisor - beta) : 0;

                Fragment.CipherData[i] -= alpha;
            }
        }

        Assert(!!(Obfuscation & TRC_REC_OBFUSCATION_AES_DATA) ^
               (Fragment.CipherData == Fragment.PlainData));

#ifndef DISABLE_OPENSSL_CRYPTO
        if (Obfuscation & TRC_REC_OBFUSCATION_AES_DATA) {
            int plainLength, outLength;

            if (Fragment.Length % 16) {
                throw InvalidDataError();
            }

            AbortUnless(EVP_DecryptInit_ex(AES, NULL, NULL, NULL, NULL));

            plainLength = TRC_REC_MAX_FRAME_SIZE;
            if (!EVP_DecryptUpdate(AES,
                                   Fragment.PlainData,
                                   &plainLength,
                                   Fragment.CipherData,
                                   Fragment.Length)) {
                throw InvalidDataError();
            }

            Assert(plainLength < TRC_REC_MAX_FRAME_SIZE);
            outLength = TRC_REC_MAX_FRAME_SIZE - plainLength;

            if (!EVP_DecryptFinal_ex(AES,
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
    uint32_t obfuscation;

    auto containerVersion = reader.ReadU16();
    auto fragmentCount = reader.ReadS32();

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
        throw InvalidDataError();
    }

    if (containerVersion > 259) {
        obfuscation |= TRC_REC_OBFUSCATION_FRAME_COUNT_OFFSET;
        obfuscation |= TRC_REC_OBFUSCATION_U16_FRAME_LENGTHS;

        fragmentCount -= 57;
    }

    if (containerVersion >= 515) {
        obfuscation |= TRC_REC_OBFUSCATION_CHECKSUM;
    }

    if (containerVersion >= 517) {
        obfuscation |= TRC_REC_OBFUSCATION_AES_DATA;
    }

    if (fragmentCount <= 0) {
        throw InvalidDataError();
    }

    struct State state(obfuscation, fragmentCount);

    auto recording = std::make_unique<Recording>();

    try {
        Parser parser(version, recovery == Recovery::Repair);
        Demuxer demuxer(2);

        for (uint32_t i = 0; i < state.FragmentCount; i++) {
            if (state.Obfuscation & TRC_REC_OBFUSCATION_U16_FRAME_LENGTHS) {
                state.Fragment.Length = reader.ReadU16();
            } else {
                state.Fragment.Length =
                        reader.ReadU32<0, TRC_REC_MAX_FRAME_SIZE>();
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

            if (state.Obfuscation & TRC_REC_OBFUSCATION_CHECKSUM) {
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
