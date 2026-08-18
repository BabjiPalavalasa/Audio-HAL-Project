#include <stdio.h>
#include <string.h>
#include <tinyalsa/pcm.h>

#include "../include/audio_hal.h"

int audio_hal_init(void)
{
    printf("HAL: initialized\n");
    return 0;
}

int audio_hal_deinit(void)
{
    printf("HAL: deinitialized\n");
    return 0;
}

static enum pcm_format audio_format_to_pcm_format(audio_format_t format)
{
    switch (format)
    {
    case AUDIO_FORMAT_S16_LE:
        return PCM_FORMAT_S16_LE;
    case AUDIO_FORMAT_S24_LE:
        return PCM_FORMAT_S24_LE;
    case AUDIO_FORMAT_S32_LE:
        return PCM_FORMAT_S32_LE;
    default:
        return PCM_FORMAT_INVALID;
    }
}

int audio_hal_open(audio_stream_t *stream)
{
    struct pcm_config config;

    if (stream->state != AUDIO_STREAM_CONFIGURED)
        return -1;

    if (stream == NULL)
        return -1;

    memset(&config, 0, sizeof(config));

    config.channels = stream->channels;
    config.rate = stream->sample_rate;
    config.format = audio_format_to_pcm_format(stream->format);
    config.period_size = 1024;
    config.period_count = 4;
    unsigned int flags = (stream->direction == AUDIO_PLAYBACK) ? PCM_OUT : PCM_IN;

    stream->pcm = pcm_open(0, 0, flags, &config);

    if (stream->pcm == NULL)
    {
        printf("HAL: pcm_open returned NULL\n");
        return -1;
    }

    if (!pcm_is_ready(stream->pcm))
    {
        printf("HAL: PCM not ready: %s\n",
               pcm_get_error(stream->pcm));
               

        pcm_close(stream->pcm);
        stream->pcm = NULL;
        return -1;
    }

    stream->state = AUDIO_STREAM_OPEN;

    printf("HAL: TinyALSA PCM opened successfully\n");

    return 0;
}


int audio_hal_configure(audio_stream_t *stream,
                        unsigned int sample_rate,
                        unsigned int channels,
                        audio_format_t format,
                        audio_direction_t direction)
{
    if (stream == NULL)
        return -1;

    stream->sample_rate = sample_rate;
    stream->channels = channels;
    stream->format = format;
    stream->direction = direction;

    stream->state = AUDIO_STREAM_CONFIGURED;

    printf("HAL: stream configured\n");
    printf("HAL: sample rate = %u\n", sample_rate);
    printf("HAL: channels = %u\n", channels);

    return 0;
}

int audio_hal_start(audio_stream_t *stream)
{
    if (stream == NULL)
        return -1;

    if (stream->state != AUDIO_STREAM_OPEN)
        return -1;

    stream->state = AUDIO_STREAM_RUNNING;

    printf("HAL: stream started\n");

    return 0;
}


unsigned int pcm_format_to_bits(enum pcm_format format)
{
    switch (format)
    {
    case PCM_FORMAT_S16_LE:
        return 16;
    case PCM_FORMAT_S24_LE:
        return 24;
    case PCM_FORMAT_S32_LE:
        return 32;
    default:
        return 0;
    }
}

int audio_hal_write(audio_stream_t *stream,
                    const void *buffer,
                    unsigned int frames)
{
    if (stream == NULL || buffer == NULL)
        return -1;

    if (stream->state != AUDIO_STREAM_RUNNING)
        return -1;

    if (stream->direction != AUDIO_PLAYBACK)
          return -1;

        if(stream->pcm == NULL)
        return -1;

    if(frames == 0)
        return -1;

    unsigned int bytes_per_frame = stream->channels * (pcm_format_to_bits(audio_format_to_pcm_format(stream->format)) / 8);

    if((pcm_write(stream->pcm, buffer, frames*bytes_per_frame) < 0))
    {
        printf("HAL: pcm_write failed: %s\n", pcm_get_error(stream->pcm));
        return -1;
    }

    printf("HAL: writing %u frames\n", frames);

    return 0;
}

int audio_hal_read(audio_stream_t *stream,
                   void *buffer,
                   unsigned int frames)
{
    if (stream == NULL || buffer == NULL)
        return -1;

    if (stream->state != AUDIO_STREAM_RUNNING)
        return -1;

    if (stream->direction != AUDIO_CAPTURE)
        return -1;

    if(frames == 0)
        return -1;

    if(stream->pcm == NULL)
        return -1;


    unsigned int bytes_per_frame = stream->channels * (pcm_format_to_bits(audio_format_to_pcm_format(stream->format)) / 8);

    int  ret=pcm_read(stream->pcm, buffer, frames*bytes_per_frame);

    if(ret < 0)
    {
        printf("HAL: pcm_read returned %d\n", ret);
        printf("HAL: pcm_read failed: %s", pcm_get_error(stream->pcm));
        return -1;
    }


    printf("HAL: reading %u frames\n", frames);

    return 0;
}

int audio_hal_set_route(audio_stream_t *stream,
                        audio_route_t route)
{
    if (stream == NULL)
        return -1;

    if (stream->state != AUDIO_STREAM_RUNNING)
        return -1;

    stream->route = route;

    printf("HAL: route changed to %d\n", route);

    return 0;
}

int audio_hal_stop(audio_stream_t *stream)
{
    if (stream == NULL)
        return -1;

    if (stream->state != AUDIO_STREAM_RUNNING)
        return -1;

    stream->state = AUDIO_STREAM_STOPPED;

    printf("HAL: stream stopped\n");

    return 0;
}

int audio_hal_close(audio_stream_t *stream)
{
    if (stream == NULL)
        return -1;

    if (stream->state != AUDIO_STREAM_STOPPED)
        return -1;

    stream->state = AUDIO_STREAM_CLOSED;

    printf("HAL: stream closed\n");

    return 0;
}
