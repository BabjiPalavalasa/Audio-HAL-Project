#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <limits.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#include <tinyalsa/asoundlib.h>
#include <tinyalsa/pcm.h>

#include "../include/audio_hal.h"
#include "../include/audio_types.h"
#include "../include/codec_controls.h"


/* ============================================================
 * Internal HAL state
 * ============================================================ */

typedef enum {
    HAL_ROUTE_AUTO = 0,
    HAL_ROUTE_FORCED_SPEAKER,
    HAL_ROUTE_FORCED_HEADPHONE
} hal_output_mode_t;

typedef enum {
    HAL_INPUT_AUTO = 0,
    HAL_INPUT_FORCED_MIC,
    HAL_INPUT_FORCED_LINE_IN
} hal_input_mode_t;


/* ============================================================
 * WAV header
 * ============================================================ */

typedef struct {
    uint16_t audio_format;
    uint16_t channels;
    uint32_t sample_rate;
    uint16_t bits_per_sample;

    uint32_t data_offset;
    uint32_t data_size;
} wav_info_t;


/* ============================================================
 * Global HAL state
 * ============================================================ */

static pthread_mutex_t hal_lock = PTHREAD_MUTEX_INITIALIZER;

static int hal_initialized = 0;

static int selected_card = -1;

static int playback_device = -1;
static int capture_device = -1;

static hal_output_mode_t output_mode = HAL_ROUTE_AUTO;
static hal_input_mode_t input_mode = HAL_INPUT_AUTO;

static int volume_percent = 80;
static int muted = 0;

static int playback_active = 0;
static int capture_active = 0;

static int playback_paused = 0;

static pthread_t playback_thread;
static pthread_t capture_thread;

static audio_stream_t playback_stream;
static audio_stream_t capture_stream;


/* ============================================================
 * Forward declarations
 * ============================================================ */

static int mixer_set_value(
        int card,
        const char *name,
        int value);

static int mixer_set_percent(
        int card,
        const char *name,
        int percent);

static int configure_playback_codec(void);

static int configure_capture_codec(void);

static int route_to_speaker(void);

static int route_to_headphone(void);

static int detect_headphone(void);

static int detect_line_microphone(void);

static int parse_wav(
        FILE *fp,
        wav_info_t *info);

static int pcm_format_from_audio_format(
        audio_format_t format,
        enum pcm_format *pcm_format);

static void *playback_worker(void *arg);

static void *capture_worker(void *arg);


/* ============================================================
 * HAL INIT   (Initialize/Reset for the internal state of Audio HAL)
 * ============================================================ */

int audio_hal_init(void)
{
    pthread_mutex_lock(&hal_lock);

    if (hal_initialized) {
        pthread_mutex_unlock(&hal_lock);
        return 0;
    }

    selected_card = -1;

    playback_device = -1;
    capture_device = -1;

    output_mode = HAL_ROUTE_AUTO;
    input_mode = HAL_INPUT_AUTO;

    volume_percent = 80;
    muted = 0;

    playback_active = 0;
    capture_active = 0;
    playback_paused = 0;

    memset(&playback_stream,
           0,
           sizeof(playback_stream));

    memset(&capture_stream,
           0,
           sizeof(capture_stream));

    hal_initialized = 1;

    pthread_mutex_unlock(&hal_lock);

    printf("Audio HAL initialized\n");

    return 0;
}


/* ============================================================
 * HAL DEINIT
 * ============================================================ */

void audio_hal_deinit(void)
{
    audio_hal_exit();
}


/* ============================================================
 * LIST CARDS
 * ============================================================ */

int audio_hal_list_cards(void)
{
    DIR *dir;
    struct dirent *entry;

    printf("\n");
    printf("========================================\n");
    printf("              SOUND CARDS\n");
    printf("========================================\n");

    dir = opendir("/proc/asound");

    if (!dir) {
        printf("Failed to open /proc/asound: %s\n",
               strerror(errno));
        return -errno;
    }

    while ((entry = readdir(dir)) != NULL) {

        if (strncmp(entry->d_name, "card", 4) != 0)
            continue;

        char path[PATH_MAX];

        snprintf(path,
                 sizeof(path),
                 "/proc/asound/%s/id",
                 entry->d_name);

        FILE *fp = fopen(path, "r");

        if (!fp)
            continue;

        char id[128] = {0};

        if (fgets(id, sizeof(id), fp)) {
            id[strcspn(id, "\n")] = '\0';

            printf("%s : %s\n",
                   entry->d_name,
                   id);
        }

        fclose(fp);
    }

    closedir(dir);

    printf("========================================\n");

    return 0;
}


/* ============================================================
 * SELECT CARD
 * ============================================================ */

int audio_hal_select_card(const char *card_id)
{
    DIR *dir;
    struct dirent *entry;

    if (!card_id)
        return -EINVAL;

    pthread_mutex_lock(&hal_lock);

    selected_card = -1;

    dir = opendir("/proc/asound");

    if (!dir) {
        pthread_mutex_unlock(&hal_lock);
        return -errno;
    }

    while ((entry = readdir(dir)) != NULL) {

        if (strncmp(entry->d_name, "card", 4) != 0)
            continue;

        if (entry->d_name[4] < '0' ||
            entry->d_name[4] > '9')
            continue;

        char path[PATH_MAX];

        snprintf(path,
                 sizeof(path),
                 "/proc/asound/%s/id",
                 entry->d_name);

        FILE *fp = fopen(path, "r");

        if (!fp)
            continue;

        char id[128] = {0};

        if (fgets(id, sizeof(id), fp)) {

            id[strcspn(id, "\n")] = '\0';

            if (strcmp(id, card_id) == 0) {

                selected_card = atoi(entry->d_name + 4);

                fclose(fp);
                closedir(dir);

                pthread_mutex_unlock(&hal_lock);

                printf("Selected ALSA card: %d (%s)\n",
                       selected_card,
                       card_id);

                return 0;
            }
        }

        fclose(fp);
    }

    closedir(dir);

    pthread_mutex_unlock(&hal_lock);

    printf("Card '%s' not found\n", card_id);

    return -ENOENT;
}


