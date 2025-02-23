#include <chip8.h>
#include <display.h>
#include <opcode.h>
#include <beeper.h>
#include <bits/time.h>
#include <sys/stat.h>
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

uint8_t keyboardState[16];

int chip8_prepare_memory(chip8_t* chip8, const char* rompath){
	// Copy the system font into memory, starting at address 0
	memcpy((void*)chip8->memory, (void*)buildinFont, 80);

	// ROM Loading process...
	
	// Make sure the user provided a file.
	struct stat romStat;
	if(stat(rompath, &romStat) != 0){
		fprintf(stderr, "Cant stat ROM file: %s.\n", strerror(errno));
		return -1;
	}
	
	if(S_ISDIR(romStat.st_mode)){
		fprintf(stderr, "ROM must be a file, not a directory.\n");
		return -1;
	}

	// Open the ROM file and make sure it fits in memory.
	int romfd = open(rompath, 0);
	if(romfd < 0){
		fprintf(stderr, "Cound not open ROM: %s.\n", strerror(errno)); 
		return -1;
	}

	size_t romSize = romStat.st_size;

	if(romSize > 0x1000 - 0x200){
		fprintf(stderr, "ROM is too large. Max size is 2048 bytes.\n"); 
		return -1;
	}

	// Copy ROM contents to memory.
	if(read(romfd, (void*)&(chip8->memory[0x200]), romSize) < 0){
		fprintf(stderr, "Could not copy the ROM contents to memory: %s.\n", strerror(errno));
		return -1;
	}

	return 0;
}

