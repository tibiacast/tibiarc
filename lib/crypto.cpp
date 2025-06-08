/*
 * Copyright 2025 "John Högberg"
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

#include "crypto.hpp"

#if defined(_WIN32) && !defined(DISABLE_BCRYPT_CRYPTO)
extern "C" {
#    include <windows.h>
#    include <ntstatus.h>
#    include <bcrypt.h>
}
#elif !defined(DISABLE_OPENSSL_CRYPTO)
extern "C" {
#    include <openssl/evp.h>
}
#endif

namespace trc {
namespace Crypto {

#if defined(_WIN32) && !defined(DISABLE_BCRYPT_CRYPTO)
class AES_ECB_256__ : public AES_ECB_256 {
    BCRYPT_ALG_HANDLE Algorithm = nullptr;
    BCRYPT_KEY_HANDLE Key = nullptr;

public:
    AES_ECB_256__(const uint8_t key[32]) : AES_ECB_256() {
        if (BCryptOpenAlgorithmProvider(&Algorithm,
                                        BCRYPT_AES_ALGORITHM,
                                        nullptr,
                                        0) != STATUS_SUCCESS) {
            throw NotSupportedError();
        }

        if (BCryptSetProperty(Algorithm,
                              BCRYPT_CHAINING_MODE,
                              (BYTE *)BCRYPT_CHAIN_MODE_ECB,
                              sizeof(BCRYPT_CHAIN_MODE_ECB),
                              0) != STATUS_SUCCESS) {
            BCryptCloseAlgorithmProvider(Algorithm, 0);
            throw NotSupportedError();
        }

        if (BCryptGenerateSymmetricKey(Algorithm,
                                       &Key,
                                       nullptr,
                                       0,
                                       const_cast<uint8_t *>(key),
                                       32,
                                       0)) {
            BCryptCloseAlgorithmProvider(Algorithm, 0);
            throw NotSupportedError();
        }
    }

    virtual size_t Decrypt(const uint8_t *input,
                           size_t inputLength,
                           uint8_t *output,
                           size_t maxLength) {
        if (inputLength % 16) {
            throw InvalidDataError();
        }

        ULONG outLength;
        if (BCryptDecrypt(Key,
                          const_cast<uint8_t *>(input),
                          inputLength,
                          nullptr,
                          nullptr,
                          0,
                          output,
                          maxLength,
                          &outLength,
                          BCRYPT_BLOCK_PADDING) != STATUS_SUCCESS) {
            throw InvalidDataError();
        }

        return outLength;
    }

    ~AES_ECB_256__() {
        BCryptCloseAlgorithmProvider(Algorithm, 0);
        BCryptDestroyKey(Key);
    }
};

std::unique_ptr<AES_ECB_256> AES_ECB_256::Create(const uint8_t key[32]) {
    return std::make_unique<AES_ECB_256__>(key);
}
#elif !defined(DISABLE_OPENSSL_CRYPTO)
class AES_ECB_256__ : public AES_ECB_256 {
    EVP_CIPHER_CTX *AES;

public:
    AES_ECB_256__(const uint8_t key[32]) : AES_ECB_256() {
        AES = EVP_CIPHER_CTX_new();
        EVP_CIPHER_CTX_init(AES);

        if (!EVP_DecryptInit(AES, EVP_aes_256_ecb(), key, NULL)) {
            EVP_CIPHER_CTX_free(AES);
            throw NotSupportedError();
        }
    }

    virtual size_t Decrypt(const uint8_t *input,
                           size_t inputLength,
                           uint8_t *output,
                           size_t maxLength) {
        if (inputLength % 16) {
            throw InvalidDataError();
        }

        AbortUnless(EVP_DecryptInit_ex(AES, NULL, NULL, NULL, NULL));

        int plainLength = maxLength;
        if (!EVP_DecryptUpdate(AES, output, &plainLength, input, inputLength)) {
            throw InvalidDataError();
        }

        int outLength = maxLength - plainLength;
        if (!EVP_DecryptFinal_ex(AES, &output[plainLength], &outLength)) {
            throw InvalidDataError();
        }

        return plainLength + outLength;
    }

    ~AES_ECB_256__() {
        EVP_CIPHER_CTX_free(AES);
    }
};

std::unique_ptr<AES_ECB_256> AES_ECB_256::Create(const uint8_t key[32]) {
    return std::make_unique<AES_ECB_256__>(key);
}
#else
std::unique_ptr<AES_ECB_256> AES_ECB_256::Create(
        [[maybe_unused]] const uint8_t key[32]) {
    throw NotSupportedError();
}
#endif

#if defined(_WIN32) && !defined(DISABLE_BCRYPT_CRYPTO)
class SHA1__ : public SHA1 {
    BCRYPT_ALG_HANDLE Algorithm;
    BCRYPT_ALG_HANDLE Context;
    PBYTE Object;

public:
    SHA1__() : SHA1() {
        if (BCryptOpenAlgorithmProvider(&Algorithm,
                                        BCRYPT_SHA1_ALGORITHM,
                                        nullptr,
                                        BCRYPT_HASH_REUSABLE_FLAG) !=
            STATUS_SUCCESS) {
            throw NotSupportedError();
        }

        DWORD objectSize, dataLength;
        if (BCryptGetProperty(Algorithm,
                              BCRYPT_OBJECT_LENGTH,
                              (PBYTE)&objectSize,
                              sizeof(DWORD),
                              &dataLength,
                              0) != STATUS_SUCCESS) {
            BCryptCloseAlgorithmProvider(Algorithm, 0);
            throw NotSupportedError();
        }

        Object = new BYTE[objectSize];
        if (BCryptCreateHash(Algorithm,
                             &Context,
                             Object,
                             objectSize,
                             NULL,
                             0,
                             BCRYPT_HASH_REUSABLE_FLAG) != STATUS_SUCCESS) {
            BCryptCloseAlgorithmProvider(Algorithm, 0);
            throw NotSupportedError();
        }
    }

    virtual void Hash(const uint8_t *input,
                      size_t length,
                      uint8_t output[Size]) {
        AbortUnless(BCryptHashData(Context, (PBYTE)input, length, 0) ==
                    STATUS_SUCCESS);
        AbortUnless(BCryptFinishHash(Context, output, Size, 0) ==
                    STATUS_SUCCESS);
    }

    ~SHA1__() {
        BCryptCloseAlgorithmProvider(Algorithm, 0);
        BCryptDestroyHash(Context);
        delete[] Object;
    }
};

std::unique_ptr<SHA1> SHA1::Create() {
    return std::make_unique<SHA1__>();
}
#elif !defined(DISABLE_OPENSSL_CRYPTO)
class SHA1__ : public SHA1 {
    EVP_MD_CTX *Context;

public:
    SHA1__() : SHA1() {
        Context = EVP_MD_CTX_new();

        if (!EVP_DigestInit_ex(Context, EVP_sha1(), NULL)) {
            EVP_MD_CTX_free(Context);
            throw NotSupportedError();
        }
    }

    virtual void Hash(const uint8_t *input,
                      size_t length,
                      uint8_t output[Size]) {
        EVP_DigestInit_ex(Context, NULL, NULL);
        EVP_DigestUpdate(Context, input, length);

        unsigned int size;
        EVP_DigestFinal_ex(Context, output, &size);

        AbortUnless(size == Size);
    }

    ~SHA1__() {
        EVP_MD_CTX_free(Context);
    }
};

std::unique_ptr<SHA1> SHA1::Create() {
    return std::make_unique<SHA1__>();
}
#else
std::unique_ptr<SHA1> SHA1::Create() {
    throw NotSupportedError();
}
#endif

} // namespace Crypto
} // namespace trc