/* ============================================================
 * LIST ALL PCM DEVICES
 *
 * Playback and capture are shown separately.
 * ============================================================ */

int audio_hal_list_pcm_devices(void)
{
    if (selected_card < 0) {
        printf("Select sound card first\n");
        return -EINVAL;
    }

    char command[PATH_MAX];

    printf("\n========================================\n");
    printf("          PLAYBACK PCM DEVICES\n");
    printf("========================================\n");

    snprintf(command,sizeof(command),"cat /proc/asound/card%d/pcm*p/sub*/info ""2>/dev/null | grep -E 'stream|id|name'",selected_card);

    /*
     * /proc does not provide a simple reliable grouping
     * for every kernel version, so use aplay for playback.
     */

    snprintf(command,sizeof(command),"aplay -l 2>/dev/null | ""grep -A4 'card %d:'",selected_card);

    system(command);


    printf("\n========================================\n");
    printf("           CAPTURE PCM DEVICES\n");
    printf("========================================\n");

    snprintf(command,
             sizeof(command),
             "arecord -l 2>/dev/null | "
             "grep -A4 'card %d:'",
             selected_card);

    system(command);

    return 0;
}


/* ============================================================
 * SELECT PCM DEVICE
 * ============================================================ */

int audio_hal_select_pcm_device(
        audio_direction_t direction,
        int device)
{
    if (selected_card < 0) {
        printf("Select sound card first\n");
        return -EINVAL;
    }

    if (device < 0)
        return -EINVAL;

    if (direction == AUDIO_PLAYBACK) {

        playback_device = device;

        printf("Playback PCM device = %d\n",
               device);

        return 0;
    }

    if (direction == AUDIO_CAPTURE) {

        capture_device = device;

        printf("Capture PCM device = %d\n",
               device);

        return 0;
    }

    return -EINVAL;
}


/* ============================================================
 * TINYALSA MIXER SET VALUE
 * ============================================================ */

static int mixer_set_value(int card,const char *name,int value)
{
    struct mixer *mixer;
    struct mixer_ctl *ctl;

    if (!name)
        return -EINVAL;

    mixer = mixer_open(card);

    if (!mixer) {
        printf("mixer_open(%d) failed\n",card);
        return -ENODEV;
    }

    ctl = mixer_get_ctl_by_name(mixer,name);

    if (!ctl) {

        printf("Mixer control not found: %s\n",name);

        mixer_close(mixer);

        return -ENOENT;
    }

    unsigned int count = mixer_ctl_get_num_values(ctl);

    int ret = 0;

    for (unsigned int i = 0; i < count; i++) {

        ret = mixer_ctl_set_value(ctl,i,value);

        if (ret < 0)
            break;
    }

    mixer_close(mixer);

    return ret;
}


/* ============================================================
 * TINYALSA MIXER SET PERCENT
 * ============================================================ */

static int mixer_set_percent(int card,const char *name,int percent)
{
    struct mixer *mixer;
    struct mixer_ctl *ctl;

    if (percent < 0)
        percent = 0;

    if (percent > 100)
        percent = 100;

    mixer = mixer_open(card);

    if (!mixer)
        return -ENODEV;

    ctl = mixer_get_ctl_by_name(mixer, name);

    if (!ctl) {

        mixer_close(mixer);

        printf("Mixer control not found: %s\n",
               name);

        return -ENOENT;
    }

    long min;
    long max;

    min=mixer_ctl_get_range_min(ctl);
    max=mixer_ctl_get_range_max(ctl);

    if (min < 0 || max < 0) {

        mixer_close(mixer);

        return -EINVAL;
    }

    int value = min + ((max - min) * percent) / 100;

    unsigned int count =  mixer_ctl_get_num_values(ctl);

    int ret = 0;

    for (unsigned int i = 0; i < count; i++) {

        ret = mixer_ctl_set_value(ctl,i,value);

        if (ret < 0)
            break;
    }

    mixer_close(mixer);

    return ret;
}


/* ============================================================
 * ENABLE PCM OUTPUT MIXER PATH
 *
 * According to 58 controls:
 *
 * Left Output Mixer PCM Playback Switch
 * Right Output Mixer PCM Playback Switch
 * ============================================================ */

static int enable_pcm_output_path(void)
{
    if (selected_card < 0)
        return -EINVAL;

    mixer_set_value(selected_card,CTL_LEFT_OUT_PCM,1);

    mixer_set_value(selected_card,CTL_RIGHT_OUT_PCM,1);

    /*
     * Make sure unwanted analog input paths
     * are not mixed into playback.
     */

    mixer_set_value(selected_card,CTL_LEFT_OUT_LINPUT3,0);

    mixer_set_value(selected_card,CTL_RIGHT_OUT_RINPUT3,0);

    mixer_set_value(selected_card,CTL_LEFT_OUT_BOOST_BYPASS,0);

    mixer_set_value(selected_card,CTL_RIGHT_OUT_BOOST_BYPASS,0);

    return 0;
}


/* ============================================================
 * ROUTE SPEAKER
 * ============================================================ */

