#ifndef CODEC_CONTROLS_H
#define CODEC_CONTROLS_H


/* ============================================================
 * INPUT BOOST MIXER
 * ============================================================ */

#define CTL_LEFT_INPUT_BOOST \
    "Left Input Boost Mixer"

#define CTL_RIGHT_INPUT_BOOST \
    "Right Input Boost Mixer"


/* ============================================================
 * LEFT INPUT BOOST INPUTS
 * ============================================================ */

#define CTL_LEFT_BOOST_LINPUT1 \
    "Left Input Boost Mixer LINPUT1 Switch"

#define CTL_LEFT_BOOST_LINPUT2 \
    "Left Input Boost Mixer LINPUT2 Switch"

#define CTL_LEFT_BOOST_LINPUT3 \
    "Left Input Boost Mixer LINPUT3 Switch"


/* ============================================================
 * RIGHT INPUT BOOST INPUTS
 * ============================================================ */

#define CTL_RIGHT_BOOST_RINPUT1 \
    "Right Input Boost Mixer RINPUT1 Switch"

#define CTL_RIGHT_BOOST_RINPUT2 \
    "Right Input Boost Mixer RINPUT2 Switch"

#define CTL_RIGHT_BOOST_RINPUT3 \
    "Right Input Boost Mixer RINPUT3 Switch"


/* ============================================================
 * CAPTURE
 * ============================================================ */

#define CTL_CAPTURE_SWITCH \
    "Capture Switch"

#define CTL_CAPTURE_VOLUME \
    "Capture Volume"

#define CTL_ADC_PCM_CAPTURE_VOLUME \
    "ADC PCM Capture Volume"


/* ============================================================
 * OUTPUT MIXER SWITCHES
 * ============================================================ */

#define CTL_LEFT_OUTPUT_PCM \
    "Left Output Mixer PCM Playback Switch"

#define CTL_LEFT_OUTPUT_LINPUT3 \
    "Left Output Mixer LINPUT3 Switch"

#define CTL_LEFT_OUTPUT_BOOST_BYPASS \
    "Left Output Mixer Boost Bypass Switch"


#define CTL_RIGHT_OUTPUT_PCM \
    "Right Output Mixer PCM Playback Switch"

#define CTL_RIGHT_OUTPUT_RINPUT3 \
    "Right Output Mixer RINPUT3 Switch"

#define CTL_RIGHT_OUTPUT_BOOST_BYPASS \
    "Right Output Mixer Boost Bypass Switch"


    

#define CTL_LEFT_OUT_PCM \
    CTL_LEFT_OUTPUT_PCM

#define CTL_LEFT_OUT_LINPUT3 \
    CTL_LEFT_OUTPUT_LINPUT3

#define CTL_LEFT_OUT_BOOST_BYPASS \
    CTL_LEFT_OUTPUT_BOOST_BYPASS


#define CTL_RIGHT_OUT_PCM \
    CTL_RIGHT_OUTPUT_PCM

#define CTL_RIGHT_OUT_RINPUT3 \
    CTL_RIGHT_OUTPUT_RINPUT3

#define CTL_RIGHT_OUT_BOOST_BYPASS \
    CTL_RIGHT_OUTPUT_BOOST_BYPASS


/* ============================================================
 * OUTPUT VOLUME
 * ============================================================ */

#define CTL_SPEAKER_PLAYBACK_VOLUME \
    "Speaker Playback Volume"

#define CTL_HEADPHONE_PLAYBACK_VOLUME \
    "Headphone Playback Volume"


/* ============================================================
 * MONO OUTPUT
 * ============================================================ */

#define CTL_MONO_LEFT \
    "Mono Output Mixer Left Switch"

#define CTL_MONO_RIGHT \
    "Mono Output Mixer Right Switch"


#endif /* CODEC_CONTROLS_H */
