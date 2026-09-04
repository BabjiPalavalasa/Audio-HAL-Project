#ifndef AUDIO_HAL_H
#define AUDIO_HAL_H
#include "audio_types.h"

/* ============================================================
 * HAL lifecycle
 * ============================================================ */

int audio_hal_init(void);
void audio_hal_deinit(void);


/* ============================================================
 * 1. List sound cards
 * ============================================================ */

int audio_hal_list_cards(void);


/* ============================================================
 * 2. Select sound card
 *
 * Application provides the ALSA card ID.
 * HAL internally finds and stores the card number.
 * ============================================================ */

int audio_hal_select_card(const char *card_id);


/* ============================================================
 * 3. List ALL PCM devices
 *
 * HAL prints:
 *
 *   Playback devices
 *   Capture devices
 *
 * separately.
 * ============================================================ */

int audio_hal_list_pcm_devices(void);


/* ============================================================
 * 4. Select PCM device
 * ============================================================ */

int audio_hal_select_pcm_device(
        audio_direction_t direction,
        int device);


/* ============================================================
 * 5. Play audio
 *
 * HAL internally performs:
 *
 *   - WAV parsing
 *   - PCM configuration
 *   - headphone jack detection
 *   - speaker/headphone routing
 *   - codec mixer configuration
 *   - PCM open
 *   - PCM start
 *   - PCM write
 *   - PCM stop/close
 * ============================================================ */

int audio_hal_play(const char *filename);


/* ============================================================
 * 6. Capture audio
 *
 * HAL internally performs:
 *
 *   - microphone detection
 *   - input routing
 *   - codec mixer configuration
 *   - PCM configuration
 *   - PCM open
 *   - PCM start
 *   - PCM read
 *   - output file creation
 *   - PCM stop/close
 * ============================================================ */

int audio_hal_capture(const char *filename);


/* ============================================================
 * 7. Stop playback
 * ============================================================ */

int audio_hal_stop_playback(void);


/* ============================================================
 * 8. Stop capture
 * ============================================================ */

int audio_hal_stop_capture(void);


/* ============================================================
 * 9. Pause / Resume playback
 *
 * One API toggles:
 *
 *     RUNNING -> PAUSED
 *     PAUSED  -> RUNNING
 *
 * ============================================================ */

int audio_hal_pause_resume(void);


/* ============================================================
 * 10. Volume
 * ============================================================ */

int audio_hal_volume_up(void);
int audio_hal_volume_down(void);


/* ============================================================
 * 11. Mute / Unmute
 * ============================================================ */

int audio_hal_mute(void);
int audio_hal_unmute(void);


/* ============================================================
 * 12. Output route
 *
 * User can explicitly select:
 *
 *     AUDIO_ROUTE_SPEAKER
 *     AUDIO_ROUTE_HEADPHONE
 *
 * ============================================================ */

int audio_hal_select_output_route(
        audio_route_t route);


/* ============================================================
 * 13. Input route
 *
 * User can explicitly select:
 *
 *     AUDIO_ROUTE_MIC
 *     AUDIO_ROUTE_LINE_IN
 *
 * ============================================================ */

int audio_hal_select_input_route(
        audio_route_t route);


/* ============================================================
 * 14. Exit
 * ============================================================ */

int audio_hal_exit(void);

#endif
