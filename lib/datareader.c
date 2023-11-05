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

#include "datareader.h"
#include "characterset.h"

#include <limits.h>
#include <math.h>

#include "utils.h"

bool datareader_ReadFloat(struct trc_data_reader *reader, double *result) {
    uint8_t Exponent = 0;
    uint32_t Significand = 0;

    if (!datareader_ReadU8(reader, &Exponent)) {
        return false;
    }

    if (!datareader_ReadU32(reader, &Significand)) {
        return false;
    }

    (*result) = ((double)Significand - INT_MAX) / pow(10.0, (double)Exponent);

    return true;
}

bool datareader_SkipString(struct trc_data_reader *reader) {
    uint16_t stringLength = 0;

    if (!datareader_ReadU16(reader, &stringLength)) {
        return false;
    }

    if (reader->Position + stringLength > reader->Length) {
        return false;
    }

    reader->Position += stringLength;

    return true;
}
