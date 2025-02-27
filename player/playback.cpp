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

extern "C" {
#include <SDL.h>
}

namespace trc {

Playback::Playback(const DataReader &file,
                   const std::string &name,
                   const DataReader &pic,
                   const DataReader &spr,
                   const DataReader &dat,
                   int major,
                   int minor,
                   int preview)
    : Scale(1.0), ScaleTick(SDL_GetTicks()), BaseTick(0) {
    auto format = Recordings::GuessFormat(name, file);

    if ((major | minor | preview) == 0) {
        if (!Recordings::QueryTibiaVersion(format,
                                           file,
                                           major,
                                           minor,
                                           preview)) {
            throw InvalidDataError();
        }
    }

    Version = std::make_unique<trc::Version>(major,
                                             minor,
                                             preview,
                                             pic,
                                             spr,
                                             dat);
    Recording = Recordings::Read(format, file, *Version);
    Gamestate = std::make_unique<trc::Gamestate>(*Version);

    Needle = Recording->Frames.cbegin();
}

uint32_t Playback::GetPlaybackTick() {
    return std::min(static_cast<uint32_t>(BaseTick +
                                          (SDL_GetTicks() - ScaleTick) * Scale),
                    Recording->Runtime);
}

void Playback::ProcessPackets() {
    /* Process packets until we have caught up */
    uint32_t playback_tick = GetPlaybackTick();

    while (Needle != Recording->Frames.cend() &&
           Needle->Timestamp <= playback_tick) {
        for (auto &event : Needle->Events) {
            event->Update(*Gamestate);
        }

        Needle = std::next(Needle);
    }

    /* Advance the gamestate */
    Gamestate->CurrentTick = playback_tick;
}

void Playback::Toggle() {
    if (Scale > 0.0) {
        SetSpeed(0.0);
        puts("Playback paused");
    } else {
        SetSpeed(1.0);
        puts("Playback resumed");
    }
}

void Playback::SetSpeed(float speed) {
    BaseTick = GetPlaybackTick();
    ScaleTick = SDL_GetTicks();
    Scale = speed;

    printf("Speed changed to %f\n", speed);
}

void Playback::Skip(int32_t by) {
    BaseTick = GetPlaybackTick();
    ScaleTick = SDL_GetTicks();

    if (by < 0) {
        Needle = Recording->Frames.cbegin();
        Gamestate->Reset();
        Gamestate->CurrentTick = 0;

        /* Fast-forward until the game state is sufficiently initialized. */
        while (!Gamestate->Creatures.contains(Gamestate->Player.Id) &&
               Needle != Recording->Frames.cend()) {
            for (auto &event : Needle->Events) {
                event->Update(*Gamestate);
            }

            Needle = std::next(Needle);
        }

        if (BaseTick < -by) {
            BaseTick = 0;
        } else {
            BaseTick += by;
        }

        ProcessPackets();
    } else {
        BaseTick = std::min(BaseTick + by, Recording->Runtime);
    }
}

}; // namespace trc