static int route_to_speaker(void)
{
    if (selected_card < 0)
        return -EINVAL;

    printf("HAL: routing playback -> SPEAKER\n");

    enable_pcm_output_path();

    /*
     * codec has:
     *
     * Speaker Playback Volume : 0..127
     * Headphone Playback Volume : 0..127
     *
     * Use the user volume percentage.
     */

    mixer_set_percent(selected_card,CTL_SPEAKER_PLAYBACK_VOLUME,volume_percent);

    mixer_set_value(selected_card,CTL_HEADPHONE_PLAYBACK_VOLUME,0);

    return 0;
}


/* ============================================================
 * ROUTE HEADPHONE
 * ============================================================ */

static int route_to_headphone(void)
{
    if (selected_card < 0)
        return -EINVAL;

    printf("HAL: routing playback -> HEADPHONE\n");

    enable_pcm_output_path();

    mixer_set_percent(selected_card,CTL_HEADPHONE_PLAYBACK_VOLUME,volume_percent);

    mixer_set_value(selected_card,CTL_SPEAKER_PLAYBACK_VOLUME,0);

    return 0;
}


/* ============================================================
 * HEADPHONE JACK DETECTION
 *
 * IMPORTANT:
 *
 * The supplied 58 codec controls contain no jack-detect
 * control.
 *
 * Therefore this function checks common Linux jack
 * detection interfaces.
 * ============================================================ */

static int detect_headphone(void)
{
    const char *paths[] = {

        "/sys/class/switch/h2w/state",
        "/sys/class/switch/headphone/state",
        "/sys/class/switch/jack/state",

        NULL
    };

    for (int i = 0; paths[i] != NULL; i++) {
        
        printf("Checking jack detection path: %s\n", paths[i]);
        FILE *fp = fopen(paths[i], "r");

        if (!fp)
        {
            printf("Jack detection path not available: %s\n", paths[i]);
            continue;
        }
        printf("Jack detection path available: %s\n", paths[i]);

        char value[32] = {0};

        if (fgets(value,sizeof(value),fp)) 
        {
            printf("Jack detection value: %s\n", value);
            fclose(fp);

            /*
             * Common switch convention:
             *
             * 0 = no headphone
             * 1 = headphone
             * 2 = headset
             */

            int state = atoi(value);
            printf("Jack detection state: %d\n", state);

            if (state > 0)
            {
                printf("Headphone detected\n");
                return 1;
            }

            printf("No headphone detected\n");
            return 0;
        }
        printf("Failed to read jack detection value from: %s\n", paths[i]);

        fclose(fp);
    }

    /*
     * No jack detector available.
     *
     * Return -1 rather than falsely saying that a
     * headphone is detected.
     */

    return -1;
}


/* ============================================================
 * INLINE / LINE MICROPHONE DETECTION
 *
 * This is also board/kernel dependent.
 *
 * The supplied 58 codec controls don't contain a jack
 * detection control.
 * ============================================================ */

static int detect_line_microphone(void)
{
    const char *paths[] = {

        "/sys/class/switch/linein/state",
        "/sys/class/switch/mic/state",
        "/sys/class/switch/headset/state",

        NULL
    };

    for (int i = 0; paths[i] != NULL; i++) {

        FILE *fp = fopen(paths[i], "r");

        if (!fp)
            continue;

        char value[32] = {0};

        if (fgets(value,sizeof(value),fp)) {

            fclose(fp);

            return atoi(value) > 0 ? 1 : 0;
        }

        fclose(fp);
    }

    return -1;
}


/* ============================================================
 * PLAYBACK AUTO ROUTING
 * ============================================================ */

static int auto_route_playback(void)
{
    int jack = detect_headphone();

    if (output_mode == HAL_ROUTE_FORCED_SPEAKER)
        return route_to_speaker();

    if (output_mode == HAL_ROUTE_FORCED_HEADPHONE)
        return route_to_headphone();

    /*
     * AUTO mode.
     */

    if (jack > 0)
        return route_to_headphone();

    /*
     * No jack OR detector unavailable:
     * safely use speaker.
     */

    return route_to_speaker();
}


/* ============================================================
 * CAPTURE CODEC CONFIGURATION
 *
 * Default path:
 *
 * LINPUT3 / RINPUT3
 *
 * based on the controls you supplied.
 * ============================================================ */

static int configure_capture_codec(void)
{
    if (selected_card < 0)
        return -EINVAL;

    /*
     * Enable left/right input boost mixers.
     */

    mixer_set_value(selected_card,CTL_LEFT_INPUT_BOOST,1);

    mixer_set_value(selected_card,CTL_RIGHT_INPUT_BOOST,1);


    /*
     * LINPUT3 -> left boost mixer.
     */

    mixer_set_value(selected_card,CTL_LEFT_BOOST_LINPUT1,0);

    mixer_set_value(selected_card,CTL_LEFT_BOOST_LINPUT2,0);

    mixer_set_value(selected_card,CTL_LEFT_BOOST_LINPUT3,1);


    /*
     * RINPUT3 -> right boost mixer.
     */

    mixer_set_value(selected_card,CTL_RIGHT_BOOST_RINPUT1,0);

    mixer_set_value(selected_card,CTL_RIGHT_BOOST_RINPUT2,0);

    mixer_set_value(selected_card,CTL_RIGHT_BOOST_RINPUT3,1);


    /*
     * Capture switch ON.
     */

    mixer_set_value(selected_card,CTL_CAPTURE_SWITCH,1);


    /*
     * Capture volume.
     */

    mixer_set_percent(selected_card,CTL_CAPTURE_VOLUME,80);

    /*
     * ADC PCM capture volume.
     */

    mixer_set_percent(selected_card,CTL_ADC_PCM_CAPTURE_VOLUME,80);

    return 0;
}


