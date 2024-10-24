#include <SDL_audio.h>
#include <SDL_stdinc.h>
#include <beeper.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

static double phase = 0.0;

void _beeper_callback(void* userData, unsigned char* stream, int length);

SDL_AudioSpec spec = {
	.freq = FREQ,
	.format = AUDIO_S8,
	.channels = 1,
	.samples = 1,
	.callback = _beeper_callback,
	.userdata = NULL
};
SDL_AudioDeviceID dev;

void _beeper_callback(void* userData, unsigned char* stream, int length){
	double phaseIncrement = 440.0 * 2.0 * M_PI / FREQ;  // Frequency is 440 Hz	

    for(int i = 0; i < length; i++){	
        // Generate square wave: check if the sine of the current phase is positive or negative
        stream[i] = (int8_t)((sin(phase) > 0.0 ? 1.0 : -1.0) * 127 * 0.5);

        // Update phase and wrap it around 2pi (for a continuous square wave)
        phase += phaseIncrement;
        if(phase >= 2.0 * M_PI) {
            phase -= 2.0 * M_PI;  // Keep phase within the 0 - 2pi range
        }
    }
}

int beeper_init(){
	if(SDL_Init(SDL_INIT_AUDIO) != 0){
		fprintf(stderr, "Failed to initialize SDL audio!\n");
		SDL_Quit();
		return -1;
	}

	dev = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);

	beeper_stop();

	return 0;
}

inline void beeper_play(){
	SDL_PauseAudioDevice(dev, 0);
}

inline void beeper_stop(){
	SDL_PauseAudioDevice(dev, 1);
}
