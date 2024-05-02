#include <ch.h>
#include <hal.h>
#include <usbcfg.h>
#include <chprintf.h>
#include <motors.h>
#include <audio/microphone.h>
#include <arm_math.h>

#include "audio_processing.h"
#include "main.h"
#include "fft.h"
#include "communications.h"

//semaphore
static BSEMAPHORE_DECL(sendToComputer_sem, TRUE);

//2 times FFT_SIZE because these arrays contain complex numbers (real + imaginary)
static float micLeft_cmplx_input[2 * FFT_SIZE];
static float micRight_cmplx_input[2 * FFT_SIZE];
static float micFront_cmplx_input[2 * FFT_SIZE];
static float micBack_cmplx_input[2 * FFT_SIZE];
//Arrays containing the computed magnitude of the complex numbers
static float micLeft_output[FFT_SIZE];
static float micRight_output[FFT_SIZE];
static float micFront_output[FFT_SIZE];
static float micBack_output[FFT_SIZE];

#define MIN_VALUE_THRESHOLD	10000 //minimum value to detect a peak

#define MIN_FREQ		10	//we don't analyze before this index to not waste resources
#define MAX_FREQ		30	//we don't analyze after this index to not waste resources

static float diff_intensity_avg_front_left = 0;
static float diff_intensity_avg_front_right = 0;

void find_direction(void){
	//FFT: 0-512 increasing positive frequencies,
	//513-1023 decreasing negative frequencies

	float avg_front_intensity = 0;
	float avg_back_intensity = 0; 
	float avg_right_intensity = 0;
	float avg_left_intensity = 0; 

	float *ptr_avg_front_intensity = &avg_front_intensity;
	// float *ptr_avg_back_intensity = &avg_back_intensity ;
	float *ptr_avg_right_intensity = &avg_right_intensity;
	float *ptr_avg_left_intensity = &avg_left_intensity ;
	
	calculate_average_intensity(micLeft_output, ptr_avg_left_intensity);
	calculate_average_intensity(micRight_output,ptr_avg_right_intensity);
	calculate_average_intensity(micFront_output,ptr_avg_front_intensity);
	// calculate_average_intensity(micBack_output, ptr_avg_back_intensity);
	
	diff_intensity_avg_front_left = *ptr_avg_front_intensity - *ptr_avg_left_intensity;
	diff_intensity_avg_front_right = *ptr_avg_front_intensity - *ptr_avg_right_intensity;
}

// void send_quadrant_to_computer(QUADRANT_NAME_t name, float avg_intensity_left, float  avg_intensity_right){
//     float diff = avg_intensity_left - avg_intensity_right;
//     chprintf((BaseSequentialStream *)&SD3, "Quadrant : %d, Left Micro Intensity : %f, Right Micro Intensity : %f, Intensity Difference : %f\n", name, avg_intensity_left, avg_intensity_right, diff);
// }

void calculate_average_intensity(float* buffer, float* average_value){
	
	//search for the highest peak
	for(uint16_t i = MIN_FREQ ; i <= MAX_FREQ ; i++){ //Band pass filter
		*average_value += buffer[i];
	} 
	*average_value = *average_value / (float)(MAX_FREQ - MIN_FREQ);
}

/*
*	Callback called when the demodulation of the four microphones is done.
*	We get 160 samples per mic every 10ms (16kHz)
*	
*	params :
*	int16_t *data			Buffer containing 4 times 160 samples. the samples are sorted by micro
*							so we have [micRight1, micLeft1, micBack1, micFront1, micRight2, etc...]
*	uint16_t num_samples	Tells how many data we get in total (should always be 640)
*/

void processAudioData(int16_t *data, uint16_t num_samples){

	/*
	*	We get 160 samples per mic every 10ms
	*	So we fill the samples buffers to reach
	*	1024 samples, then we compute the FFTs.
	*/

	static uint16_t nb_samples = 0;

	//loop to fill the buffers
	for(uint16_t i = 0 ; i < num_samples ; i+=4){

		//construct an array of complex numbers. Put 0 to the imaginary part
		micRight_cmplx_input[nb_samples] = (float)data[i + MIC_RIGHT];
		micLeft_cmplx_input[nb_samples] = (float)data[i + MIC_LEFT];
		micBack_cmplx_input[nb_samples] = (float)data[i + MIC_BACK];
		micFront_cmplx_input[nb_samples] = (float)data[i + MIC_FRONT];

		nb_samples++;

		micRight_cmplx_input[nb_samples] = 0;
		micLeft_cmplx_input[nb_samples] = 0;
		micBack_cmplx_input[nb_samples] = 0;
		micFront_cmplx_input[nb_samples] = 0;

		nb_samples++;

		//stop when buffer is full
		if(nb_samples >= (2 * FFT_SIZE)){
			break;
		}
	}

	if(nb_samples >= (2 * FFT_SIZE)){
		/*	FFT proccessing
		*
		*	This FFT function stores the results in the input buffer given.
		*	This is an "In Place" function. 
		*/

		doFFT_optimized(FFT_SIZE, micRight_cmplx_input);
		doFFT_optimized(FFT_SIZE, micLeft_cmplx_input);
		doFFT_optimized(FFT_SIZE, micFront_cmplx_input);
		doFFT_optimized(FFT_SIZE, micBack_cmplx_input);

		/*	Magnitude processing
		*	Computes the magnitude of the complex numbers and
		*	stores them in a buffer of FFT_SIZE because it only contains
		*	real numbers.
		*/
		arm_cmplx_mag_f32(micRight_cmplx_input, micRight_output, FFT_SIZE);
		arm_cmplx_mag_f32(micLeft_cmplx_input, micLeft_output, FFT_SIZE);
		arm_cmplx_mag_f32(micFront_cmplx_input, micFront_output, FFT_SIZE);
		arm_cmplx_mag_f32(micBack_cmplx_input, micBack_output, FFT_SIZE);

		//sends only one FFT result over 10 for 1 mic to not flood the computer
		//sends to UART3

		nb_samples = 0;

		//sound_remote(micLeft_output);
		find_direction();
	}
}

void wait_send_to_computer(void){
	chBSemWait(&sendToComputer_sem);
}

float* get_audio_buffer_ptr(BUFFER_NAME_t name){
	if(name == LEFT_CMPLX_INPUT){
		return micLeft_cmplx_input;
	}
	else if (name == RIGHT_CMPLX_INPUT){
		return micRight_cmplx_input;
	}
	else if (name == FRONT_CMPLX_INPUT){
		return micFront_cmplx_input;
	}
	else if (name == BACK_CMPLX_INPUT){
		return micBack_cmplx_input;
	}
	else if (name == LEFT_OUTPUT){
		return micLeft_output;
	}
	else if (name == RIGHT_OUTPUT){
		return micRight_output;
	}
	else if (name == FRONT_OUTPUT){
		return micFront_output;
	}
	else if (name == BACK_OUTPUT){
		return micBack_output;
	}
	else{
		return NULL;
	}
}

float audio_get_diff_intensity_front_left(void){
	return diff_intensity_avg_front_left;
}

float audio_get_diff_intensity_front_right(void){
	return diff_intensity_avg_front_right;
}