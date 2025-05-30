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

#include "characterset.hpp"

#include <sstream>

namespace trc {
namespace CharacterSet {

static const std::string Windows1252ToUtf8[128] = {
        {{-30, -126, -84}, 3},  {{-62, -127}, 2},       {{-30, -128, -102}, 3},
        {{-58, -110}, 2},       {{-30, -128, -98}, 3},  {{-30, -128, -90}, 3},
        {{-30, -128, -96}, 3},  {{-30, -128, -95}, 3},  {{-53, -122}, 2},
        {{-30, -128, -80}, 3},  {{-59, -96}, 2},        {{-30, -128, -71}, 3},
        {{-59, -110}, 2},       {{-62, -115}, 2},       {{-59, -67}, 2},
        {{-62, -113}, 2},       {{-62, -112}, 2},       {{-30, -128, -104}, 3},
        {{-30, -128, -103}, 3}, {{-30, -128, -100}, 3}, {{-30, -128, -99}, 3},
        {{-30, -128, -94}, 3},  {{-30, -128, -109}, 3}, {{-30, -128, -108}, 3},
        {{-53, -100}, 2},       {{-30, -124, -94}, 3},  {{-59, -95}, 2},
        {{-30, -128, -70}, 3},  {{-59, -109}, 2},       {{-62, -99}, 2},
        {{-59, -66}, 2},        {{-59, -72}, 2},        {{-62, -96}, 2},
        {{-62, -95}, 2},        {{-62, -94}, 2},        {{-62, -93}, 2},
        {{-62, -92}, 2},        {{-62, -91}, 2},        {{-62, -90}, 2},
        {{-62, -89}, 2},        {{-62, -88}, 2},        {{-62, -87}, 2},
        {{-62, -86}, 2},        {{-62, -85}, 2},        {{-62, -84}, 2},
        {{-62, -83}, 2},        {{-62, -82}, 2},        {{-62, -81}, 2},
        {{-62, -80}, 2},        {{-62, -79}, 2},        {{-62, -78}, 2},
        {{-62, -77}, 2},        {{-62, -76}, 2},        {{-62, -75}, 2},
        {{-62, -74}, 2},        {{-62, -73}, 2},        {{-62, -72}, 2},
        {{-62, -71}, 2},        {{-62, -70}, 2},        {{-62, -69}, 2},
        {{-62, -68}, 2},        {{-62, -67}, 2},        {{-62, -66}, 2},
        {{-62, -65}, 2},        {{-61, -128}, 2},       {{-61, -127}, 2},
        {{-61, -126}, 2},       {{-61, -125}, 2},       {{-61, -124}, 2},
        {{-61, -123}, 2},       {{-61, -122}, 2},       {{-61, -121}, 2},
        {{-61, -120}, 2},       {{-61, -119}, 2},       {{-61, -118}, 2},
        {{-61, -117}, 2},       {{-61, -116}, 2},       {{-61, -115}, 2},
        {{-61, -114}, 2},       {{-61, -113}, 2},       {{-61, -112}, 2},
        {{-61, -111}, 2},       {{-61, -110}, 2},       {{-61, -109}, 2},
        {{-61, -108}, 2},       {{-61, -107}, 2},       {{-61, -106}, 2},
        {{-61, -105}, 2},       {{-61, -104}, 2},       {{-61, -103}, 2},
        {{-61, -102}, 2},       {{-61, -101}, 2},       {{-61, -100}, 2},
        {{-61, -99}, 2},        {{-61, -98}, 2},        {{-61, -97}, 2},
        {{-61, -96}, 2},        {{-61, -95}, 2},        {{-61, -94}, 2},
        {{-61, -93}, 2},        {{-61, -92}, 2},        {{-61, -91}, 2},
        {{-61, -90}, 2},        {{-61, -89}, 2},        {{-61, -88}, 2},
        {{-61, -87}, 2},        {{-61, -86}, 2},        {{-61, -85}, 2},
        {{-61, -84}, 2},        {{-61, -83}, 2},        {{-61, -82}, 2},
        {{-61, -81}, 2},        {{-61, -80}, 2},        {{-61, -79}, 2},
        {{-61, -78}, 2},        {{-61, -77}, 2},        {{-61, -76}, 2},
        {{-61, -75}, 2},        {{-61, -74}, 2},        {{-61, -73}, 2},
        {{-61, -72}, 2},        {{-61, -71}, 2},        {{-61, -70}, 2},
        {{-61, -69}, 2},        {{-61, -68}, 2},        {{-61, -67}, 2},
        {{-61, -66}, 2},        {{-61, -65}, 2}};

std::string ToUtf8(const std::string &text) {
    std::stringstream result;

    for (auto character : text) {
        if (character & 0x80) {
            result << Windows1252ToUtf8[character & 0x7F];
        } else {
            result << character;
        }
    }

    return result.str();
}

} // namespace CharacterSet
} // namespace trc
