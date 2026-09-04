#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "../hal/include/audio_hal.h"
#include "../hal/include/audio_types.h"



/* ============================================================
 * MENU
 * ============================================================ */

static void print_menu(void)
{
    printf("\n");
    printf("==================================================\n");
    printf("              AUDIO HAL TEST APPLICATION\n");
    printf("==================================================\n");

    printf(" 1. List Sound Cards\n");
    printf(" 2. Select Sound Card\n");
    printf(" 3. List PCM Devices\n");
    printf(" 4. Select PCM Device\n");
    printf(" 5. Play WAV File\n");
    printf(" 6. Capture Audio\n");
    printf(" 7. Stop Playback\n");
    printf(" 8. Stop Capture\n");
    printf(" 9. Pause / Resume Playback\n");
    printf("10. Volume Up\n");
    printf("11. Volume Down\n");
    printf("12. Mute\n");
    printf("13. Unmute\n");
    printf("14. Select Output Route\n");
    printf("15. Select Input Route\n");
    printf(" 0. Exit\n");

    printf("==================================================\n");
    printf("Enter choice: ");
}


/* ============================================================
 * SELECT SOUND CARD
 * ============================================================ */

static void select_card(void)
{
    char card_id[128];

    printf("\nEnter ALSA card ID: ");

    if (scanf("%127s", card_id) != 1) {
        printf("Invalid input\n");
        return;
    }

    if (audio_hal_select_card(card_id) < 0) {
        printf("Failed to select card\n");
        return;
    }

    printf("Sound card selected successfully\n");
}


/* ============================================================
 * SELECT PCM DEVICE
 * ============================================================ */

static void select_pcm_device(void)
{
    int direction;
    int device;

    printf("\n");
    printf("1. Playback\n");
    printf("2. Capture\n");
    printf("Select direction: ");

    if (scanf("%d", &direction) != 1) {
        printf("Invalid input\n");
        return;
    }

    printf("Enter PCM device number: ");

    if (scanf("%d", &device) != 1) {
        printf("Invalid input\n");
        return;
    }

    if (direction == 1) {

        if (audio_hal_select_pcm_device(
                    AUDIO_PLAYBACK,
                    device) < 0) {

            printf("Failed to select playback PCM\n");
            return;
        }

        printf("Playback PCM selected\n");
    }
    else if (direction == 2) {

        if (audio_hal_select_pcm_device(
                    AUDIO_CAPTURE,
                    device) < 0) {

            printf("Failed to select capture PCM\n");
            return;
        }

        printf("Capture PCM selected\n");
    }
    else {

        printf("Invalid direction\n");
    }
}


/* ============================================================
 * PLAY WAV
 * ============================================================ */

static void play_wav(void)
{
    char filename[PATH_MAX];

    printf("\nEnter WAV file path: ");

    if (scanf("%4095s", filename) != 1) {
        printf("Invalid input\n");
        return;
    }

    /*
     * IMPORTANT:
     *
     * Application does NOT:
     *
     * - parse WAV
     * - configure PCM
     * - detect jack
     * - configure codec
     * - select speaker/headphone
     *
     * All of that is handled by audio_hal_play()
     */

    if (audio_hal_play(filename) < 0) {

        printf("Failed to start playback\n");
        return;
    }

    printf("Playback started\n");
}


/* ============================================================
 * CAPTURE
 * ============================================================ */

static void capture_audio(void)
{
    char filename[PATH_MAX];

    printf("\nEnter output capture file: ");

    if (scanf("%4095s", filename) != 1) {
        printf("Invalid input\n");
        return;
    }

    /*
     * HAL internally performs:
     *
     * - microphone detection
     * - input routing
     * - codec configuration
     * - PCM configuration
     * - PCM read
     */

    if (audio_hal_capture(filename) < 0) {

        printf("Failed to start capture\n");
        return;
    }

    printf("Capture started\n");
}


/* ============================================================
 * OUTPUT ROUTE
 * ============================================================ */

static void select_output_route(void)
{
    int choice;

    printf("\n");
    printf("1. Speaker\n");
    printf("2. Headphone\n");
    printf("Select output route: ");

    if (scanf("%d", &choice) != 1) {
        printf("Invalid input\n");
        return;
    }

    if (choice == 1) {

        if (audio_hal_select_output_route(
                    AUDIO_ROUTE_SPEAKER) < 0) {

            printf("Failed to select speaker route\n");
            return;
        }

        printf("Output route = SPEAKER\n");
    }
    else if (choice == 2) {

        if (audio_hal_select_output_route(
                    AUDIO_ROUTE_HEADPHONE) < 0) {

            printf("Failed to select headphone route\n");
            return;
        }

        printf("Output route = HEADPHONE\n");
    }
    else {

        printf("Invalid output route\n");
    }
}


