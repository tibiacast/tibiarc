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

#ifndef __TRC_CHARACTERSET_HPP__
#define __TRC_CHARACTERSET_HPP__

#include <cstdint>
#include <string>

namespace trc {
namespace CharacterSet {
static inline uint8_t ToUpper(uint8_t c) {
    switch (c) {
    case 0x9A:
        /* š -> Š */
        return 0x8A;
    case 0x9C:
        /* œ -> Œ */
        return 0x8C;
    case 0x9E:
        /* ž -> Ž */
        return 0x8E;
    case 0xFF:
        /* ÿ -> Ÿ */
        return 0x9F;
    case 0xD7:
    case 0xF7:
        /* ×, ÷ */
        break;
    default:
        if ((c >= 97 && c <= 122) || (c > 223)) {
            return c - 32;
        }
    }

    return c;
}

static inline uint8_t ToLower(uint8_t c) {
    switch (c) {
    case 0x8A:
        /* Š -> š */
        return 0x9A;
    case 0x8C:
        /* Œ -> œ */
        return 0x9C;
    case 0x8E:
        /* Ž -> ž */
        return 0x9E;
    case 0x9F:
        /* Ÿ -> ÿ */
        return 0xFF;
    case 0xD7:
    case 0xF7:
        /* ×, ÷ */
        break;
    default:
        if ((c >= 65 && c <= 90) || (c >= 192 && c < 223)) {
            return c + 32;
        }
    }

    return c;
}

static inline bool IsUpper(uint8_t c) {
    return ToUpper(c) == c;
}

static inline bool IsLower(uint8_t c) {
    return ToLower(c) == c;
}

inline uint8_t ToPrintable(uint8_t c) {
    if (c < 32) {
        return 127; /* That square we all know and love. */
    }

    return c;
}

std::string ToUtf8(const std::string &text);
}; // namespace CharacterSet
}; // namespace trc

#endif /* __TRC_CHARACTERSET_HPP__ */
