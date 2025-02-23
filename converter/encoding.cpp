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

#include "encoding.hpp"

#include "utils.hpp"

#include "encoders/inert.hpp"
#include "encoders/libav.hpp"

namespace trc {
namespace Encoding {

std::unique_ptr<Encoder> Open(Backend backend,
                              const std::string &format,
                              const std::string &encoding,
                              const std::string &flags,
                              int width,
                              int height,
                              int frameRate,
                              const std::string &path) {
    switch (backend) {
    case Backend::Inert:
        return std::make_unique<Inert>();
    case Backend::LibAV:
#ifndef DISABLE_LIBAV
        return std::make_unique<LibAV>(format,
                                       encoding,
                                       flags,
                                       width,
                                       height,
                                       frameRate,
                                       path);
#else
        throw NotSupportedError();
#endif
    default:
        abort();
    }
}

Encoder::~Encoder() {
}

} // namespace Encoding
} // namespace trc
