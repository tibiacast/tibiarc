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

#ifndef __TRC_CHARACTERSET_H__
#define __TRC_CHARACTERSET_H__

#include <stdbool.h>
#include <stdint.h>

#include "utils.h"

TRC_UNUSED static bool characterset_IsUpper(uint8_t inCharacter) {
    return (inCharacter >= 65 && inCharacter <= 90) ||
           (inCharacter >= 196 && inCharacter <= 223);
}

TRC_UNUSED static bool characterset_IsLower(uint8_t inCharacter) {
    return (inCharacter >= 97 && inCharacter <= 122) ||
           (inCharacter >= 223 && inCharacter <= 255);
}

TRC_UNUSED static uint8_t characterset_ToUpper(uint8_t inCharacter) {
    if (characterset_IsLower(inCharacter)) {
        return inCharacter - 32;
    }

    return inCharacter;
}

TRC_UNUSED static uint8_t characterset_ToLower(uint8_t inCharacter) {
    if (characterset_IsUpper(inCharacter)) {
        return inCharacter + 32;
    }

    return inCharacter;
}

TRC_UNUSED static uint8_t characterset_ConvertPrintable(uint8_t inCharacter) {
    if (inCharacter < 32) {
        return 127; /* That square we all know and love. */
    }

    return inCharacter;
}

#endif /* __TRC_CHARACTERSET_H__ */
