//defines the data types used by our HAL
#ifndef AUDIO_TYPES_H
#define AUDIO_TYPES_H

typedef enum {
    AUDIO_PLAYBACK,
    AUDIO_CAPTURE
} audio_direction_t;

typedef enum {
    AUDIO_ROUTE_SPEAKER,
    AUDIO_ROUTE_HEADPHONE,
    AUDIO_ROUTE_LINE_OUT,
    AUDIO_ROUTE_MIC,
    AUDIO_ROUTE_LINE_IN
} audio_route_t;

typedef enum {
    AUDIO_FORMAT_S16_LE,
    AUDIO_FORMAT_S24_LE,
    AUDIO_FORMAT_S32_LE
} audio_format_t;

typedef enum {
    AUDIO_STREAM_CLOSED,
    AUDIO_STREAM_OPEN,
    AUDIO_STREAM_CONFIGURED,
    AUDIO_STREAM_RUNNING,
    AUDIO_STREAM_STOPPED,
    AUDIO_STREAM_ERROR
} audio_stream_state_t;

/* Audio stream object */
typedef struct {
    audio_direction_t direction;
    audio_route_t route;
    audio_format_t format;

    unsigned int sample_rate;
    unsigned int channels;

    audio_stream_state_t state;

    struct pcm *pcm ;//Tinyalsa PCM handle
} audio_stream_t;

#endif