int chip8_execute(chip8_t* chip8){
	if(chip8->PC >= 0x1000){
		fprintf(stderr, "PC out of memory range.\n");
		return -1;
	}

	uint8_t nibble1 = (chip8->memory[chip8->PC] & 0xF0) >> 4;
	uint8_t nibble2 = (chip8->memory[chip8->PC] & 0x0F);
	uint8_t nibble3 = (chip8->memory[chip8->PC+1] & 0xF0) >> 4;
	uint8_t nibble4 = (chip8->memory[chip8->PC+1] & 0x0F);

	switch(nibble1){
		case 0:
			if(nibble3 == 0xE && nibble4 == 0xE){		//00EE - RET
				if(opcode_ret(chip8) != 0) return -1;
			} else if(nibble3 == 0xE){					//00E0 - CLS
				opcode_clear_screen(chip8);	
			} else {
				return -1;
			}
			break;
		case 1:	// 1nnn - JMP 
			if(opcode_jmp(chip8, ((nibble2 << 8) | (nibble3 << 4) | (nibble4))) != 0) return -1;
			break;
		case 2: // 2nnn - CALL
			if(opcode_call(chip8, ((nibble2 << 8) | (nibble3 << 4) | (nibble4))) != 0) return -1;	
			break;
		case 3: // 3xkk - SE Vx, byte
			opcode_se(chip8, nibble2, ((nibble3 << 4) | (nibble4)));
			break;
		case 4: // 4xkk - SNE Vx, byte
			opcode_sne(chip8, nibble2, ((nibble3 << 4) | (nibble4)));
			break;
		case 5: // 5xy0 - SE Vx, Vy
			opcode_se(chip8, nibble2, chip8->V[nibble3]);
			break;
		case 6: // 6xkk - LD Vx, byte
			opcode_ld(chip8, nibble2, ((nibble3 << 4) | (nibble4)));
			break;
		case 7: // 7xkk - ADD Vx, byte
			opcode_add(chip8, nibble2, ((nibble3 << 4) | (nibble4)), false);
			break;
		case 8:
			if(nibble4 == 0){ 			// 8xy0 - LD Vx, Vy
				opcode_ld(chip8, nibble2, chip8->V[nibble3]);
			} else if(nibble4 == 1){	// 8xy1 - OR Vx, Vy
				opcode_or(chip8, nibble2, nibble3);
			} else if(nibble4 == 2){	// 8xy2 - AND Vx, Vy
				opcode_and(chip8, nibble2, nibble3);
			} else if(nibble4 == 3){	// 8xy3 - XOR Vx, Vy
				opcode_xor(chip8, nibble2, nibble3);
			} else if(nibble4 == 4){	// 8xy4 - ADD Vx, Vy
				opcode_add(chip8, nibble2, chip8->V[nibble3], true);
			} else if(nibble4 == 5){	// 8xy5 - SUB Vx, Vy
				opcode_sub(chip8, nibble2, nibble3);
			} else if(nibble4 == 6){	// 8xy6 - SHR Vx, {, Vy}
				opcode_shr(chip8, nibble2, nibble3);	
			} else if(nibble4 == 7){	// 8xy7 - SUBN Vx, Vy
				opcode_subn(chip8, nibble2, nibble3);
			} else if(nibble4 == 0xE){	// 8xyE - SHL Vx, {, Vy} 
				opcode_shl(chip8, nibble2, nibble3);		
			} else {
				return -1;	
			}
			break;
		case 9:	// 9xy0 - SNE Vx, Vy
			opcode_sne(chip8, nibble2, chip8->V[nibble3]);
			break;
		case 0xA: // Annn - LD I, addr	
			opcode_ldI(chip8, ((nibble2 << 8) | (nibble3 << 4) | (nibble4)));
			break;
		case 0xB: // Bnnn - JMP V0 + addr
			opcode_jmp(chip8, ((nibble2 << 8) | (nibble3 << 4) | (nibble4)) + chip8->V[0]);
			break;
		case 0xC: // Cxkk - RND Vx, byte
			opcode_rnd(chip8, nibble2, ((nibble3 << 4) | (nibble4)));
			break;
		case 0xD: // Dxyn - DRW Vx, Vy, heigth
			opcode_drw(chip8, chip8->V[nibble2], chip8->V[nibble3], nibble4);
			break;
		case 0xE:
			if(nibble3 == 9 && nibble4 == 0xE){			// Ex9E - SKP Vx
				opcode_skp(chip8, nibble2, keyboardState);
			} else if(nibble3 == 0xA && nibble4 == 1){	// ExA1 - SKNP Vx
				opcode_sknp(chip8, nibble2, keyboardState);
			}		
			break;
		case 0xF:
			if(nibble3 == 0 && nibble4 == 7){			// Fx07 - LD Vx, DT
				opcode_ld(chip8, nibble2, chip8->DT);
			} else if(nibble3 == 0 && nibble4 == 0xA){	// Fx0A - LD Vx, K
				opcode_wfkp(chip8, nibble2, keyboardState);
			} else if(nibble3 == 1 && nibble4 == 0x5){	// Fx15 - LD DT, Vx
				opcode_sdt(chip8, chip8->V[nibble2]);
			} else if(nibble3 == 1 && nibble4 == 8){	// Fx18 - LD ST, Vx
				opcode_sst(chip8, chip8->V[nibble2]);
			} else if(nibble3 == 1 && nibble4 == 0xE){	// Fx1E - ADD I, Vx
				opcode_ldI(chip8, chip8->I + chip8->V[nibble2]);
			} else if(nibble3 == 2 && nibble4 == 9){	// Fx29 - LD I, systemFont[Vx]
				opcode_get_builtin_sprite(chip8, chip8->V[nibble2]);		
			} else if(nibble3 == 3 && nibble4 == 3){	// Fx33 - LD [I], BCD(Vx)
				opcode_store_bcd(chip8, chip8->V[nibble2]);		
			} else if(nibble3 == 5 && nibble4 == 5){	// Fx55 - LD [I], V0...Vx;
				opcode_store_regs(chip8, nibble2);	
			} else if(nibble3 == 6 && nibble4 == 5){	// Fx55 - LD V0...Vx, [I]
				opcode_load_regs(chip8, nibble2);
			} else {
				return -1;
			}
			break;
		default:
			return -1;
	}
	chip8->PC += 2;
	
	return 0;
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

	if(display_init() != 0){
		fprintf(stderr, "Failed to initialize display!\n");
		return NULL;
	}

	if(beeper_init() != 0){
		fprintf(stderr, "Failed to initialize beeper! Continuing with no audio...\n");
	}

	return chip8;
}