/* ============================================================
 * INPUT ROUTING
 * ============================================================ */

static int route_capture_input(void)
{
    int line_mic = detect_line_microphone();

    if (input_mode == HAL_INPUT_FORCED_LINE_IN) {

        printf("HAL: input -> LINE/INLINE MIC\n");

        /*
         * Current codec controls show LINPUT3/RINPUT3
         * as available external input paths.
         */

        return configure_capture_codec();
    }

    if (input_mode == HAL_INPUT_FORCED_MIC) {

        printf("HAL: input -> INTERNAL MIC\n");

        return configure_capture_codec();
    }

    /*
     * AUTO
     */

    if (line_mic > 0) {

        printf("HAL: inline/line microphone detected\n");

        return configure_capture_codec();
    }

    printf("HAL: inline microphone not detected\n");
    printf("HAL: using default microphone input path\n");

    return configure_capture_codec();
}


/* ============================================================
 * PLAYBACK CODEC CONFIGURATION
 * ============================================================ */

static int configure_playback_codec(void)
{
    if (selected_card < 0)
        return -EINVAL;

    /*
     * Enable PCM playback path.
     */

    enable_pcm_output_path();

    /*
     * Restore requested volume.
     */

    if (output_mode == HAL_ROUTE_FORCED_HEADPHONE)
        return route_to_headphone();

    if (output_mode == HAL_ROUTE_FORCED_SPEAKER)
        return route_to_speaker();

    return auto_route_playback();
}

static int update_wav_header(
        FILE *fp,
        uint32_t data_size)
{
    uint32_t riff_size = 36 + data_size;

    if (!fp)
        return -EINVAL;

    /*
     * Update RIFF chunk size.
     *
     * RIFF size field starts at byte 4.
     */
    if (fseek(fp, 4, SEEK_SET) != 0)
        return -EIO;

    if (fwrite(&riff_size,
                sizeof(riff_size),
                1,
                fp) != 1)
        return -EIO;

    /*
     * Update data chunk size.
     *
     * data size field starts at byte 40.
     */
    if (fseek(fp, 40, SEEK_SET) != 0)
        return -EIO;

    if (fwrite(&data_size,
                sizeof(data_size),
                1,
                fp) != 1)
        return -EIO;

    fflush(fp);

    return 0;
}

/* ============================================================
 * WAV WRITER
 * ============================================================ */

static int write_wav_header(
        FILE *fp,
        uint16_t channels,
        uint32_t sample_rate,
        uint16_t bits_per_sample,
        uint32_t data_size)
{
    uint32_t riff_size;
    uint32_t fmt_size = 16;
    uint16_t audio_format = 1; /* PCM */

    uint16_t block_align =
        channels * (bits_per_sample / 8);

    uint32_t byte_rate =
        sample_rate * block_align;

    if (!fp)
        return -EINVAL;

    /*
     * RIFF size = everything after
     * RIFF header itself.
     */
    riff_size = 36 + data_size;

    /* RIFF */
    if (fwrite("RIFF", 1, 4, fp) != 4)
        return -EIO;

    if (fwrite(&riff_size, sizeof(riff_size), 1, fp) != 1)
        return -EIO;

    /* WAVE */
    if (fwrite("WAVE", 1, 4, fp) != 4)
        return -EIO;

    /* fmt chunk */
    if (fwrite("fmt ", 1, 4, fp) != 4)
        return -EIO;

    if (fwrite(&fmt_size, sizeof(fmt_size), 1, fp) != 1)
        return -EIO;

    if (fwrite(&audio_format, sizeof(audio_format), 1, fp) != 1)
        return -EIO;

    if (fwrite(&channels, sizeof(channels), 1, fp) != 1)
        return -EIO;

    if (fwrite(&sample_rate, sizeof(sample_rate), 1, fp) != 1)
        return -EIO;

    if (fwrite(&byte_rate, sizeof(byte_rate), 1, fp) != 1)
        return -EIO;

    if (fwrite(&block_align, sizeof(block_align), 1, fp) != 1)
        return -EIO;

    if (fwrite(&bits_per_sample,
               sizeof(bits_per_sample),
               1,
               fp) != 1)
        return -EIO;

    /* data chunk */
    if (fwrite("data", 1, 4, fp) != 4)
        return -EIO;

    if (fwrite(&data_size, sizeof(data_size), 1, fp) != 1)
        return -EIO;

    return 0;
}


/* ============================================================
 * WAV PARSER
 *
 * Handles normal RIFF/WAVE PCM files and scans chunks
 * until "fmt " and "data" are found.
 * ============================================================ */

