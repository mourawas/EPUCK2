/**
 * @file     audio_processing.h
 * @brief    This source file contains function definitions for audio data processing.
**/

#ifndef AUDIO_PROCESSING_H
#define AUDIO_PROCESSING_H

#include <stdint.h>

/**
 * @brief Callback called when the demodulation of the four microphones is done.
 * We get 160 samples per mic every 10ms (16kHz)
 *
 * @param data Buffer containing 4 times 160 samples. The samples are sorted by mic
 * so we have [micRight1, micLeft1, micBack1, micFront1, micRight2, etc...]
 * @param num_samples Tells how many data we get in total (should always be 640)
**/
void processAudioData(int16_t *data, uint16_t num_samples);

/**
 * @brief Gets the difference in intensity between the front-left and front-right microphones.
 *
 * @return Difference in intensity between front-left and front-right.
**/
float audio_get_diff_intensity_front_left(void);

/**
 * @brief Gets the difference in intensity between the front-right and front-left microphones.
 *
 * @return Difference in intensity between front-right and front-left.
**/
float audio_get_diff_intensity_front_right(void);

#endif /* AUDIO_PROCESSING_H**/
