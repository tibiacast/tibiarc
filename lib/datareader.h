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

#ifndef __TRC_DATAREADER_H__
#define __TRC_DATAREADER_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct trc_data_reader {
    size_t Position;
    size_t Length;
    const uint8_t *Data;
};

#define datareader_Remaining(Reader)                                           \
    (ASSERT((Reader)->Length >= (Reader)->Position),                           \
     (Reader)->Length - (Reader)->Position)

#define datareader_Peek8(Type, Reader, Result)                                 \
    (datareader_Remaining(Reader) < 1                                          \
             ? false                                                           \
             : (*Result = (Type)((Reader)->Data[(Reader)->Position]), true))

#define datareader_Peek16(Type, Reader, Result)                                \
    ((datareader_Remaining(Reader) < 2 || sizeof(*Result) < 2)                 \
             ? false                                                           \
             : (*Result = (Type)(((uint16_t)(Reader)                           \
                                          ->Data[(Reader)->Position + 0])      \
                                         << 0x00 |                             \
                                 ((uint16_t)(Reader)                           \
                                          ->Data[(Reader)->Position + 1])      \
                                         << 0x08),                             \
                true))

#define datareader_Peek32(Type, Reader, Result)                                \
    ((datareader_Remaining(Reader) < 4 || sizeof(*Result) < 4)                 \
             ? false                                                           \
             : (*Result = (Type)(((uint32_t)(Reader)                           \
                                          ->Data[(Reader)->Position + 0])      \
                                         << 0x00 |                             \
                                 ((uint32_t)(Reader)                           \
                                          ->Data[(Reader)->Position + 1])      \
                                         << 0x08 |                             \
                                 ((uint32_t)(Reader)                           \
                                          ->Data[(Reader)->Position + 2])      \
                                         << 0x10 |                             \
                                 ((uint32_t)(Reader)                           \
                                          ->Data[(Reader)->Position + 3])      \
                                         << 0x18),                             \
                true))

#define datareader_Peek64(Type, Reader, Result)                                \
    ((datareader_Remaining(Reader) < 8 || sizeof(*Result) < 8)                 \
             ? false                                                           \
             : (*Result = (Type)(((uint64_t)(Reader)                           \
                                          ->Data[(Reader)->Position + 0])      \
                                         << 0x00 |                             \
                                 ((uint64_t)(Reader)                           \
                                          ->Data[(Reader)->Position + 1])      \
                                         << 0x08 |                             \
                                 ((uint64_t)(Reader)                           \
                                          ->Data[(Reader)->Position + 2])      \
                                         << 0x10 |                             \
                                 ((uint64_t)(Reader)                           \
                                          ->Data[(Reader)->Position + 3])      \
                                         << 0x18 |                             \
                                 ((uint64_t)(Reader)                           \
                                          ->Data[(Reader)->Position + 4])      \
                                         << 0x20 |                             \
                                 ((uint64_t)(Reader)                           \
                                          ->Data[(Reader)->Position + 5])      \
                                         << 0x28 |                             \
                                 ((uint64_t)(Reader)                           \
                                          ->Data[(Reader)->Position + 6])      \
                                         << 0x30 |                             \
                                 ((uint64_t)(Reader)                           \
                                          ->Data[(Reader)->Position + 7])      \
                                         << 0x38),                             \
                true))

#define datareader_PeekU8(Reader, Result)                                      \
    datareader_Peek8(uint8_t, Reader, Result)
#define datareader_PeekU16(Reader, Result)                                     \
    datareader_Peek16(uint16_t, Reader, Result)
#define datareader_PeekU32(Reader, Result)                                     \
    datareader_Peek32(uint32_t, Reader, Result)
#define datareader_PeekU64(Reader, Result)                                     \
    datareader_Peek64(uint64_t, Reader, Result)

#define datareader_PeekI8(Reader, Result)                                      \
    datareader_Peek8(int8_t, Reader, Result)
#define datareader_PeekI16(Reader, Result)                                     \
    datareader_Peek16(int16_t, Reader, Result)
#define datareader_PeekI32(Reader, Result)                                     \
    datareader_Peek32(int32_t, Reader, Result)
#define datareader_PeekI64(Reader, Result)                                     \
    datareader_Peek64(int64_t, Reader, Result)

#define datareader_ReadU8(Reader, Result)                                      \
    (!datareader_PeekU8(Reader, Result) ? false                                \
                                        : ((Reader)->Position += 1, true))
#define datareader_ReadU16(Reader, Result)                                     \
    (!datareader_PeekU16(Reader, Result) ? false                               \
                                         : ((Reader)->Position += 2, true))
#define datareader_ReadU32(Reader, Result)                                     \
    (!datareader_PeekU32(Reader, Result) ? false                               \
                                         : ((Reader)->Position += 4, true))
#define datareader_ReadU64(Reader, Result)                                     \
    (!datareader_PeekU64(Reader, Result) ? false                               \
                                         : ((Reader)->Position += 8, true))

#define datareader_ReadI8(Reader, Result)                                      \
    (!datareader_PeekI8(Reader, Result) ? false                                \
                                        : ((Reader)->Position += 1, true))
#define datareader_ReadI16(Reader, Result)                                     \
    (!datareader_PeekI16(Reader, Result) ? false                               \
                                         : ((Reader)->Position += 2, true))
#define datareader_ReadI32(Reader, Result)                                     \
    (!datareader_PeekI32(Reader, Result) ? false                               \
                                         : ((Reader)->Position += 4, true))
#define datareader_ReadI64(Reader, Result)                                     \
    (!datareader_PeekI64(Reader, Result) ? false                               \
                                         : ((Reader)->Position += 8, true))

#define datareader_ReadRaw(Reader, Count, Buffer)                              \
    (datareader_Remaining(Reader) < (Count)                                    \
             ? false                                                           \
             : (memcpy(Buffer, datareader_RawData(Reader), (Count)),           \
                (Reader)->Position += (Count),                                 \
                true))
#define datareader_ReadString(Reader, MaxLength, TextLength, Text)             \
    ((!datareader_ReadU16(Reader, TextLength) ||                               \
      datareader_Remaining(Reader) < *(TextLength) ||                          \
      *(TextLength) > ((MaxLength)-1))                                         \
             ? false                                                           \
             : (Text[*(TextLength)] = '\0',                                    \
                datareader_ReadRaw(Reader, *(TextLength), Text)))

#define datareader_Tell(Reader)                                                \
    (ASSERT((Reader)->Length >= (Reader)->Position), (Reader)->Position)
#define datareader_Seek(Reader, To)                                            \
    ((Reader)->Length < (To) ? false : ((Reader)->Position = (To), true))

#define datareader_RawData(Reader)                                             \
    (ASSERT((Reader)->Length >= (Reader)->Position),                           \
     (&(Reader)->Data[(Reader)->Position]))
#define datareader_Slice(Reader, Count, Out)                                   \
    (datareader_Remaining(Reader) < (Count)                                    \
             ? false                                                           \
             : ((Out)->Data = datareader_RawData(Reader),                      \
                (Out)->Length = (Count),                                       \
                (Out)->Position = 0,                                           \
                ((Reader)->Position += (Count), true)))

#define datareader_Skip(Reader, Count)                                         \
    (datareader_Remaining(Reader) < (Count)                                    \
             ? false                                                           \
             : ((Reader)->Position += (Count), true))

bool datareader_ReadFloat(struct trc_data_reader *reader, double *result);
bool datareader_SkipString(struct trc_data_reader *reader);

#endif /* __TRC_DATAREADER_H__ */