static int parse_wav(FILE *fp,wav_info_t *info)
{
    char riff[4];
    uint32_t riff_size;
    char wave[4];

    if (!fp || !info)
        return -EINVAL;

    memset(info, 0, sizeof(*info));

    if (fread(riff, 1, 4, fp) != 4)
        return -EINVAL;

    if (memcmp(riff, "RIFF", 4) != 0)
        return -EINVAL;

    if (fread(&riff_size,sizeof(riff_size),1,fp) != 1)
        return -EINVAL;

    (void)riff_size;

    if (fread(wave, 1, 4, fp) != 4)
        return -EINVAL;

    if (memcmp(wave, "WAVE", 4) != 0)
        return -EINVAL;


    int fmt_found = 0;
    int data_found = 0;

    while (!fmt_found || !data_found) 
    {

        char chunk_id[4];
        uint32_t chunk_size;

        if (fread(chunk_id, 1, 4, fp) != 4)
            break;

        if (fread(&chunk_size,sizeof(chunk_size),1,fp) != 1)
            break;

        if (memcmp(chunk_id, "fmt ", 4) == 0) {

            if (chunk_size < 16)
                return -EINVAL;

            uint16_t format;
            uint16_t channels;
            uint32_t rate;
            uint32_t byte_rate;
            uint16_t block_align;
            uint16_t bits;

            if (fread(&format,sizeof(format),1,fp) != 1)
                return -EINVAL;

            if (fread(&channels,sizeof(channels),1,fp) != 1)
                return -EINVAL;

            if (fread(&rate,sizeof(rate),1,fp) != 1)
                return -EINVAL;

            if (fread(&byte_rate,sizeof(byte_rate),1,fp) != 1)
                return -EINVAL;

            if (fread(&block_align,sizeof(block_align),1,fp) != 1)
                return -EINVAL;

            if (fread(&bits,sizeof(bits),1,fp) != 1)
                return -EINVAL;

            (void)byte_rate;
            (void)block_align;

            info->audio_format = format;
            info->channels = channels;
            info->sample_rate = rate;
            info->bits_per_sample = bits;

            /*
             * Skip remaining fmt bytes.
             */

            if (chunk_size > 16)
                fseek(fp,chunk_size - 16,SEEK_CUR);

            fmt_found = 1;
        }
        else if (memcmp(chunk_id, "data", 4) == 0) {

            info->data_offset = (uint32_t)ftell(fp);

            info->data_size =chunk_size;

            data_found = 1;

            /*
             * Don't skip data.
             */

            break;
        }
        else {

            /*
             * Skip unknown RIFF chunk.
             */

            fseek(fp,chunk_size + (chunk_size & 1),SEEK_CUR);
        }
    }

    if (!fmt_found || !data_found)
        return -EINVAL;

    if (info->audio_format != 1) {

        printf("Only PCM WAV supported\n");

        return -ENOTSUP;
    }

    if (info->channels < 1 || info->channels > 2)
        return -EINVAL;

    if (info->bits_per_sample != 16 && info->bits_per_sample != 24 && info->bits_per_sample != 32)
        return -ENOTSUP;

    return 0;
}


/* ============================================================
 * AUDIO FORMAT -> TINYALSA FORMAT
 * ============================================================ */

static int pcm_format_from_audio_format(audio_format_t format,
        enum pcm_format *pcm_format)
{
    if (!pcm_format)
        return -EINVAL;

    switch (format) {

    case AUDIO_FORMAT_S16_LE:
        *pcm_format = PCM_FORMAT_S16_LE;
        return 0;

    case AUDIO_FORMAT_S24_LE:
        *pcm_format = PCM_FORMAT_S24_3LE;
        return 0;

    case AUDIO_FORMAT_S32_LE:
        *pcm_format = PCM_FORMAT_S32_LE;
        return 0;

    default:
        return -EINVAL;
    }
}


/* ============================================================
 * PLAYBACK WORKER
 * ============================================================ */

