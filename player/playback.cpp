/*
 * Copyright 2024 "Simon Sandström"
 * Copyright 2024-2025 "John Högberg"
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

#include "playback.hpp"

#include <iostream>

extern "C" {
#include <SDL.h>
}

namespace trc {

Playback::Playback(const DataReader &file,
                   const std::filesystem::path &name,
                   const DataReader &pic,
                   const DataReader &spr,
                   const DataReader &dat,
                   int major,
                   int minor,
                   int preview)
    : Scale(1.0), ScaleTick(SDL_GetTicks()), BaseTick(0) {
    auto format = Recordings::GuessFormat(name, file);
    VersionTriplet desiredVersion(major, minor, preview);

    if (desiredVersion == VersionTriplet()) {
        if (!Recordings::QueryTibiaVersion(format, file, desiredVersion)) {
            throw InvalidDataError();
        }
    }

    Version = std::make_unique<trc::Version>(desiredVersion, pic, spr, dat);

    auto [recording, partial] = Recordings::Read(format, file, *Version);

    if (partial) {
        throw InvalidDataError();
    }

    Recording = std::move(recording);
    Gamestate = std::make_unique<trc::Gamestate>(*Version);

    Needle = Recording->Frames.cbegin();
    Stabilize();
}

void Playback::Stabilize() {
    /* Fast-forward until the game state is sufficiently initialized. */
    while (!Gamestate->Creatures.contains(Gamestate->Player.Id) &&
           Needle != Recording->Frames.cend()) {
        for (auto &event : Needle->Events) {
            event->Update(*Gamestate);
        }

        Needle = std::next(Needle);
    }
}

std::chrono::milliseconds Playback::GetPlaybackTick() {
    auto scaledTick =
            BaseTick + std::chrono::milliseconds(static_cast<int64_t>(
                               (SDL_GetTicks() - ScaleTick.count()) * Scale));
    return std::min(scaledTick, Recording->Runtime);
}
void Playback::ProcessPackets() {
    /* Process packets until we have caught up */
    auto tick = GetPlaybackTick();

    while (Needle != Recording->Frames.cend() && Needle->Timestamp <= tick) {
        for (auto &event : Needle->Events) {
            event->Update(*Gamestate);
        }

        Needle = std::next(Needle);
    }

    /* Advance the gamestate */
    Gamestate->CurrentTick = tick.count();
}

void Playback::Toggle() {
    if (Scale > 0.0) {
        std::cout << "Playback paused" << std::endl;
    } else {
        SetSpeed(1.0);
        std::cout << "Playback resumed" << std::endl;
    }
}

void Playback::SetSpeed(float speed) {
    ScaleTick = std::chrono::milliseconds(SDL_GetTicks());
    BaseTick = GetPlaybackTick();
    Scale = speed;

    std::cout << "Speed changed to " << speed << std::endl;
}

void Playback::Skip(std::chrono::milliseconds by) {
    ScaleTick = std::chrono::milliseconds(SDL_GetTicks());
    BaseTick = GetPlaybackTick();

    if (by < std::chrono::milliseconds::zero()) {
        Needle = Recording->Frames.cbegin();
        Gamestate->Reset();
        Gamestate->CurrentTick = 0;

        Stabilize();

        if (BaseTick < -by) {
            BaseTick = std::chrono::milliseconds::zero();
        } else {
            BaseTick += by;
        }

        ProcessPackets();
    } else {
        BaseTick = std::min(BaseTick + by, Recording->Runtime);
    }
}

}; // namespace trc
