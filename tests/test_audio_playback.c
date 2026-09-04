#include<stdio.h>
#include<string.h>
#include<stdint.h>
#include<math.h>
#include<unistd.h>

#define SAMPLE_RATE 48000
#define CHANNELS    2
#define FRAMES      1024
#define DURATION    10
#define FREQUENCY   1000

#include"../hal/include/audio_hal.h"
#include"../hal/include/audio_types.h"

    int main()
    {
        audio_stream_t stream;
        audio_hal_init();
        if(audio_hal_configure(&stream,48000,2, AUDIO_FORMAT_S16_LE,AUDIO_PLAYBACK)!=0)
        {
            printf("Failed to configure audio device\n");
            return 0;
        }
        if(audio_hal_open(&stream)!=0)
        {
            printf("Failed to open audio device\n");
            return 0;
        }
        if(audio_hal_start(&stream)!=0)
        {
            printf("Failed to start audio device\n");
            return 0;
        }


        int16_t buffer[FRAMES * CHANNELS];

        int16_t samples[]={0,10000,20000,30000,20000,10000,0,-10000,-20000,-30000,-20000,-10000};

       int total_frames = SAMPLE_RATE * DURATION;

    for (int frame = 0; frame < total_frames; frame += FRAMES)
    {
        int frames_to_write = total_frames - frame;

        if (frames_to_write > FRAMES)
        {
            frames_to_write = FRAMES;
        }   
        
        /* Generate 1 kHz sine wave */
        for (int i = 0; i < frames_to_write; i++)
        {

            int16_t sample = samples[i % (sizeof(samples) / sizeof(samples[0]))];

            /* Left channel */
            buffer[i * 2] = sample;

            /* Right channel */
            buffer[i * 2 + 1] = sample;
        }

        if (audio_hal_write(&stream, buffer, frames_to_write) != 0)
        {
            printf("Failed to write audio\n");
            break;
        }

        printf("Wrote %d frames\n", frames_to_write);
    }
    
        if(audio_hal_stop(&stream)!=0)
        {
            printf("Failed to stop audio device\n");
            return 0;
        }
        if(audio_hal_close(&stream)!=0)
        {
            printf("Failed to write audio data\n");
            return 0;
        }
        if(audio_hal_deinit()!=0)
        {
            printf("Failed to deinitialize audio device\n");
            return 0;
        }
        printf("SUCCESSFULLY PCM CYCLE TESTED\n");
        return 0;
    }