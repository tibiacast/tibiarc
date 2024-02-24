#ifndef PLAYER_PLAYBACK_H
#define PLAYER_PLAYBACK_H

#include <stdint.h>

#include "memoryfile.h"
#include "datareader.h"
#include "versions.h"
#include "recordings.h"
#include "gamestate.h"

struct playback {
    struct trc_version *Version;
    struct trc_recording *Recording;
    struct trc_game_state *Gamestate;

    uint32_t PlaybackStart;
    bool PlaybackPaused;
    uint32_t PlaybackPauseTick;
};

// Note: Data in the pic, spr and dat data_readers are consumed
//       in this call, but Data in the recording must still be
//       available until after playback_Free has been called
bool playback_Init(struct playback *playback,
                   struct trc_data_reader *recording,
                   struct trc_data_reader *pic,
                   struct trc_data_reader *spr,
                   struct trc_data_reader *dat);
void playback_Free(struct playback *playback);

uint32_t playback_GetPlaybackTick(const struct playback *playback);
void playback_ProcessPackets(struct playback *playback);
void playback_TogglePlayback(struct playback *playback);

#endif // PLAYER_PLAYBACK_H