int main(int argc, char** argv){
	printf("Emul8tor v1.0 by Garnek0.\n");

	if(SDL_Init(SDL_INIT_EVENTS) != 0){
		fprintf(stderr, "Failed to initialize SDL events!\n"); 
		SDL_Quit(); 
		return 1;
	}

	chip8_t* chip8 = chip8_init(argc, argv);
	if(chip8 == NULL) return 1;

	struct timespec startTime, endTime;
	time_t sleepTime;

	SDL_Event event;
	bool quit = false;
	while(!quit){
		// Use the system clock for timing.
		clock_gettime(CLOCK_MONOTONIC, &startTime);

		for(size_t i = 0; i < CYCLES_PER_TIMER_TICK; i++){
			while(SDL_PollEvent(&event)){
				if(event.type == SDL_QUIT) quit = true;
				else if(event.type == SDL_KEYDOWN){
					SDL_KeyboardEvent kbEvent = event.key;
					switch(kbEvent.keysym.sym){
						case SDLK_x: keyboardState[0] = 1; break;
						case SDLK_1: keyboardState[1] = 1; break;
						case SDLK_2: keyboardState[2] = 1; break;
						case SDLK_3: keyboardState[3] = 1; break;
						case SDLK_q: keyboardState[4] = 1; break;
						case SDLK_w: keyboardState[5] = 1; break;
						case SDLK_e: keyboardState[6] = 1; break;
						case SDLK_a: keyboardState[7] = 1; break;
						case SDLK_s: keyboardState[8] = 1; break;
						case SDLK_d: keyboardState[9] = 1; break;
						case SDLK_z: keyboardState[10] = 1; break;
						case SDLK_c: keyboardState[11] = 1; break;
						case SDLK_4: keyboardState[12] = 1; break;
						case SDLK_r: keyboardState[13] = 1; break;
						case SDLK_f: keyboardState[14] = 1; break;
						case SDLK_v: keyboardState[15] = 1; break;
						default: break;
					}
				} else if(event.type == SDL_KEYUP){
					SDL_KeyboardEvent kbEvent = event.key;
					switch(kbEvent.keysym.sym){
						case SDLK_x: keyboardState[0] = 0; break;
						case SDLK_1: keyboardState[1] = 0; break;
						case SDLK_2: keyboardState[2] = 0; break;
						case SDLK_3: keyboardState[3] = 0; break;
						case SDLK_q: keyboardState[4] = 0; break;
						case SDLK_w: keyboardState[5] = 0; break;
						case SDLK_e: keyboardState[6] = 0; break;
						case SDLK_a: keyboardState[7] = 0; break;
						case SDLK_s: keyboardState[8] = 0; break;
						case SDLK_d: keyboardState[9] = 0; break;
						case SDLK_z: keyboardState[10] = 0; break;
						case SDLK_c: keyboardState[11] = 0; break;
						case SDLK_4: keyboardState[12] = 0; break;
						case SDLK_r: keyboardState[13] = 0; break;
						case SDLK_f: keyboardState[14] = 0; break;
						case SDLK_v: keyboardState[15] = 0; break;
						default: break;

					}
				}
			}

			if(chip8_execute(chip8) != 0){
				printf("ERROR: Failed to execute instruction at 0x%x\n\n", chip8->PC);
				quit = true;
				break;
			}
		}

		clock_gettime(CLOCK_MONOTONIC, &endTime);
 
		sleepTime = TIME_BETWEEN_TICKS - (endTime.tv_sec - startTime.tv_sec) * 1000000 - (endTime.tv_nsec - startTime.tv_nsec) / 1000;

		// Do nothing for the duration of the remaining time quota.
		if(sleepTime > 0) usleep(sleepTime);

		// Decrement the delay and sound timers.
		if(chip8->DT > 0) chip8->DT--;
		if(chip8->ST > 0){
			chip8->ST--;
			if(chip8->ST == 0) beeper_stop();
		}

		// Update the display. This happens once every 60th of a second on the original CHIP-8 interpreter.
		display_update(chip8);	
	}
	
	free(chip8);
	SDL_Quit();

	return 0;
}