static void *playback_worker(void *arg)
{
    char *filename = (char *)arg;

    FILE *fp = NULL;
    void *buffer = NULL;

    struct pcm_config config;

    wav_info_t wav;

    memset(&config, 0, sizeof(config));

    fp = fopen(filename, "rb");

    if (!fp) {

        printf("Cannot open WAV: %s\n",
               filename);

        goto error;
    }

    if (parse_wav(fp, &wav) < 0) {

        printf("Invalid/unsupported WAV file\n");

        goto error;
    }

    printf("\nWAV:\n");
    printf("  Rate     : %u\n", wav.sample_rate);
    printf("  Channels : %u\n", wav.channels);
    printf("  Bits     : %u\n", wav.bits_per_sample);


    audio_format_t format;

    if (wav.bits_per_sample == 16)
        format = AUDIO_FORMAT_S16_LE;
    else if (wav.bits_per_sample == 24)
        format = AUDIO_FORMAT_S24_LE;
    else
        format = AUDIO_FORMAT_S32_LE;


    enum pcm_format pcm_fmt;

    if (pcm_format_from_audio_format(format, &pcm_fmt) < 0)
        goto error;


    if (playback_device < 0) {

        printf("Playback PCM device not selected\n");

        goto error;
    }


    /*
     * Configure codec BEFORE PCM starts.
     *
     * This is where dynamic jack routing happens.
     */

    if (configure_playback_codec() < 0)
        goto error;


    memset(&playback_stream,0,sizeof(playback_stream));

    playback_stream.direction = AUDIO_PLAYBACK;

    playback_stream.format = format;

    playback_stream.sample_rate = wav.sample_rate;

    playback_stream.channels = wav.channels;

    playback_stream.endpoint.card = selected_card;

    playback_stream.endpoint.device = playback_device;

    playback_stream.state = AUDIO_STREAM_CONFIGURED;

    memset(&config, 0, sizeof(config));

    config.channels = wav.channels;
    config.rate = wav.sample_rate;
    config.period_size = 1024;
    config.period_count = 4;
    config.format = pcm_fmt;

    config.start_threshold = 0;
    config.stop_threshold = 0;
    config.silence_threshold = 0;


    playback_stream.pcm = pcm_open(selected_card,playback_device,PCM_OUT,&config);

    if (!playback_stream.pcm || !pcm_is_ready(playback_stream.pcm)) {

        printf("PCM open failed: %s\n", playback_stream.pcm ? pcm_get_error(playback_stream.pcm) : "NULL");

        if (playback_stream.pcm)
            pcm_close(playback_stream.pcm);

        playback_stream.pcm = NULL;

        goto error;
    }

    playback_stream.state = AUDIO_STREAM_OPEN;


    if (pcm_prepare(playback_stream.pcm) < 0) {

        printf("PCM prepare failed: %s\n", pcm_get_error(playback_stream.pcm));

        goto error_pcm;
    }

    playback_stream.state = AUDIO_STREAM_RUNNING;

    /*
     * Seek to actual WAV data.
     */

    fseek(fp,wav.data_offset,SEEK_SET);


    unsigned int bytes_per_sample = wav.bits_per_sample / 8;

    unsigned int bytes_per_frame = bytes_per_sample * wav.channels;

    unsigned int buffer_frames = 1024;

    unsigned int buffer_bytes = buffer_frames * bytes_per_frame;

    buffer = malloc(buffer_bytes);

    if (!buffer)
    {
        printf("Failed to allocate playback buffer: %u bytes\n", buffer_bytes);
        goto error_pcm;
    }


    printf("Playback started\n");


    uint32_t remaining = wav.data_size;


    while (playback_active && remaining > 0) 
    {

        /*
         * Pause handling.
         */

        while (playback_paused && playback_active) {

            usleep(10000);
        }

        if (!playback_active)
            break;


        unsigned int request = remaining > buffer_bytes ? buffer_bytes : remaining;

        size_t bytes = fread(buffer,1,request,fp);

        if (bytes == 0)
            break;


        /*
         * Ensure complete frames.
         */

        unsigned int frames = bytes / bytes_per_frame;

        unsigned int write_bytes = frames * bytes_per_frame;

        if (frames == 0)
            break;


        if (pcm_writei(playback_stream.pcm,buffer,frames) < 0) {

            printf("PCM write failed: %s\n",
                   pcm_get_error(playback_stream.pcm));

            break;
        }

        remaining -= write_bytes;
    }


    free(buffer);
    buffer = NULL;

    fclose(fp);
    fp = NULL;


    if (playback_stream.pcm) {

        pcm_stop(playback_stream.pcm);
        pcm_close(playback_stream.pcm);

        playback_stream.pcm = NULL;
    }

    playback_stream.state = AUDIO_STREAM_STOPPED;

    playback_active = 0;
    playback_paused = 0;

    free(filename);

    printf("Playback finished\n");

    return NULL;


error_pcm:

    if (buffer)
        free(buffer);

    if (fp)
        fclose(fp);

    if (playback_stream.pcm) {

        pcm_stop(playback_stream.pcm);
        pcm_close(playback_stream.pcm);

        playback_stream.pcm = NULL;
    }

error:

    playback_stream.state =
        AUDIO_STREAM_ERROR;

    playback_active = 0;
    playback_paused = 0;

    if (filename)
        free(filename);

    return NULL;
}


/* ============================================================
 * PLAY
 *
 * Application only calls this one API.
 * ============================================================ */

int audio_hal_play(const char *filename)
{
    if (!filename)
        return -EINVAL;

    if (selected_card < 0) {
        printf("Select sound card first\n");

        return -EINVAL;
    }

    if (playback_device < 0) {

        printf("Select playback PCM device first\n");

        return -EINVAL;
    }

    pthread_mutex_lock(&hal_lock);

    if (playback_active) {

        pthread_mutex_unlock(&hal_lock);

        printf("Playback already running\n");

        return -EBUSY;
    }

    char *copy = strdup(filename);

    if (!copy) {

        pthread_mutex_unlock(&hal_lock);

        return -ENOMEM;
    }

    playback_active = 1;
    playback_paused = 0;

    pthread_mutex_unlock(&hal_lock);


    if (pthread_create(&playback_thread,NULL,playback_worker,copy) != 0) 
    {
        pthread_mutex_lock(&hal_lock);

        playback_active = 0;

        pthread_mutex_unlock(&hal_lock);

        free(copy);

        return -EIO;
    }

    /*
     * Detached thread.
     *
     * Application doesn't need to know thread details.
     */

    pthread_detach(playback_thread);

    return 0;
}


/* ============================================================
 * CAPTURE WORKER
 * ============================================================ */

