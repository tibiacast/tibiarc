#include "playback.h"

#include <SDL.h>

static bool openMemoryFile(const char *filename,
                           struct memory_file *file,
                           struct trc_data_reader *reader) {
    if (!memoryfile_Open(filename, file)) {
        fprintf(stderr, "Could not open file: %s\n", filename);
        return false;
    }
    reader->Position = 0;
    reader->Data = file->View;
    reader->Length = file->Size;
    return true;
}

bool playback_Init(struct playback *playback,
                   struct trc_data_reader *recording,
                   struct trc_data_reader *pic,
                   struct trc_data_reader *spr,
                   struct trc_data_reader *dat) {

    if (!version_Load(7, 41, 0, pic, spr, dat, &playback->Version)) {
        fprintf(stderr, "Could not load files\n");
        return false;
    }

    playback->Recording = recording_Create(RECORDING_FORMAT_TRP);

    if (!recording_Open(playback->Recording,
                        recording,
                        playback->Version)) {
        fprintf(stderr, "Could not load recording\n");
        return false;
    }

    // Create gamestate
    playback->Gamestate = gamestate_Create(playback->Version);

    // Initialize the playback by processing the first packet (assumes only
    // first packet has time = 0)
    playback->PlaybackStart = SDL_GetTicks();
    playback->PlaybackPaused = false;
    playback->Gamestate->CurrentTick = playback_GetPlaybackTick(playback);
    if (!recording_ProcessNextPacket(playback->Recording,
                                     playback->Gamestate)) {
        fprintf(stderr, "Could not process packet\n");
        return false;
    }

    return true;
}

void playback_Free(struct playback *playback) {
    recording_Free(playback->Recording);
    playback->Recording = NULL;

    version_Free(playback->Version);
    playback->Version = NULL;

    gamestate_Free(playback->Gamestate);
    playback->Gamestate = NULL;
}

uint32_t playback_GetPlaybackTick(const struct playback *playback) {
    if (playback->PlaybackPaused) {
        return playback->PlaybackPauseTick;
    }
    return SDL_GetTicks() - playback->PlaybackStart;
}

void playback_ProcessPackets(struct playback *playback) {
    // Process packets until we have caught up
    uint32_t playback_tick = playback_GetPlaybackTick(playback);
    while (playback->Recording->NextPacketTimestamp <= playback_tick) {
        if (!recording_ProcessNextPacket(playback->Recording,
                                         playback->Gamestate)) {
            fprintf(stderr, "Could not process packet\n");
            abort();
        }
    }

    // Advance the gamestate
    playback->Gamestate->CurrentTick = playback_tick;
}

void playback_TogglePlayback(struct playback *playback) {
    if (!playback->PlaybackPaused) {
        playback->PlaybackPauseTick = playback_GetPlaybackTick(playback);
        puts("Playback paused");
    } else {
        playback->PlaybackStart =
                (SDL_GetTicks() - playback->PlaybackPauseTick);
        puts("Playback resumed");
    }
    playback->PlaybackPaused = !playback->PlaybackPaused;
}
