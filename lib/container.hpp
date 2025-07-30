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

#ifndef __TRC_CONTAINER_HPP__
#define __TRC_CONTAINER_HPP__

#include "object.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace trc {
struct Container {
    std::string Name;

    uint16_t ItemId;
    uint8_t Mark;
    uint8_t Animation;
    uint8_t SlotsPerPage;
    uint8_t HasParent;

    uint8_t DragAndDrop;
    uint8_t Pagination;

    uint16_t StartIndex;
    uint16_t TotalObjects;

    uint16_t Size;

    std::vector<Object> Items;
};
} // namespace trc

#endif /* __TRC_CONTAINER_HPP__ */
