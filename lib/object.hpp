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

#ifndef __TRC_OBJECT_HPP__
#define __TRC_OBJECT_HPP__

#include <cstdint>

namespace trc {
struct Object {
    static constexpr uint16_t CreatureMarker = 0x63;

    uint16_t Id;

    union {
        uint32_t CreatureId;
        struct {
            uint8_t ExtraByte;
            uint8_t Animation;
            uint8_t Mark;
        };
    };

    uint32_t PhaseTick;

    Object(uint16_t id) : Id(id), PhaseTick(0) {
    }

    Object() : Object(0) {
    }

    bool IsCreature() const {
        return Id == CreatureMarker;
    }
};
} // namespace trc

#endif /* __TRC_OBJECT_HPP__ */
