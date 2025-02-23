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

#ifndef __TRC_MESSAGE_HPP__
#define __TRC_MESSAGE_HPP__

#include <algorithm>
#include <cstdint>
#include <utility>
#include <string>
#include <list>

#include "position.hpp"

static constexpr uint32_t MESSAGE_DISPLAY_TIME = 3000;

namespace trc {
/* Priority-ordered canonical message modes, lower values have higher display
 * precedence. */

enum class MessageMode {
    PrivateIn = 0,
    PrivateOut,

    Say,
    Whisper,
    Yell,

    ChannelWhite,
    ChannelYellow,
    ChannelOrange,
    ChannelRed,
    ChannelAnonymousRed,

    ConsoleBlue,
    ConsoleOrange,
    ConsoleRed,

    Spell,

    NPCStart,
    NPCContinued,
    PlayerToNPC,

    Broadcast,

    GMToPlayer,
    PlayerToGM,

    Login,
    Admin,
    Game,
    Failure,

    Look,

    DamageDealt,

    DamageReceived,
    Healing,
    Experience,

    DamageReceivedOthers,
    HealingOthers,
    ExperienceOthers,

    Status,
    Loot,
    NPCTrade,
    Guild,

    PartyWhite,

    Party,

    MonsterSay,
    MonsterYell,

    Report,
    Hotkey,

    Tutorial,
    ThankYou,
    Market,

    Mana,

    Warning,

    RuleViolationChannel,
    RuleViolationAnswer,
    RuleViolationContinue,
};

class MessageList;
struct Message {
    friend MessageList;

    MessageMode Type;
    trc::Position Position;
    std::string Author;
    std::string Text;

    Message(MessageMode type,
            const trc::Position &position,
            const std::string &author,
            const std::string &text,
            uint32_t start,
            uint32_t end)
        : Type(type),
          Position(position),
          Author(author),
          Text(text),
          StartTick(start),
          EndTick(end) {
    }

    Message() {
    }

private:
    uint32_t StartTick;
    uint32_t EndTick;
};

class MessageList {
    std::list<Message> Messages;

    static std::strong_ordering CompareTypes(MessageMode messageType,
                                             MessageMode compareType);
    static std::strong_ordering SortFunction(MessageMode type,
                                             const Position &position,
                                             const std::string &author,
                                             const Message &compareTo);

public:
    using Iterator = std::list<Message>::const_iterator;

    MessageList() {
    }

    void AddMessage(MessageMode type,
                    const trc::Position &position,
                    const std::string &author,
                    const std::string &text,
                    uint32_t tick);

    void Prune(uint32_t tick) {
        /* FIXME: This is naive until the C++ migration is complete,
         * performance is not important at the moment. */
        std::erase_if(Messages, [tick](const auto &message) {
            return message.EndTick < tick;
        });
    }

    void Clear() {
        Messages.clear();
    }

    Iterator begin() const {
        return Messages.cbegin();
    }

    Iterator end() const {
        return Messages.cend();
    }

    std::pair<bool, bool> QueryNext(Iterator current);
};

} // namespace trc

#endif /* __TRC_MESSAGE_HPP__ */