static void *capture_worker(void *arg)
{
    char *filename = (char *)arg;

    FILE *fp = NULL;

    struct pcm_config config;

    void *buffer = NULL;

    unsigned int rate = 48000;
    unsigned int channels = 2;
    unsigned int bits = 16;

    unsigned int frames = 1024;

    unsigned int bytes = frames * channels * 2;


    /*
     * Capture routing is entirely internal.
     */

    if (route_capture_input() < 0)
        goto error;


    memset(&capture_stream,0,sizeof(capture_stream));

    capture_stream.direction = AUDIO_CAPTURE;

    capture_stream.format = AUDIO_FORMAT_S16_LE;

    capture_stream.sample_rate = rate;

    capture_stream.channels = channels;

    capture_stream.endpoint.card = selected_card;

    capture_stream.endpoint.device = capture_device;


    memset(&config, 0, sizeof(config));

    config.channels = channels;
    config.rate = rate;
    config.period_size = frames;
    config.period_count = 4;
    config.format = PCM_FORMAT_S16_LE;

    config.start_threshold = 0;
    config.stop_threshold = 0;
    config.silence_threshold = 0;


    capture_stream.pcm =  pcm_open(selected_card,capture_device,
                 PCM_IN,
                 &config);


    if (!capture_stream.pcm || !pcm_is_ready(capture_stream.pcm)) {

        printf("Capture PCM open failed: %s\n", capture_stream.pcm ?
               pcm_get_error(capture_stream.pcm) :
               "NULL");

        if (capture_stream.pcm)
            pcm_close(capture_stream.pcm);

        capture_stream.pcm = NULL;

        goto error;
    }


    capture_stream.state = AUDIO_STREAM_OPEN;


    if (pcm_prepare(capture_stream.pcm) < 0)
        goto error_pcm;


    capture_stream.state = AUDIO_STREAM_RUNNING;


    fp = fopen(filename, "wb");

    if (!fp) {

        printf("Cannot create capture file: %s\n",
               filename);

        goto error_pcm;
    }

    uint32_t total_data_bytes = 0;

    if(write_wav_header(fp,channels,rate,bits,total_data_bytes) < 0) {

        printf("Failed to write WAV header\n");

        goto error_pcm;
    }

    buffer = malloc(bytes);

    if (!buffer)
    {
        printf("Failed to allocate capture buffer: %u bytes\n",bytes);
        goto error_pcm;
    }


    printf("Capture started -> %s\n",
           filename);


    while (capture_active) {

    int captured_frames;

    captured_frames =
        pcm_readi(capture_stream.pcm,
                  buffer,
                  frames);

    if (captured_frames < 0) {

        printf("PCM read failed: %s\n",
               pcm_get_error(capture_stream.pcm));

        break;
    }

    if (captured_frames == 0)
        continue;

    size_t capture_bytes =
        (size_t)captured_frames *
        channels *
        sizeof(int16_t);

    size_t written =
        fwrite(buffer,
               1,
               capture_bytes,
               fp);

    if (written != capture_bytes) {

        printf("Failed to write complete audio buffer\n");

        break;
    }

    total_data_bytes += written;
}


    if (update_wav_header(fp, total_data_bytes) < 0) {

        printf("Failed to update WAV header\n");
    }
    
    free(buffer);

    fclose(fp);


    pcm_stop(capture_stream.pcm);

    pcm_close(capture_stream.pcm);

    capture_stream.pcm = NULL;

    capture_stream.state =
        AUDIO_STREAM_STOPPED;

    capture_active = 0;

    free(filename);

    printf("Capture stopped\n");

    return NULL;


error_pcm:

    if (buffer)
        free(buffer);

    if (fp)
        fclose(fp);

    if (capture_stream.pcm) {

        pcm_stop(capture_stream.pcm);
        pcm_close(capture_stream.pcm);

        capture_stream.pcm = NULL;
    }

error:

    capture_stream.state =
        AUDIO_STREAM_ERROR;

    capture_active = 0;

    free(filename);

    return NULL;
}


/* ============================================================
 * CAPTURE
 * ============================================================ */

int audio_hal_capture(const char *filename)
{
    if (!filename)
        return -EINVAL;

    if (selected_card < 0) {

        printf("Select sound card first\n");

        return -EINVAL;
    }

    if (capture_device < 0) {

        printf("Select capture PCM device first\n");

        return -EINVAL;
    }

    pthread_mutex_lock(&hal_lock);

    if (capture_active) {

        pthread_mutex_unlock(&hal_lock);

        printf("Capture already running\n");

        return -EBUSY;
    }

    char *copy = strdup(filename);

    if (!copy) {

        pthread_mutex_unlock(&hal_lock);

        return -ENOMEM;
    }

    capture_active = 1;

    pthread_mutex_unlock(&hal_lock);


    if (pthread_create(&capture_thread,
                       NULL,
                       capture_worker,
                       copy) != 0) {

        pthread_mutex_lock(&hal_lock);

        capture_active = 0;

        pthread_mutex_unlock(&hal_lock);

        free(copy);

        return -EIO;
    }

    pthread_detach(capture_thread);

    return 0;
}


/* ============================================================
 * STOP PLAYBACK
 * ============================================================ */

int audio_hal_stop_playback(void)
{
    if (!playback_active)
        return 0;

    printf("Stopping playback...\n");

    playback_active = 0;

    /*
     * Worker sees playback_active == 0 and exits.
     */

    return 0;
}


/* ============================================================
 * STOP CAPTURE
 * ============================================================ */

int audio_hal_stop_capture(void)
{
    if (!capture_active)
        return 0;

    printf("Stopping capture...\n");

    capture_active = 0;

    return 0;
}


/* ============================================================
 * PAUSE / RESUME
 * ============================================================ */

int audio_hal_pause_resume(void)
{
    if (!playback_active) {

        printf("Playback is not running\n");

        return -EINVAL;
    }

    playback_paused = !playback_paused;

    if (playback_paused)
        printf("Playback PAUSED\n");
    else
        printf("Playback RESUMED\n");

    return 0;
}


/* ============================================================
 * VOLUME UP
 *
 * Uses codec's:
 *
 * Headphone Playback Volume
 * Speaker Playback Volume
 *
 * range is automatically obtained by mixer API.
 * ============================================================ */

int audio_hal_volume_up(void)
{
    if (selected_card < 0)
        return -EINVAL;

    volume_percent += 10;

    if (volume_percent > 100)
        volume_percent = 100;


    /*
     * Update currently selected output.
     */

    if (output_mode == HAL_ROUTE_FORCED_HEADPHONE) {

        mixer_set_percent(
                selected_card,
                CTL_HEADPHONE_PLAYBACK_VOLUME,
                volume_percent);

    }
    else if (output_mode == HAL_ROUTE_FORCED_SPEAKER) {

        mixer_set_percent(
                selected_card,
                CTL_SPEAKER_PLAYBACK_VOLUME,
                volume_percent);

    }
    else {

        int hp = detect_headphone();

        if (hp > 0) {

            mixer_set_percent(
                    selected_card,
                    CTL_HEADPHONE_PLAYBACK_VOLUME,
                    volume_percent);

        } else {

            mixer_set_percent(
                    selected_card,
                    CTL_SPEAKER_PLAYBACK_VOLUME,
                    volume_percent);
        }
    }

    printf("Volume = %d%%\n",
           volume_percent);

    return 0;
}


