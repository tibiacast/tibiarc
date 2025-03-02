/*
 * Copyright 2024 "Simon Sandström"
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

#ifndef PLAYER_PLAYBACK_H
#define PLAYER_PLAYBACK_H

#include "memoryfile.hpp"
#include "datareader.hpp"
#include "versions.hpp"
#include "recordings.hpp"
#include "gamestate.hpp"

#include <cstdint>
#include <filesystem>

namespace trc {
class Playback {
    void Stabilize();

public:
    std::unique_ptr<const trc::Version> Version;
    std::unique_ptr<trc::Gamestate> Gamestate;
    std::unique_ptr<Recordings::Recording> Recording;

    std::list<Recordings::Recording::Frame>::const_iterator Needle;
    uint32_t BaseTick;
    uint32_t ScaleTick;
    float Scale;

    Playback(const DataReader &file,
             const std::filesystem::path &name,
             const DataReader &pic,
             const DataReader &spr,
             const DataReader &dat,
             int major = 0,
             int minor = 0,
             int preview = 0);

    uint32_t GetPlaybackTick();
    void ProcessPackets();
    void Toggle();
    void SetSpeed(float speed);
    void Skip(int32_t by);
};
}; // namespace trc

#endif /* PLAYER_PLAYBACK_H */
