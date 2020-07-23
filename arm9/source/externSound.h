#pragma once
#ifndef __PICOTWL_SOUND__
#define __PICOTWL_SOUND__
#include <nds.h>
#include <mm_types.h>
#include <maxmod9.h>
#include "singleton.h"
#include <cstdio>

/*
 * Handles playing sound effects and the streaming background music control.
 * See streamingaudio.c for a technical overview of how streaming works.
 */
class SoundControl {
    public:
        SoundControl();

        // Refill the stream buffers
        volatile void twl_updateStream();
        volatile void updateStream();

        void twl_loadStream(const char* filenameStart, const char* filename);
        void loadStream(const char* filenameStart, const char* filename);
        void beginStream();
        void stopStream();
        void closeStream();
        void twl_resetStream();
        void resetStream();
        void fadeOutStream();
        void cancelFadeOutStream();

        // Sets the number of samples of silence to
        // stream before continuing.
        void setStreamDelay(u32 stream_delay);
        
        u32 getStartupSoundLength() { return startup_sample_length; }
      
    private:
        mm_stream stream;
        mm_ds_system sys;
        bool stream_is_playing;
        FILE* stream_start_source;
        FILE* stream_source;
        u32 startup_sample_length;
};

typedef singleton<SoundControl> soundCtl_s;
inline SoundControl &snd() { return soundCtl_s::instance(); }

extern bool MusicPlayRAM(void);
extern void SoundPlayRAM(void);
extern void SoundPlayZ80(void);

#endif