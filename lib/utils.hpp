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

#ifndef __TRC_COMMON_HPP__
#define __TRC_COMMON_HPP__

#include <exception>
#include <stacktrace>
#include <type_traits>

namespace trc {
template <typename T, typename A, typename B>
inline bool CheckRange(T value, A min, B max) {
    return value >= static_cast<T>(static_cast<std::remove_cv_t<A>>(min)) &&
           value <= static_cast<T>(static_cast<std::remove_cv_t<B>>(max));
}

inline void AbortUnless(bool assertion) {
    if (!assertion) {
        std::terminate();
    }
}

inline void Assert([[maybe_unused]] bool assertion) {
#ifndef NDEBUG
    AbortUnless(assertion);
#endif
}

/* Generic exception for _recoverable_ errors, the intent is that any
 * function that parses user-provided data throws this exception _before_
 * making any irreversible changes to the state.
 *
 * Irrecoverable errors (e.g. broken invariants) should instead use
 * std::terminate() */
struct ErrorBase {
    std::stacktrace Stacktrace;

    ErrorBase() : Stacktrace(std::stacktrace::current()) {
    }

    virtual std::string Description() const {
        return "general";
    }
};

struct InvalidDataError : public ErrorBase {
    InvalidDataError() : ErrorBase() {
    }

    virtual std::string Description() const {
        return "invalid data";
    }
};

struct IOError : public ErrorBase {
    IOError() : ErrorBase() {
    }

    virtual std::string Description() const {
        return "IO failure";
    }
};

struct NotSupportedError : public ErrorBase {
    NotSupportedError() : ErrorBase() {
    }

    virtual std::string Description() const {
        return "unsupported operation";
    }
};
} // namespace trc

#endif /* __TRC_COMMON_HPP__ */
