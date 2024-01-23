#ifndef PLAYER_PLAYBACK_H
#define PLAYER_PLAYBACK_H

#include <stdint.h>

#include "memoryfile.h"
#include "datareader.h"
#include "versions.h"
#include "recordings.h"
#include "gamestate.h"

struct playback {
    struct memory_file File;
    struct trc_data_reader Reader;
    struct trc_version *Version;
    struct trc_recording *Recording;
    struct trc_game_state *Gamestate;

    uint32_t PlaybackStart;
    bool PlaybackPaused;
    uint32_t PlaybackPauseTick;
};

bool playback_Init(struct playback *playback,
                   const char *recordingFilename,
                   const char *picFilename,
                   const char *sprFilename,
                   const char *datFilename);
void playback_Free(struct playback *playback);

uint32_t playback_GetPlaybackTick(const struct playback *playback);
void playback_ProcessPackets(struct playback *playback);
void playback_TogglePlayback(struct playback *playback);

#endif // PLAYER_PLAYBACK_H
