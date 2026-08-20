#include<stdio.h>
#include<string.h>
#include<stdint.h>
#include<math.h>
#include<unistd.h>
#include<stdlib.h>

#define SAMPLE_RATE 48000
#define CHANNELS    2
#define FRAMES      1024
#define DURATION    10

#define TOTAL_FRAMES (SAMPLE_RATE * DURATION)

#include"../hal/include/audio_hal.h"
#include"../hal/include/audio_types.h"

    int main()
    {
        audio_stream_t stream;
        audio_hal_init();
        if(audio_hal_configure(&stream,48000,2, AUDIO_FORMAT_S16_LE,AUDIO_CAPTURE)!=0)
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
        if(audio_hal_set_route(&stream,AUDIO_ROUTE_SPEAKER)!=0)
        {
            printf("Failed to route audio data\n");
            return 0;
        }


        int16_t *buffer;

         buffer = (int16_t *)malloc(TOTAL_FRAMES * CHANNELS * sizeof(int16_t));
            if (buffer == NULL)
            {
                printf("Failed to allocate memory for audio buffer\n");
                return 0;
            }

       for (int i = 0;i < TOTAL_FRAMES;i+=FRAMES)
            {
                int frames_to_read = TOTAL_FRAMES - i;
                if (frames_to_read > FRAMES)
                {
                    frames_to_read = FRAMES;
                }

                if (audio_hal_read(&stream, &buffer[i * CHANNELS],frames_to_read) != 0)
                {
                        printf("Failed to read audio\n");
                        break;
                }  
            }    

        if(audio_hal_stop(&stream)!=0)
        {
            printf("Failed to stop audio device\n");
            return 0;
        }
        if(audio_hal_close(&stream)!=0)
        {
            printf("Failed to close audio device\n");
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