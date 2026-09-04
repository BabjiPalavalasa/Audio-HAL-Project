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
#define FREQUENCY   440
#define AMPLITUDE   10000

#define TOTAL_FRAMES (SAMPLE_RATE * DURATION)

#include"../hal/include/audio_hal.h"
#include"../hal/include/audio_types.h"

    int main()
    {
        audio_stream_t capture_stream;
        audio_stream_t playback_stream;
        audio_hal_init();
        if(audio_hal_configure(&capture_stream,48000,2, AUDIO_FORMAT_S16_LE,AUDIO_CAPTURE)!=0)
        {
            printf("Failed to configure audio device\n");
            return 0;
        }
        if(audio_hal_configure(&playback_stream,48000,2, AUDIO_FORMAT_S16_LE,AUDIO_PLAYBACK)!=0)
        {
            printf("Failed to configure audio device\n");
            return 0;
        }

        if(audio_hal_open(&capture_stream)!=0)
        {
            printf("Failed to open audio device for capture\n");
            return 0;
        }
        if(audio_hal_open(&playback_stream)!=0)
        {
            printf("Failed to open audio device for playback\n");
            return 0;
        }

        if(audio_hal_start(&capture_stream)!=0)
        {
            printf("Failed to start audio device for capture\n");
            return 0;
        }
        if(audio_hal_start(&playback_stream)!=0)
        {
            printf("Failed to start audio device for playback\n");
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

                     if (audio_hal_read(&capture_stream, &buffer[i * CHANNELS],frames_to_read) != 0)
                   {
                        printf("Failed to read audio\n");
                        break;
                   }  
            }    

          printf("Waiting for 5 second before starting playback\n");
          sleep(5);

       

         for (int i = 0;i < TOTAL_FRAMES;i+=FRAMES)
            {
                int frames_to_write = TOTAL_FRAMES - i;

                if (frames_to_write > FRAMES)
                {
                    frames_to_write = FRAMES;
                }
                
                     if (audio_hal_write(&playback_stream, &buffer[i * CHANNELS],frames_to_write) != 0)
                   {
                        printf("Failed to write audio\n");
                        break;
                   }  
            }    
       

        if(audio_hal_stop(&playback_stream)!=0)
        {
            printf("Failed to stop audio device\n");
            return 0;
        }
        if(audio_hal_stop(&capture_stream)!=0)
        {
            printf("Failed to stop audio device\n");
            return 0;
        }

        if(audio_hal_close(&playback_stream)!=0)
        {
            printf("Failed to write audio data\n");
            return 0;
        }
        if(audio_hal_close(&capture_stream)!=0)
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