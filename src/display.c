#include <display.h>
#include <SDL2/SDL.h>

SDL_Window* window;
SDL_Renderer* renderer;

int display_init() {
	// Because on most modern displays a 64x32 image would be very small and thus very hard to look at,
	// We make the dispay 8 times bigger.
	window = SDL_CreateWindow("Emul8tor", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 64*8, 32*8, SDL_WINDOW_SHOWN);
	if (window == NULL) {
		fprintf(stderr, "Could not create SDL window!\n");
		return -1;
	}
	
	renderer = SDL_CreateRenderer(window, -1, 0);
	if (renderer == NULL) {
		fprintf(stderr, "Could not create SDL renderer!\n");
		SDL_DestroyWindow(window);
		return -1;
	}

	return 0;
}

void display_update(chip8_t* chip8) {
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);

	SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);

	SDL_Rect rect;
	for (int i = 0; i < 32; i++) {
		for (int j = 0; j < 64; j++) {
			if (chip8->displayFB[i*64 + j]) {
				rect.x = j*8;			// 
				rect.y = i*8;			// Match the 1:8 pixel ratio
				rect.h = rect.w = 8;	//
				SDL_RenderFillRect(renderer, &rect);
			}
		}
	}

	SDL_RenderPresent(renderer);
}
