#include <bits/time.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <SDL2/SDL.h>

#define CPU_INSTR_PER_TICK 10
#define TIMER_HZ 60
#define TIME_BETWEEN_TICKS (1000000 / TIMER_HZ)

#define BUFFER_DURATION 1
#define FREQ 48000
#define BUFFER_LEN (BUFFER_DURATION*FREQ)

typedef struct {
	uint8_t V[16];
	uint16_t I, SP, PC;
	uint8_t DT, ST;

	uint16_t stack[16];
	uint8_t displayFB[64*32];
	uint8_t memory[0x1000];

	uint8_t keyboardState[16];
} chip8_t;

static uint8_t buildinFont[80] = {
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80  // F 
};

void chip8_beeper_callback(void* userData, unsigned char* stream, int length);

SDL_AudioSpec audioSpec = {
	.freq = FREQ,
	.format = AUDIO_S8,
	.channels = 1,
	.samples = 1,
	.callback = chip8_beeper_callback,
	.userdata = NULL
};

SDL_AudioDeviceID audioDev;

SDL_Window* window;
SDL_Renderer* renderer;

chip8_t chip8;

int chip8_ret() {
	if (chip8.SP == 0)
		return -1;
	chip8.SP--;
	chip8.PC = chip8.stack[chip8.SP] + 2;

	// You may notice 2 is subtracted from the PC in instructions that change it.
	// This is because the chip8_execute function automatically adds 2 to the PC after
	// the instruction finishes executing, and in instructions like these we dont want our
	// PC to be changed (with one exception), so we cancel it out with a -2.
	chip8.PC -= 2;
	return 0;
}

int chip8_jmp(uint16_t addr) {
	if (addr >= 0x1000) {
		fprintf(stderr, "Jump address outside memory range.\n");
		return -1;
	}
	chip8.PC = addr - 2;
	return 0;
}

int chip8_call(uint16_t addr) {
	if (addr >= 0x1000) {
		fprintf(stderr, "Jump address outside memory range.\n");
		return -1;
	} else if (chip8.SP == 16) {
		fprintf(stderr, "Out of stack space.\n");
		return -1;
	}
	chip8.stack[chip8.SP] = chip8.PC;
	chip8.SP++;
	chip8.PC = addr - 2;
	return 0;
}

void chip8_add(uint8_t reg, uint8_t num, bool carryFlag) {
	if ((chip8.V[reg] + num) > 256) {
		// We need to simulate an overflow ourselves. (We cant overflow the register by simply adding
		// the value, because the C spec does not define this behaviour)
		chip8.V[reg] = num - (255-chip8.V[reg]) - 1; // Calculate overflow value
		if (carryFlag)
			chip8.V[0xF] = 1;
	} else {
		chip8.V[reg] += num;
		if (carryFlag)
			chip8.V[0xF] = 0;
	}	
}

void chip8_sub(uint8_t reg1, uint8_t reg2) {
	if (chip8.V[reg1] >= chip8.V[reg2]) {
		chip8.V[reg1] -= chip8.V[reg2]; 
		chip8.V[0xF] = 1;
	} else {
		chip8.V[reg1] = 0x100 - (chip8.V[reg2] - chip8.V[reg1]);
		chip8.V[0xF] = 0;
	}
}

void chip8_subn(uint8_t reg1, uint8_t reg2) {
	if (chip8.V[reg2] >= chip8.V[reg1]) {
		chip8.V[reg1] = chip8.V[reg2] - chip8.V[reg1];
		chip8.V[0xF] = 1;
	} else {
		chip8.V[reg1] = 0x100 - (chip8.V[reg1] - chip8.V[reg2]);
		chip8.V[0xF] = 0;
	}
}

