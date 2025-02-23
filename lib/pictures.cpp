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

#include "pictures.hpp"

#include "canvas.hpp"
#include "sprites.hpp"
#include "versions.hpp"

#include <cstdlib>

#include "utils.hpp"

namespace trc {

void PictureFile::ReadPicture(DataReader &reader, const PictureIndex index) {
    int width, height;

    width = reader.ReadU8<1, 127>();
    height = reader.ReadU8<1, 127>();

    auto [it, _] =
            Pictures.emplace(std::piecewise_construct,
                             std::forward_as_tuple(index),
                             std::forward_as_tuple(width * 32, height * 32));
    auto &canvas = it->second;

    /* Color key */
    reader.Skip(3);
    canvas.Wipe();

    for (int yIdx = 0; yIdx < height; yIdx++) {
        for (int xIdx = 0; xIdx < width; xIdx++) {
            size_t spriteOffset = reader.ReadU32();

            auto indexReader = reader.Seek(spriteOffset);

            if (size_t dataLength = indexReader.ReadU16()) {
                auto spriteReader = indexReader.Slice(dataLength);
                Sprite sprite(spriteReader, 32, 32);
                canvas.Draw(sprite, 32 * xIdx, 32 * yIdx, 32, 32);
            }
        }
    }
}

PictureFile::PictureFile(const VersionBase &version, DataReader data) {
    Signature = data.ReadU32();

    /* Picture count, must be kept in sync with the code below. */
    data.SkipU16<8, 9>();

    ReadPicture(data, PictureIndex::SplashBackground);

    /* FIXME: When was this added? I don't have the .pic files for most
     * versions, just .dat and .spr */
    if (version.AtLeast(9, 00)) {
        ReadPicture(data, PictureIndex::SplashLogo);
    }

    ReadPicture(data, PictureIndex::Tutorial);
    ReadPicture(data, PictureIndex::FontUnbordered);
    ReadPicture(data, PictureIndex::Icons);
    ReadPicture(data, PictureIndex::FontGame);
    ReadPicture(data, PictureIndex::FontInterfaceSmall);
    ReadPicture(data, PictureIndex::LightFallbacks);
    ReadPicture(data, PictureIndex::FontInterfaceLarge);

#ifdef DUMP_PIC
    for (const auto &[index, canvas] : Pictures) {
        char path[128];
        snprintf(path, sizeof(path), "pict_%i.bmp", static_cast<int>(index));
        canvas.Dump(path);
    }
#endif
}
} // namespace trc
