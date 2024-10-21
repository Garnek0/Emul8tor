#include <SDL2/SDL_events.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <SDL2/SDL.h>

typedef struct {
	uint8_t V[16];
	uint16_t I;
	uint8_t SP;
	uint16_t PC;

	uint8_t DT;
	uint8_t ST;

	uint16_t stack[16];
	uint8_t displayFB[64*32];
	uint8_t memory[0x1000];
} chip8_t;

uint8_t buildinFont[80] = {
    // Sprite for '0'
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    // Sprite for '1'
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    // Sprite for '2'
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    // Sprite for '3'
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    // Sprite for '4'
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    // Sprite for '5'
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    // Sprite for '6'
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    // Sprite for '7'
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    // Sprite for '8'
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    // Sprite for '9'
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    // Sprite for 'A'
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    // Sprite for 'B'
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    // Sprite for 'C'
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    // Sprite for 'D'
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    // Sprite for 'E'
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    // Sprite for 'F'
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

uint8_t keyboardState[16];

SDL_Window* window;
SDL_Renderer* renderer;

int chip8_prepare_memory(chip8_t* chip8, const char* rompath){
	// Load Font
	memcpy((void*)chip8->memory, (void*)buildinFont, 80);

	// Load ROM
	int rom = open(rompath, 0);
	if(rom < 0){
		fprintf(stderr, "Cound not find specified ROM.\n");
		return -1;
	}

	size_t romSize = lseek(rom, 0, SEEK_END);
	if(romSize < 0) return -1;

	if(romSize > 0x1000 - 0x200){
		fprintf(stderr, "Specified ROM is too large.\n");
		return -1;
	}

	if(lseek(rom, 0, SEEK_SET) < 0) return -1;
	if(read(rom, (void*)&(chip8->memory[0x200]), romSize) < 0) return -1;

	return 0;
}

int chip8_execute(chip8_t* chip8){
	uint8_t nibble1 = (chip8->memory[chip8->PC] & 0xF0) >> 4;
	uint8_t nibble2 = (chip8->memory[chip8->PC] & 0x0F);
	uint8_t nibble3 = (chip8->memory[chip8->PC+1] & 0xF0) >> 4;
	uint8_t nibble4 = (chip8->memory[chip8->PC+1] & 0x0F);

	printf("0x%x (0x%x)\n", chip8->PC, ((chip8->memory[chip8->PC] << 8) | chip8->memory[chip8->PC+1]));

	printf("STATE:\n");
	printf("V0-VF: ");
	for(int i = 0; i < 16; i++){
		printf("0x%x ", chip8->V[i]);
	}
	printf("\n");
	printf("I: 0x%x\n", chip8->I);

	switch(nibble1){
		case 0:
		{
			if(nibble3 == 0xE && nibble4 == 0xE){
				if(chip8->SP == 0) return -1;
				chip8->SP--;
				chip8->PC = chip8->stack[chip8->SP] + 2;
				return 0;
			} else if(nibble3 == 0xE){
				for(int i = 0; i < 32*64; i++){
					chip8->displayFB[i] = 0;
				}	
			}
			break;
		}
		case 1:
		{
			chip8->PC = ((nibble2 << 8) | (nibble3 << 4) | (nibble4));
			return 0;
		}
		case 2:
		{
			chip8->stack[chip8->SP] = chip8->PC;
			chip8->PC = ((nibble2 << 8) | (nibble3 << 4) | (nibble4));
			chip8->SP++;
			return 0;
		}
		case 3:
		{
			if(chip8->V[nibble2] == ((nibble3 << 4) | (nibble4))) chip8->PC += 2;
			break;
		}
		case 4:
		{
			if(chip8->V[nibble2] != ((nibble3 << 4) | (nibble4))) chip8->PC += 2;
			break;
		}
		case 5:
		{
			if(chip8->V[nibble2] == chip8->V[nibble3]) chip8->PC += 2;
			break;
		}
		case 6:
		{
			chip8->V[nibble2] = ((nibble3 << 4) | (nibble4));
			break;
		}
		case 7:
		{
			if(chip8->V[nibble2] + ((nibble3 << 4) | (nibble4)) > 256) chip8->V[nibble2] = ((nibble3 << 4) | (nibble4)) - (255-chip8->V[nibble2]) - 1;
			else chip8->V[nibble2] = chip8->V[nibble2] + ((nibble3 << 4) | (nibble4));
			break;
		}
		case 8:
		{
			if(nibble4 == 0){
				chip8->V[nibble2] = chip8->V[nibble3];
			} else if(nibble4 == 1){
				chip8->V[nibble2] |= chip8->V[nibble3];
			} else if(nibble4 == 2){
				chip8->V[nibble2] &= chip8->V[nibble3];
			} else if(nibble4 == 3){
				chip8->V[nibble2] ^= chip8->V[nibble3];
			} else if(nibble4 == 4){
				if(chip8->V[nibble2] + chip8->V[nibble3] > 256){
					chip8->V[nibble2] = chip8->V[nibble3] - (255-chip8->V[nibble2]) - 1;
					chip8->V[0xF] = 1;
				} else {
					chip8->V[nibble2] = chip8->V[nibble2] + chip8->V[nibble3];
					chip8->V[0xF] = 0;
				}
			} else if(nibble4 == 5){
				if(chip8->V[nibble2] >= chip8->V[nibble3]){
					chip8->V[nibble2] -= chip8->V[nibble3];
					chip8->V[0xF] = 1;
				} else {
					chip8->V[nibble2] = 0x100 - (chip8->V[nibble3] - chip8->V[nibble2]);
					chip8->V[0xF] = 0;
				}
			} else if(nibble4 == 6){
				uint8_t lsb = chip8->V[nibble2] & 1;
				chip8->V[nibble2] /= 2;
				chip8->V[0xF] = lsb;
			} else if(nibble4 == 7){
				if(chip8->V[nibble3] >= chip8->V[nibble2]){
					chip8->V[nibble2] = chip8->V[nibble3] - chip8->V[nibble2];
					chip8->V[0xF] = 1;
				} else {
					chip8->V[nibble2] = 0x100 - (chip8->V[nibble2] - chip8->V[nibble3]);
					chip8->V[0xF] = 0;
				}	
			} else if(nibble4 == 0xE){
				uint8_t msb = ((chip8->V[nibble2] >> 7) & 1);
				chip8->V[nibble2] *= 2;
				chip8->V[0xF] = msb;
			} else {
				return -1;	
			}
			break;
		}
		case 9:
		{
			if(chip8->V[nibble2] != chip8->V[nibble3]) chip8->PC += 2;
			break;
		}
		case 0xA:
		{
			chip8->I = ((nibble2 << 8) | (nibble3 << 4) | (nibble4));
			break;
		}
		case 0xB:
		{
			chip8->PC = ((nibble2 << 8) | (nibble3 << 4) | (nibble4)) + chip8->V[0];
			return 0;
		}
		case 0xC:
		{
			srand(time(NULL));
			uint8_t random = rand() % 256;

			chip8->V[nibble2] = (random & ((nibble3 << 4) | (nibble4)));
			break;
		}
		case 0xD:
		{
			size_t x = chip8->V[nibble2];
			size_t y = chip8->V[nibble3];

			printf("Sprite Draw (from 0x%x):\n", chip8->I);
			printf("X: %d -> %d\n", x, x+8);
			printf("Y: %d -> %d\n", y, y+nibble4);

			chip8->V[0xF] = 0;

			for(int i = 0; i < nibble4; i++){
				if(y+i > 31) break;
				for(int j = 0; j < 8; j++){
					if(x+j > 63) break;
					if((chip8->displayFB[64*(y + i)+x+j] & 1) && (((chip8->memory[chip8->I+i] & (0b10000000 >> j)) >> (7-j)) & 1)) chip8->V[0xF] = 1;
					chip8->displayFB[64*(y + i)+x+j] ^= ((chip8->memory[chip8->I+i] & (0b10000000 >> j)) >> (7-j));
				}
			}

			break;
		}
		case 0xE:
		{
			if(nibble3 == 9 && nibble4 == 0xE){
				if(chip8->V[nibble2] > 0xF) return -1;
				if(keyboardState[chip8->V[nibble2]]) chip8->PC += 2;
			} else if(nibble3 == 0xA && nibble4 == 1){
				if(chip8->V[nibble2] > 0xF) return -1;
				if(!keyboardState[chip8->V[nibble2]]) chip8->PC += 2;
			}
			break;
		}
		case 0xF:
		{
			if(nibble3 == 0 && nibble4 == 7){
				chip8->V[nibble2] = chip8->DT;
			} else if(nibble3 == 0 && nibble4 == 0xA){
				return -1;
			} else if(nibble3 == 1 && nibble4 == 0x5){
				chip8->DT = chip8->V[nibble2];
			} else if(nibble3 == 1 && nibble4 == 8){
				chip8->ST = chip8->V[nibble2];
			} else if(nibble3 == 1 && nibble4 == 0xE){
				chip8->I += chip8->V[nibble2];
			} else if(nibble3 == 2 && nibble4 == 9){
				if(chip8->V[nibble2] > 0xF) return -1;
				chip8->I = 5*chip8->V[nibble2];
			} else if(nibble3 == 3 && nibble4 == 3){
				chip8->memory[chip8->I] = chip8->V[nibble2]/100%10;
				chip8->memory[chip8->I+1] = chip8->V[nibble2]/10%10;
				chip8->memory[chip8->I+2] = chip8->V[nibble2]%10;
			} else if(nibble3 == 5 && nibble4 == 5){
				memcpy((void*)&chip8->memory[chip8->I], (void*)chip8->V, 16);
			} else if(nibble3 == 6 && nibble4 == 5){
				memcpy((void*)chip8->V, (void*)&chip8->memory[chip8->I], 16);
			} else {
				return -1;
			}
			break;
		}
		default:
			return -1;
	}

	// Subtract 1 from DT/ST
	usleep(16660);
	if(chip8->DT > 0) chip8->DT--;
	if(chip8->ST > 0) chip8->ST--;
	
	chip8->PC += 2;
	
	return 0;
}

void chip8_display_update(chip8_t* chip8){
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);

	SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);

	SDL_Rect rect;

	for(int i = 0; i < 32; i++){
		for(int j = 0; j < 64; j++){
			if(chip8->displayFB[i*64 + j]){
				rect.x = j*4;
				rect.y = i*4;
				rect.h = rect.w = 4;
				SDL_RenderFillRect(renderer, &rect);
			}
		}
	}

	SDL_RenderPresent(renderer);
}

