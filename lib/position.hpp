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

#ifndef __TRC_POSITION_HPP__
#define __TRC_POSITION_HPP__

#include "utils.hpp"

#include <cstdint>

namespace trc {
struct Position {
    uint16_t X;
    uint16_t Y;
    uint8_t Z;

    Position(uint16_t x, uint16_t y, uint8_t z) : X(x), Y(y), Z(z) {
    }

    Position() : X(0xFFFF), Y(0), Z(0) {
        /* Null position according to the protocol, used for inventory and so
         * on. */
    }
};

} // namespace trc

#endif /* __TRC_POSITION_HPP__ */
