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

#ifndef __TRC_DATAREADER_HPP__
#define __TRC_DATAREADER_HPP__

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include <type_traits>
#include <limits>
#include <string>
#include <bit>

#include "utils.hpp"

namespace trc {

class DataReader {
    void CheckRemaining(size_t count) const {
        if (Remaining() < count) {
            throw InvalidDataError();
        }
    }

    DataReader(size_t position, size_t length, const uint8_t *data)
        : Position(position), Length(length), Data(data) {
        if (Position > Length) {
            throw InvalidDataError();
        }
    }

public:
    size_t Position;
    size_t Length;
    const uint8_t *Data;

    DataReader(size_t length, const uint8_t *data)
        : DataReader(0, length, data) {
    }

    size_t Remaining() const {
        return (Length - Position);
    }

    auto RawData() const {
        return &Data[Position];
    }

    void Skip(size_t count) {
        CheckRemaining(count);
        Position += count;
    }

    size_t Tell() {
        return Position;
    }

    DataReader Seek(size_t to) {
        return DataReader(to, Length, Data);
    }

    void Copy(size_t count, uint8_t *destination) {
        CheckRemaining(count);

        memcpy(destination, &Data[Position], count);
        Position += count;
    }

    DataReader Slice(size_t count) {
        CheckRemaining(count);

        const uint8_t *base = &Data[Position];
        Position += count;
        return DataReader(count, base);
    }

    operator bool() const {
        return Remaining() > 0;
    }

    template <typename T,
              std::enable_if_t<std::is_integral<T>::value, bool> = true>
    T Peek() const {
        using U = std::make_unsigned<T>::type;

        CheckRemaining(sizeof(T));

        const uint8_t *base = &Data[Position];
        auto result = static_cast<U>(base[0]);

        for (auto byte = 1u; byte < sizeof(T); byte++) {
            result |= static_cast<U>(base[byte]) << (byte * 8);
        }

        return std::bit_cast<T, U>(result);
    }

    template <typename T,
              typename Size = uint8_t,
              std::enable_if_t<std::is_enum<T>::value, bool> = true>
    T Read() {
        Size candidate = Read<Size,
                              static_cast<Size>(T::First),
                              static_cast<Size>(T::Last)>();
        return static_cast<T>(candidate);
    }

    template <typename T,
              T Min = std::numeric_limits<T>::min(),
              T Max = std::numeric_limits<T>::max(),
              std::enable_if_t<std::is_integral<T>::value, bool> = true>
    T Read() {
        auto result = Peek<T>();

        if (result < Min || result > Max) {
            throw InvalidDataError();
        }

        Position += sizeof(T);
        return result;
    }

    template <typename T,
              std::enable_if_t<std::is_same<T, double>::value, bool> = true>
    T Read() {
        auto Exponent = Read<uint8_t>();
        auto Significand = Read<uint32_t>();

        T result =
                (static_cast<double>(Significand) -
                 std::numeric_limits<int32_t>::max()) /
                std::pow<double, double>(10.0, static_cast<double>(Exponent));

        return result;
    }

    template <
            typename T,
            std::enable_if_t<std::is_same<T, std::string>::value, bool> = true>
    T Read() {
        auto count = Read<uint16_t>();

        CheckRemaining(count);

        auto base = &Data[Position];
        Position += count;

        return T(reinterpret_cast<const char *>(base), count);
    }

    template <typename T, std::enable_if_t<std::is_enum<T>::value, bool> = true>
    void Skip() {
        (void)Read<T>();
    }

    /* FIXME: There must be a better way to do this in C++ ... */
#define __DataReader_GenIntegral__(T, Name)                                    \
    template <T Min = std::numeric_limits<T>::min(),                           \
              T Max = std::numeric_limits<T>::max()>                           \
    inline T Read##Name() {                                                    \
        return Read<T, Min, Max>();                                            \
    }                                                                          \
    template <T Min = std::numeric_limits<T>::min(),                           \
              T Max = std::numeric_limits<T>::max()>                           \
    inline void Skip##Name() {                                                 \
        (void)Read<T, Min, Max>();                                             \
    }

    __DataReader_GenIntegral__(uint8_t, U8);
    __DataReader_GenIntegral__(uint16_t, U16);
    __DataReader_GenIntegral__(uint32_t, U32);
    __DataReader_GenIntegral__(uint64_t, U64);
    __DataReader_GenIntegral__(int8_t, S8);
    __DataReader_GenIntegral__(int16_t, S16);
    __DataReader_GenIntegral__(int32_t, S32);
    __DataReader_GenIntegral__(int64_t, S64);

#undef __DataReader_GenIntegral__

    double ReadFloat() {
        return Read<double>();
    }

    void SkipFloat() {
        (void)Read<double>();
    }

    std::string ReadString() {
        return Read<std::string>();
    }

    void SkipString() {
        (void)Read<std::string>();
    }
};
}; // namespace trc

#endif /* __TRC_DATAREADER_HPP__ */