void chip8_drw(uint8_t x, uint8_t y, uint8_t h) {
	chip8.V[0xF] = 0;

	for (int i = 0; i < h; i++) {
		uint8_t spriteRow = chip8.memory[chip8.I+i];
		for (int j = 0; j < 8; j++) {
			int dispIndex = ((x + j)%64) + (((y + i)%32)*64), currSprPixelState = ((spriteRow & (0b10000000 >> j))	>> (7-j));

			if (chip8.displayFB[dispIndex] && currSprPixelState)
				chip8.V[0xF] = 1;
			chip8.displayFB[dispIndex] ^= currSprPixelState;
		}
	}
}

void chip8_wait_for_keypress(uint8_t reg, uint8_t* keyboardState) {
	static int key = 0;
	static bool keyPressDetected = false;

	if (!keyPressDetected) {
		for (int i = 0; i < 16; i++) {
			if (keyboardState[i]) {
				keyPressDetected = true;
				key = i;
				break;
			}
		}
	} else {
		if (!keyboardState[key]) {
			keyPressDetected = false;
			chip8.V[reg] = key;
			return;
		}
	}
	// Execute this instruction again
	chip8.PC -= 2;
}

int chip8_display_init() {
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

void chip8_display_update() {
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);

	SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);

	SDL_Rect rect;
	for (int i = 0; i < 32; i++) {
		for (int j = 0; j < 64; j++) {
			if (chip8.displayFB[i*64 + j]) {
				rect.x = j*8;			// 
				rect.y = i*8;			// Match the 1:8 pixel ratio
				rect.h = rect.w = 8;	//
				SDL_RenderFillRect(renderer, &rect);
			}
		}
	}

	SDL_RenderPresent(renderer);
}

void chip8_beeper_callback(void* userData, unsigned char* stream, int length) {
	double phaseIncrement = 220.0 * 2.0 * M_PI / FREQ;	// Frequency is 220 Hz
	static double phase = 0.0;

	for (int i = 0; i < length; i++) {
		// Generate square wave: check if the sine of the current phase is positive or negative
		stream[i] = (int8_t)((sin(phase) > 0.0 ? 1.0 : -1.0) * 127 * 0.5);

		// Update phase and wrap it around 2pi (for a continuous square wave)
		phase += phaseIncrement;
		if (phase >= 2.0 * M_PI)
			phase -= 2.0 * M_PI;  // Keep phase within the 0 - 2pi range
	}
}

int chip8_beeper_init() {
	audioDev = SDL_OpenAudioDevice(NULL, 0, &audioSpec, NULL, 0);
	SDL_PauseAudioDevice(audioDev, 1);

	return 0;
}

