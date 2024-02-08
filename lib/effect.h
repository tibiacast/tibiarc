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

#ifndef __TRC_EFFECT_H__
#define __TRC_EFFECT_H__

#include <stdint.h>

struct trc_graphical_effect {
    uint8_t Id;
    uint32_t StartTick;
};

struct trc_numerical_effect {
    uint32_t StartTick;
    uint8_t Color;
    uint32_t Value;
};

#endif /* __TRC_EFFECT_H__ */