/* ============================================================
 * INPUT ROUTE
 * ============================================================ */

static void select_input_route(void)
{
    int choice;

    printf("\n");
    printf("1. Internal Microphone\n");
    printf("2. Inline / Line Microphone\n");
    printf("Select input route: ");

    if (scanf("%d", &choice) != 1) {
        printf("Invalid input\n");
        return;
    }

    if (choice == 1) {

        if (audio_hal_select_input_route(
                    AUDIO_ROUTE_MIC) < 0) {

            printf("Failed to select internal microphone\n");
            return;
        }

        printf("Input route = INTERNAL MIC\n");
    }
    else if (choice == 2) {

        if (audio_hal_select_input_route(
                    AUDIO_ROUTE_LINE_IN) < 0) {

            printf("Failed to select inline microphone\n");
            return;
        }

        printf("Input route = INLINE / LINE MIC\n");
    }
    else {

        printf("Invalid input route\n");
    }
}


/* ============================================================
 * MAIN
 * ============================================================ */

int main(void)
{
    int choice;

    /*
     * Initialize HAL.
     */

    if (audio_hal_init() < 0) {

        printf("Audio HAL initialization failed\n");

        return EXIT_FAILURE;
    }


    while (1) {

        print_menu();

        if (scanf("%d", &choice) != 1) {

            printf("Invalid input\n");

            /*
             * Clear invalid input.
             */

            int c;

            while ((c = getchar()) != '\n' &&
                   c != EOF);

            continue;
        }


        switch (choice) {

        /* ----------------------------------------------------
         * 1. List cards
         * ---------------------------------------------------- */

        case 1:

            audio_hal_list_cards();

            break;


        /* ----------------------------------------------------
         * 2. Select card
         * ---------------------------------------------------- */

        case 2:

            select_card();

            break;


        /* ----------------------------------------------------
         * 3. List PCM devices
         * ---------------------------------------------------- */

        case 3:

            audio_hal_list_pcm_devices();

            break;


        /* ----------------------------------------------------
         * 4. Select PCM device
         * ---------------------------------------------------- */

        case 4:

            select_pcm_device();

            break;


        /* ----------------------------------------------------
         * 5. Play
         * ---------------------------------------------------- */

        case 5:

            play_wav();

            break;


        /* ----------------------------------------------------
         * 6. Capture
         * ---------------------------------------------------- */

        case 6:

            capture_audio();

            break;


        /* ----------------------------------------------------
         * 7. Stop playback
         * ---------------------------------------------------- */

        case 7:

            audio_hal_stop_playback();

            break;


        /* ----------------------------------------------------
         * 8. Stop capture
         * ---------------------------------------------------- */

        case 8:

            audio_hal_stop_capture();

            break;


        /* ----------------------------------------------------
         * 9. Pause / Resume
         * ---------------------------------------------------- */

        case 9:

            audio_hal_pause_resume();

            break;


        /* ----------------------------------------------------
         * 10. Volume up
         * ---------------------------------------------------- */

        case 10:

            audio_hal_volume_up();

            break;


        /* ----------------------------------------------------
         * 11. Volume down
         * ---------------------------------------------------- */

        case 11:

            audio_hal_volume_down();

            break;


        /* ----------------------------------------------------
         * 12. Mute
         * ---------------------------------------------------- */

        case 12:

            audio_hal_mute();

            break;


        /* ----------------------------------------------------
         * 13. Unmute
         * ---------------------------------------------------- */

        case 13:

            audio_hal_unmute();

            break;


        /* ----------------------------------------------------
         * 14. Output route
         * ---------------------------------------------------- */

        case 14:

            select_output_route();

            break;


        /* ----------------------------------------------------
         * 15. Input route
         * ---------------------------------------------------- */

        case 15:

            select_input_route();

            break;


        /* ----------------------------------------------------
         * 0. Exit
         * ---------------------------------------------------- */

        case 0:

            audio_hal_deinit();

            printf("Exiting audio test application\n");

            return EXIT_SUCCESS;


        default:

            printf("Invalid choice\n");

            break;
        }
    }


    return EXIT_SUCCESS;
}