int chip8_execute() {
	if (chip8.PC >= 0x1000) {
		fprintf(stderr, "PC address outside memory range.\n");
		return -1;
	}

	uint8_t nibble1 = (chip8.memory[chip8.PC] & 0xF0) >> 4;
	uint8_t nibble2 = (chip8.memory[chip8.PC] & 0x0F);
	uint8_t nibble3 = (chip8.memory[chip8.PC+1] & 0xF0) >> 4;
	uint8_t nibble4 = (chip8.memory[chip8.PC+1] & 0x0F);

	switch (nibble1) {
		case 0:
			if (nibble3 == 0xE && nibble4 == 0xE) {		//00EE - RET
				if (chip8_ret() != 0)
					return -1;
			} else if (nibble3 == 0xE) {				//00E0 - CLS
				// Zero out the framebuffer
				for (int i = 0; i < 32*64; i++)
					chip8.displayFB[i] = 0;	
			} else {
				return -1;
			}
			break;
		case 1:	// 1nnn - JMP 
			if (chip8_jmp(((nibble2 << 8) | (nibble3 << 4) | (nibble4))) != 0)
				return -1;
			break;
		case 2: // 2nnn - CALL
			if (chip8_call(((nibble2 << 8) | (nibble3 << 4) | (nibble4))) != 0)
				return -1;	
			break;
		case 3: // 3xkk - SE Vx, byte
			if (chip8.V[nibble2] == (((nibble3) << 4) | (nibble4)))
				chip8.PC += 2;
			break;
		case 4: // 4xkk - SNE Vx, byte
			if (chip8.V[nibble2] != ((nibble3 << 4) | (nibble4)))
				chip8.PC += 2;
			break;
		case 5: // 5xy0 - SE Vx, Vy
			if (chip8.V[nibble2] == chip8.V[nibble3])
				chip8.PC += 2;
			break;
		case 6: // 6xkk - LD Vx, byte
			chip8.V[nibble2] = ((nibble3 << 4) | (nibble4));
			break;
		case 7: // 7xkk - ADD Vx, byte
			chip8_add(nibble2, ((nibble3 << 4) | (nibble4)), false);
			break;
		case 8:
			if (nibble4 == 0) {		    	// 8xy0 - LD Vx, Vy
				chip8.V[nibble2] = chip8.V[nibble3];
			} else if (nibble4 == 1) {    	// 8xy1 - OR Vx, Vy
				chip8.V[nibble2] |= chip8.V[nibble3];
				chip8.V[0xF] = 0;
			} else if (nibble4 == 2) {    	// 8xy2 - AND Vx, Vy
				chip8.V[nibble2] &= chip8.V[nibble3];
				chip8.V[0xF] = 0;
			} else if (nibble4 == 3) {    	// 8xy3 - XOR Vx, Vy
				chip8.V[nibble2] ^= chip8.V[nibble3];
				chip8.V[0xF] = 0;
			} else if (nibble4 == 4) {    	// 8xy4 - ADD Vx, Vy
				chip8_add(nibble2, chip8.V[nibble3], true);
			} else if (nibble4 == 5) {    	// 8xy5 - SUB Vx, Vy
				chip8_sub(nibble2, nibble3);
			} else if (nibble4 == 6) {    	// 8xy6 - SHR Vx, {, Vy}
				uint8_t lsb = chip8.V[nibble3] & 1;
				chip8.V[nibble2] = chip8.V[nibble3]/2;
				chip8.V[0xF] = lsb;
			} else if (nibble4 == 7) {    	// 8xy7 - SUBN Vx, Vy
				chip8_subn(nibble2, nibble3);
			} else if (nibble4 == 0xE) {    // 8xyE - SHL Vx, {, Vy} 
				uint8_t msb = ((chip8.V[nibble3] >> 7) & 1);
				chip8.V[nibble2] = chip8.V[nibble3] * 2;
				chip8.V[0xF] = msb;

			} else {
				return -1;
			}
			break;
		case 9:	// 9xy0 - SNE Vx, Vy
			if (chip8.V[nibble2] != chip8.V[nibble3])
				chip8.PC += 2;
			break;
		case 0xA: // Annn - LD I, addr	
			chip8.I = ((nibble2 << 8) | (nibble3 << 4) | (nibble4));
			break;
		case 0xB: // Bnnn - JMP V0 + addr
			chip8_jmp(((nibble2 << 8) | (nibble3 << 4) | (nibble4)) + chip8.V[0]);
			break;
		case 0xC: // Cxkk - RND Vx, byte
			srand(time(NULL));
			chip8.V[nibble2] = ((rand() % 256) & ((nibble3 << 4) | (nibble4)));
			break;
		case 0xD: // Dxyn - DRW Vx, Vy, heigth
			chip8_drw(chip8.V[nibble2], chip8.V[nibble3], nibble4);
			break;
		case 0xE:
			if (nibble3 == 9 && nibble4 == 0xE) {			// Ex9E - SKP Vx
				if (chip8.keyboardState[chip8.V[(nibble2 & 0xF)]])
					chip8.PC += 2;
			} else if (nibble3 == 0xA && nibble4 == 1) {	// ExA1 - SKNP Vx
				if (!chip8.keyboardState[chip8.V[(nibble2 & 0xF)]])
					chip8.PC += 2;
			}
			break;
		case 0xF:
			if (nibble3 == 0 && nibble4 == 7) {             // Fx07 - LD Vx, DT
				chip8.V[nibble2] = chip8.DT;
			} else if (nibble3 == 0 && nibble4 == 0xA) {	// Fx0A - LD Vx, K
				chip8_wait_for_keypress(nibble2, chip8.keyboardState);
			} else if (nibble3 == 1 && nibble4 == 0x5) {	// Fx15 - LD DT, Vx
				chip8.DT = chip8.V[nibble2];
			} else if (nibble3 == 1 && nibble4 == 8) {		// Fx18 - LD ST, Vx
				chip8.ST = chip8.V[nibble2];
				SDL_PauseAudioDevice(audioDev, 0);
			} else if (nibble3 == 1 && nibble4 == 0xE) {	// Fx1E - ADD I, Vx
				chip8.I = chip8.I + chip8.V[nibble2];
			} else if (nibble3 == 2 && nibble4 == 9) {		// Fx29 - LD I, systemFont[Vx]
				chip8.I = 5*(chip8.V[nibble2] & 0xF);		
			} else if (nibble3 == 3 && nibble4 == 3) {		// Fx33 - LD [I], BCD(Vx)
				chip8.memory[chip8.I] = chip8.V[nibble2]/100%10;
				chip8.memory[chip8.I+1] = chip8.V[nibble2]/10%10;
				chip8.memory[chip8.I+2] = chip8.V[nibble2]%10;	
			} else if (nibble3 == 5 && nibble4 == 5) {		// Fx55 - LD [I], V0...Vx;
				memcpy((void*)&chip8.memory[chip8.I], (void*)chip8.V, nibble2+1);
				chip8.I += nibble2+1;	
			} else if (nibble3 == 6 && nibble4 == 5) {		// Fx55 - LD V0...Vx, [I]
				memcpy((void*)chip8.V, (void*)&chip8.memory[chip8.I], nibble2+1);
				chip8.I += nibble2+1;
			} else {
				return -1;
			}
			break;
		default:
			return -1;
	}
	chip8.PC += 2;
	
	return 0;
}

