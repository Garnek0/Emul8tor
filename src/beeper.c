#include <SDL_audio.h>
#include <SDL_stdinc.h>
#include <beeper.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

void beeper_callback(void* userData, unsigned char* stream, int length);

SDL_AudioSpec audioSpec = {
	.freq = FREQ,
	.format = AUDIO_S8,
	.channels = 1,
	.samples = 1,
	.callback = beeper_callback,
	.userdata = NULL
};

SDL_AudioDeviceID audioDev;

void beeper_callback(void* userData, unsigned char* stream, int length) {
	double phaseIncrement = 440.0 * 2.0 * M_PI / FREQ;  // Frequency is 440 Hz
	static double phase = 0.0;

    for (int i = 0; i < length; i++) {
        // Generate square wave: check if the sine of the current phase is positive or negative
        stream[i] = (int8_t)((sin(phase) > 0.0 ? 1.0 : -1.0) * 127 * 0.5);

        // Update phase and wrap it around 2pi (for a continuous square wave)
        phase += phaseIncrement;
        if (phase >= 2.0 * M_PI) {
            phase -= 2.0 * M_PI;  // Keep phase within the 0 - 2pi range
        }
    }
}

int beeper_init() {
	audioDev = SDL_OpenAudioDevice(NULL, 0, &audioSpec, NULL, 0);
	beeper_stop();

	return 0;
}

inline void beeper_play(){
	SDL_PauseAudioDevice(audioDev, 0);
}

inline void beeper_stop(){
	SDL_PauseAudioDevice(audioDev, 1);
}
