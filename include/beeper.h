#pragma once

#include <SDL2/SDL.h>
#include <SDL_audio.h>

#define BUFFER_DURATION 1
#define FREQ 48000
#define BUFFER_LEN (BUFFER_DURATION*FREQ)

int beeper_init();
void beeper_play();
void beeper_stop();