int chip8_init(const char* romPath) {
	chip8.PC = 0x200;

	// Copy the system font into memory, starting at address 0x00
	memcpy((void*)chip8.memory, (void*)buildinFont, 80);

	// Open the ROM file and make sure it fits in memory.
	FILE* romFile = fopen(romPath, "rb");
	if (!romFile) {
		fprintf(stderr, "Cound not open ROM: %s.\n", strerror(errno)); 
		return -1;
	}

	fseek(romFile, 0, SEEK_END);
	size_t romSize = ftell(romFile);
	fseek(romFile, 0, SEEK_SET);

	if (romSize > 0x1000 - 0x200) {
		fprintf(stderr, "ROM is too large. Max size is 3584 bytes.\n"); 
		return -1;
	}

	// Copy ROM contents to memory starting from initial PC address 0x200.
	if (fread((void*)&(chip8.memory[0x200]), 1, romSize, romFile) < romSize) {
		fprintf(stderr, "Could not copy the ROM contents to memory: %s.\n", strerror(errno));
		return -1;
	}

	if (chip8_display_init() != 0) {
		fprintf(stderr, "Failed to initialize display!\n");
		return -1;
	}

	if (chip8_beeper_init() != 0) {
		fprintf(stderr, "Failed to initialize beeper! Continuing with no audio...\n");
	}

	return 0;
}

