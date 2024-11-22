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

#ifndef __TRC_PICTURES_HPP__
#define __TRC_PICTURES_HPP__

#include <cstdint>
#include <unordered_map>

#include "versions_decl.hpp"

#include "canvas.hpp"
#include "datareader.hpp"

namespace trc {

enum class PictureIndex {
    SplashBackground,
    SplashLogo,
    Tutorial,
    FontUnbordered,
    Icons,
    FontGame,
    FontInterfaceSmall,
    LightFallbacks,
    FontInterfaceLarge,
};

class PictureFile {
    std::unordered_map<PictureIndex, Canvas> Pictures;

    void ReadPicture(DataReader &data, PictureIndex index);

public:
    uint32_t Signature;

    PictureFile(const VersionBase &version, DataReader data);

    const Canvas &Get(PictureIndex index) const {
        auto it = Pictures.find(index);

        if (it != Pictures.end()) {
            return it->second;
        }

        throw InvalidDataError();
    }
};
} // namespace trc

#endif /* __TRC_PICTURES_HPP__ */