/* ============================================================
 * VOLUME DOWN
 * ============================================================ */

int audio_hal_volume_down(void)
{
    if (selected_card < 0)
        return -EINVAL;

    volume_percent -= 10;

    if (volume_percent < 0)
        volume_percent = 0;


    if (output_mode == HAL_ROUTE_FORCED_HEADPHONE) {

        mixer_set_percent(
                selected_card,
                CTL_HEADPHONE_PLAYBACK_VOLUME,
                volume_percent);

    }
    else if (output_mode == HAL_ROUTE_FORCED_SPEAKER) {

        mixer_set_percent(
                selected_card,
                CTL_SPEAKER_PLAYBACK_VOLUME,
                volume_percent);

    }
    else {

        int hp = detect_headphone();

        if (hp > 0) {

            mixer_set_percent(
                    selected_card,
                    CTL_HEADPHONE_PLAYBACK_VOLUME,
                    volume_percent);

        } else {

            mixer_set_percent(
                    selected_card,
                    CTL_SPEAKER_PLAYBACK_VOLUME,
                    volume_percent);
        }
    }

    printf("Volume = %d%%\n",
           volume_percent);

    return 0;
}


/* ============================================================
 * MUTE
 *
 * Your codec does NOT have "Playback Switch".
 *
 * Therefore mute both physical output volumes.
 * ============================================================ */

int audio_hal_mute(void)
{
    if (selected_card < 0)
        return -EINVAL;

    mixer_set_value(
            selected_card,
            CTL_HEADPHONE_PLAYBACK_VOLUME,
            0);

    mixer_set_value(
            selected_card,
            CTL_SPEAKER_PLAYBACK_VOLUME,
            0);

    muted = 1;

    printf("Audio MUTED\n");

    return 0;
}


/* ============================================================
 * UNMUTE
 * ============================================================ */

int audio_hal_unmute(void)
{
    if (selected_card < 0)
        return -EINVAL;

    if (output_mode == HAL_ROUTE_FORCED_HEADPHONE) {

        mixer_set_percent(
                selected_card,
                CTL_HEADPHONE_PLAYBACK_VOLUME,
                volume_percent);

    }
    else if (output_mode == HAL_ROUTE_FORCED_SPEAKER) {

        mixer_set_percent(
                selected_card,
                CTL_SPEAKER_PLAYBACK_VOLUME,
                volume_percent);

    }
    else {

        int hp = detect_headphone();

        if (hp > 0)
            route_to_headphone();
        else
            route_to_speaker();
    }

    muted = 0;

    printf("Audio UNMUTED\n");

    return 0;
}


/* ============================================================
 * SELECT OUTPUT ROUTE
 * ============================================================ */

int audio_hal_select_output_route(
        audio_route_t route)
{
    if (selected_card < 0)
        return -EINVAL;

    switch (route) {

    case AUDIO_ROUTE_SPEAKER:

        output_mode =
            HAL_ROUTE_FORCED_SPEAKER;

        return route_to_speaker();


    case AUDIO_ROUTE_HEADPHONE:

        output_mode =
            HAL_ROUTE_FORCED_HEADPHONE;

        return route_to_headphone();


    default:

        printf("Unsupported output route\n");

        return -EINVAL;
    }
}


/* ============================================================
 * SELECT INPUT ROUTE
 * ============================================================ */

int audio_hal_select_input_route(
        audio_route_t route)
{
    if (selected_card < 0)
        return -EINVAL;

    switch (route) {

    case AUDIO_ROUTE_MIC:

        input_mode =
            HAL_INPUT_FORCED_MIC;

        printf("Input route = INTERNAL MIC\n");

        return configure_capture_codec();


    case AUDIO_ROUTE_LINE_IN:

        input_mode =
            HAL_INPUT_FORCED_LINE_IN;

        printf("Input route = LINE/INLINE MIC\n");

        return configure_capture_codec();


    default:

        printf("Unsupported input route\n");

        return -EINVAL;
    }
}


/* ============================================================
 * EXIT
 * ============================================================ */

int audio_hal_exit(void)
{
    /*
     * Stop active streams.
     */

    audio_hal_stop_playback();
    audio_hal_stop_capture();


    /*
     * Give workers a little time to exit.
     */

    usleep(20000);


    /*
     * Disable capture path.
     */

    if (selected_card >= 0) {

        mixer_set_value(
                selected_card,
                CTL_CAPTURE_SWITCH,
                0);

        /*
         * Disable PCM output mixer paths.
         */

        mixer_set_value(
                selected_card,
                CTL_LEFT_OUT_PCM,
                0);

        mixer_set_value(
                selected_card,
                CTL_RIGHT_OUT_PCM,
                0);
    }


    pthread_mutex_lock(&hal_lock);

    selected_card = -1;

    playback_device = -1;
    capture_device = -1;

    playback_active = 0;
    capture_active = 0;

    playback_paused = 0;

    hal_initialized = 0;

    pthread_mutex_unlock(&hal_lock);

    printf("Audio HAL exited\n");

    return 0;
}