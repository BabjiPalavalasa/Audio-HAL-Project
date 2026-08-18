//defines the functions/apis our HAL provides

#ifndef AUDIO_HAL_H
#define AUDIO_HAL_H

#include "audio_types.h"

/* HAL lifecycle */
int audio_hal_init(void);
int audio_hal_deinit(void);

/* Stream configuration */
int audio_hal_configure(audio_stream_t *stream,
                        unsigned int sample_rate,
                        unsigned int channels,
                        audio_format_t format,audio_direction_t direction);

/* Stream management */
int audio_hal_open(audio_stream_t *stream);
int audio_hal_close(audio_stream_t *stream);

/* Stream control */
int audio_hal_start(audio_stream_t *stream);
int audio_hal_stop(audio_stream_t *stream);

/* PCM data transfer */
int audio_hal_write(audio_stream_t *stream,
                    const void *buffer,
                    unsigned int frames);

int audio_hal_read(audio_stream_t *stream,
                   void *buffer,
                   unsigned int frames);

/* Audio routing */
int audio_hal_set_route(audio_stream_t *stream,
                        audio_route_t route);

#endif