chip8_t* chip8_init(int argc, char** argv){
	if(argc < 2){
		fprintf(stderr, "No ROM specified.\n");
		return NULL;
	} else if(argc > 2){
		fprintf(stderr, "Too many arguments.\n");
		return NULL;
	}

	chip8_t* chip8 = malloc(sizeof(chip8_t));
	memset((void*)chip8, 0, sizeof(chip8_t));

	chip8->PC = 0x200;

	if(chip8_prepare_memory(chip8, argv[1]) != 0){
		fprintf(stderr, "Failed to initialize memory image!\n");
		return NULL;
	}

	return chip8;
}

int main(int argc, char** argv){
	if(SDL_Init(SDL_INIT_EVERYTHING) != 0){
		fprintf(stderr, "Failed to initialize SDL!\n");
		SDL_Quit();
		return 1;
	}	

	window = SDL_CreateWindow("Emul8tor", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 64*4, 32*4, SDL_WINDOW_SHOWN);
	if(window == NULL){
		fprintf(stderr, "Could not create SDL window!\n");
		SDL_Quit();
		return 1;
	}

	renderer = SDL_CreateRenderer(window, -1, 0);
	if(renderer == NULL){
		fprintf(stderr, "Could not create SDL renderer!\n");
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}


	chip8_t* chip8 = chip8_init(argc, argv);
	if(chip8 == NULL) return 1;

	SDL_Event event;
	bool quit = false;
	while(!quit){
		while(SDL_PollEvent(&event)){
			if(event.type == SDL_QUIT){
				quit = true;
			}

			if(event.type == SDL_KEYDOWN){
				SDL_KeyboardEvent kbEvent = event.key;
				switch(kbEvent.keysym.sym){
					case SDLK_0:
						keyboardState[0] = 1;
						break;
					case SDLK_1:
						keyboardState[1] = 1;
						break;
					case SDLK_2:
						keyboardState[2] = 1;
						break;
					case SDLK_3:
						keyboardState[3] = 1;
						break;
					case SDLK_4:
						keyboardState[4] = 1;
						break;
					case SDLK_5:
						keyboardState[5] = 1;
						break;
					case SDLK_6:
						keyboardState[6] = 1;
						break;
					case SDLK_7:
						keyboardState[7] = 1;
						break;
					case SDLK_8:
						keyboardState[8] = 1;
						break;
					case SDLK_9:
						keyboardState[9] = 1;
						break;
					case SDLK_a:
						keyboardState[10] = 1;
						break;
					case SDLK_b:
						keyboardState[11] = 1;
						break;
					case SDLK_c:
						keyboardState[12] = 1;
						break;
					case SDLK_d:
						keyboardState[13] = 1;
						break;
					case SDLK_e:
						keyboardState[14] = 1;
						break;
					case SDLK_f:
						keyboardState[15] = 1;
						break;
					default:
						break;
				}
			} else if(event.type == SDL_KEYUP){
				SDL_KeyboardEvent kbEvent = event.key;
				switch(kbEvent.keysym.sym){
					case SDLK_0:
						keyboardState[0] = 0;
						break;
					case SDLK_1:
						keyboardState[1] = 0;
						break;
					case SDLK_2:
						keyboardState[2] = 0;
						break;
					case SDLK_3:
						keyboardState[3] = 0;
						break;
					case SDLK_4:
						keyboardState[4] = 0;
						break;
					case SDLK_5:
						keyboardState[5] = 0;
						break;
					case SDLK_6:
						keyboardState[6] = 0;
						break;
					case SDLK_7:
						keyboardState[7] = 0;
						break;
					case SDLK_8:
						keyboardState[8] = 0;
						break;
					case SDLK_9:
						keyboardState[9] = 0;
						break;
					case SDLK_a:
						keyboardState[10] = 0;
						break;
					case SDLK_b:
						keyboardState[11] = 0;
						break;
					case SDLK_c:
						keyboardState[12] = 0;
						break;
					case SDLK_d:
						keyboardState[13] = 0;
						break;
					case SDLK_e:
						keyboardState[14] = 0;
						break;
					case SDLK_f:
						keyboardState[15] = 0;
						break;
					default:
						break;
				}
			}
		}

		if(chip8_execute(chip8) != 0){
			printf("ERROR: Failed to execute instruction at 0x%x\n\n", chip8->PC);

			// Dump memory and stack
			
			printf("Memory dump at PC:\n");
			for(int i = 0; i < 0x100; i++){	
				printf("0x%x ", chip8->memory[0x200 + i]);
				if(i%16==15) printf("\n");
			}

			printf("\n\nStack dump:\n");
			for(int i = 0; i < 16; i++){
				printf("0x%x ", chip8->stack[i]);
			}

			quit = true;
		}

		chip8_display_update(chip8);
	}
	
	free(chip8);
	SDL_Quit();

	return 0;
}
