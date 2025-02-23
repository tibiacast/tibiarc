/*
 * Copyright 2024 "John Högberg"
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

#ifndef __TRC_DEMUXER_HPP__
#define __TRC_DEMUXER_HPP__

#include "datareader.hpp"

#include <cstdint>
#include <cstdlib>

namespace trc {
class Demuxer {
    enum class State { Header, Payload } State;
    uint8_t HeaderSize;
    uint32_t Timestamp;

    size_t Remaining;
    size_t Used;
    uint8_t Buffer[128 << 10];

public:
    template <typename Lambda>
    void Submit(uint32_t timestamp, DataReader &reader, Lambda process) {

        while (reader.Remaining() > 0) {
            if (Remaining == 0) {
                switch (State) {
                case State::Payload: {
                    process(DataReader(Used, Buffer), Timestamp);

                    State = State::Header;
                    Remaining = HeaderSize;
                    Used = 0;
                    break;
                }
                case State::Header: {
                    Assert(Used == HeaderSize);

                    auto header = &Buffer[0];
                    Used = 0;

                    Remaining = ((uint16_t)header[0] << 0x00) |
                                ((uint16_t)header[1] << 0x08);
                    if (HeaderSize == 4) {
                        Remaining |= ((uint32_t)header[2] << 0x10) |
                                     ((uint32_t)header[3] << 0x18);
                    }

                    if (Remaining > sizeof(Buffer)) {
                        throw InvalidDataError();
                    }

                    State = State::Payload;
                    Timestamp = timestamp;
                    break;
                }
                }
            }

            size_t to_copy = std::min(reader.Remaining(), Remaining);

            reader.Copy(to_copy, &Buffer[Used]);

            Remaining -= to_copy;
            Used += to_copy;
        }
    }

    void Finish() {
        if (Remaining > 0) {
            throw InvalidDataError();
        }
    }

    Demuxer(uint8_t headerSize)
        : State(State::Header),
          HeaderSize(headerSize),
          Timestamp(0),
          Remaining(HeaderSize),
          Used(0) {
    }
};
} // namespace trc

#endif