int main(int argc, char** argv) {
	printf("Emul8tor v1.1 by Garnek0.\n");

	if (argc < 2) {
		fprintf(stderr, "No ROM specified.\nUsage: %s pathToROM\n", argv[0]);
		return 1;
	} else if (argc > 2) {
		fprintf(stderr, "Too many arguments.\nUsage: %s pathToROM\n", argv[0]);
		return 1;
	}

	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		fprintf(stderr, "Failed to initialize SDL!\n"); 
		SDL_Quit(); 
		return 1;
	}

	if (chip8_init(argv[1]) != 0) {
		SDL_Quit();
		return 1;
	}

	struct timespec startTime, endTime;
	time_t sleepTime;

	SDL_Event event;
	bool quit = false;
	while (!quit) {
		// Use the system clock for timing.
		clock_gettime(CLOCK_MONOTONIC, &startTime);

		for (size_t i = 0; i < CPU_INSTR_PER_TICK; i++) {
			while (SDL_PollEvent(&event)) {
				if (event.type == SDL_QUIT) {
					quit = true;
				} else if (event.type == SDL_KEYDOWN) {
					SDL_KeyboardEvent kbEvent = event.key;
					switch (kbEvent.keysym.sym) {
						case SDLK_x: chip8.keyboardState[0] = 1; break;
						case SDLK_1: chip8.keyboardState[1] = 1; break;
						case SDLK_2: chip8.keyboardState[2] = 1; break;
						case SDLK_3: chip8.keyboardState[3] = 1; break;
						case SDLK_q: chip8.keyboardState[4] = 1; break;
						case SDLK_w: chip8.keyboardState[5] = 1; break;
						case SDLK_e: chip8.keyboardState[6] = 1; break;
						case SDLK_a: chip8.keyboardState[7] = 1; break;
						case SDLK_s: chip8.keyboardState[8] = 1; break;
						case SDLK_d: chip8.keyboardState[9] = 1; break;
						case SDLK_z: chip8.keyboardState[10] = 1; break;
						case SDLK_c: chip8.keyboardState[11] = 1; break;
						case SDLK_4: chip8.keyboardState[12] = 1; break;
						case SDLK_r: chip8.keyboardState[13] = 1; break;
						case SDLK_f: chip8.keyboardState[14] = 1; break;
						case SDLK_v: chip8.keyboardState[15] = 1; break;
						default:
							break;
					}
				} else if (event.type == SDL_KEYUP) {
					SDL_KeyboardEvent kbEvent = event.key;
					switch (kbEvent.keysym.sym) {
						case SDLK_x: chip8.keyboardState[0] = 0; break;
						case SDLK_1: chip8.keyboardState[1] = 0; break;
						case SDLK_2: chip8.keyboardState[2] = 0; break;
						case SDLK_3: chip8.keyboardState[3] = 0; break;
						case SDLK_q: chip8.keyboardState[4] = 0; break;
						case SDLK_w: chip8.keyboardState[5] = 0; break;
						case SDLK_e: chip8.keyboardState[6] = 0; break;
						case SDLK_a: chip8.keyboardState[7] = 0; break;
						case SDLK_s: chip8.keyboardState[8] = 0; break;
						case SDLK_d: chip8.keyboardState[9] = 0; break;
						case SDLK_z: chip8.keyboardState[10] = 0; break;
						case SDLK_c: chip8.keyboardState[11] = 0; break;
						case SDLK_4: chip8.keyboardState[12] = 0; break;
						case SDLK_r: chip8.keyboardState[13] = 0; break;
						case SDLK_f: chip8.keyboardState[14] = 0; break;
						case SDLK_v: chip8.keyboardState[15] = 0; break;
						default:
							break;

					}
				}
			}

			if (chip8_execute() != 0) {
				printf("ERROR: Failed to execute instruction at 0x%x\n\n", chip8.PC);
				quit = true;
				break;
			}
		}

		clock_gettime(CLOCK_MONOTONIC, &endTime);
 
		sleepTime = TIME_BETWEEN_TICKS - (endTime.tv_sec - startTime.tv_sec) * 1000000 - (endTime.tv_nsec - startTime.tv_nsec) / 1000;

		// Do nothing for the duration of the remaining time quota.
		if (sleepTime > 0)
			usleep(sleepTime);
		
		// Decrement the delay and sound timers.
		if (chip8.DT > 0)
			chip8.DT--;
		
		if (chip8.ST > 0) {
			chip8.ST--;
			if (chip8.ST == 0)
				SDL_PauseAudioDevice(audioDev, 1);
		}

		// Update the display. This happens once every 60th of a second on the original CHIP-8 interpreter.
		chip8_display_update();	
	}
	
	SDL_Quit();

	return 0;
}
