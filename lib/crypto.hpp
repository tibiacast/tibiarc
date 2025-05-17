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

#ifndef __TRC_CRYPTO_HPP__
#define __TRC_CRYPTO_HPP__

#include "utils.hpp"

#include <cstdint>
#include <memory>

namespace trc {
namespace Crypto {
struct AES_ECB_256 {
    static std::unique_ptr<AES_ECB_256> Create(const uint8_t key[32]);

    virtual size_t Decrypt(const uint8_t *input,
                           size_t inputLength,
                           uint8_t *output,
                           size_t maxLength) = 0;
    virtual ~AES_ECB_256() {
    }
};

struct SHA1 {
    static std::unique_ptr<SHA1> Create();
    static constexpr size_t Size = 20;

    virtual void Hash(const uint8_t *input,
                      size_t length,
                      uint8_t output[Size]) = 0;
    virtual ~SHA1() {
    }
};
} // namespace Crypto
} // namespace trc

#endif
