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

#include "message.hpp"

#include <algorithm>
#include <compare>
#include <cstring>

#include "utils.hpp"

namespace trc {

std::strong_ordering MessageList::CompareTypes(MessageMode messageType,
                                               MessageMode compareType) {
    switch (messageType) {
    case MessageMode::Say:
    case MessageMode::Whisper:
    case MessageMode::Yell:
    case MessageMode::Spell:
        messageType = MessageMode::Say;
    default:
        break;
    }

    switch (compareType) {
    case MessageMode::Say:
    case MessageMode::Whisper:
    case MessageMode::Yell:
    case MessageMode::Spell:
        compareType = MessageMode::Say;
    default:
        break;
    }

    if (messageType < compareType) {
        return std::strong_ordering::greater;
    } else if (messageType > compareType) {
        return std::strong_ordering::less;
    }

    return std::strong_ordering::equal;
}

std::strong_ordering MessageList::SortFunction(MessageMode type,
                                               const Position &position,
                                               const std::string &author,
                                               const Message &compareTo) {
    auto typeCompare = CompareTypes(type, compareTo.Type);

    if (typeCompare != std::strong_ordering::equal) {
        return typeCompare;
    }

    if (position.X < compareTo.Position.X) {
        return std::strong_ordering::less;
    } else if (position.X > compareTo.Position.X) {
        return std::strong_ordering::greater;
    }

    if (position.Y < compareTo.Position.Y) {
        return std::strong_ordering::less;
    } else if (position.Y > compareTo.Position.Y) {
        return std::strong_ordering::greater;
    }

    if (position.Z < compareTo.Position.Z) {
        return std::strong_ordering::less;
    } else if (position.Z > compareTo.Position.Z) {
        return std::strong_ordering::greater;
    }

    return author <=> compareTo.Author;
}

std::pair<bool, bool> MessageList::QueryNext(MessageList::Iterator current) {
    bool preserveCoordinates = false, canMerge = false;

    AbortUnless(current != end());
    auto next = std::next(current);

    if (next != end()) {
        if (current->Position.X == next->Position.X &&
            current->Position.Y == next->Position.Y &&
            current->Position.Z == next->Position.Z) {
            preserveCoordinates = (CompareTypes(current->Type, next->Type) ==
                                   std::strong_ordering::equal);

            canMerge = preserveCoordinates &&
                       (current->Type != MessageMode::PrivateIn) &&
                       (current->Author == next->Author);
        }
    }

    return std::make_pair(preserveCoordinates, canMerge);
}

void MessageList::AddMessage(MessageMode type,
                             const Position &position,
                             const std::string &author,
                             const std::string &text,
                             uint32_t tick) {
    auto insert_before = std::find_if(begin(), end(), [=](const auto &element) {
        return SortFunction(type, position, author, element) !=
               std::strong_ordering::less;
    });

    if (type == MessageMode::PrivateIn && insert_before != end() &&
        insert_before->Type == MessageMode::PrivateIn) {
        /* Private messages have their display time bumped longer than the
         * ordinary client so that messages sent in quick succession can be
         * seen in full despite lacking a browsable chat channel. */
        tick = std::max(tick, insert_before->EndTick);
    }

    Messages.emplace(insert_before,
                     type,
                     position,
                     author,
                     text,
                     tick + MESSAGE_DISPLAY_TIME);
}

} // namespace trc
