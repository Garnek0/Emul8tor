#include <chip8.h>
#include <beeper.h>
#include <opcode.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

inline void opcode_clear_screen(chip8_t* chip8){
	// Zero out the framebuffer
	for(int i = 0; i < 32*64; i++) chip8->displayFB[i] = 0;	
}

inline int opcode_ret(chip8_t* chip8){
	if(chip8->SP == 0) return -1;
	chip8->SP--;
	chip8->PC = chip8->stack[chip8->SP] + 2;

	// You may notice 2 is subtracted from the PC in instructions that change it.
	// This is because the chip8_execute function automatically adds 2 to the PC after
	// the instruction finishes executing, and in instructions like these we dont want our
	// PC to be changed (with one exception), so we zero it out with a -2.
	chip8->PC -= 2;
	return 0;
}

inline int opcode_jmp(chip8_t* chip8, uint16_t addr){
	if(addr >= 0x1000){
		fprintf(stderr, "Jump outside memory range.\n");
		return -1;
	}
	chip8->PC = addr;
	chip8->PC -= 2;
	return 0;
}

inline int opcode_call(chip8_t* chip8, uint16_t addr){
	if(addr >= 0x1000){
		fprintf(stderr, "Jump outside memory range.\n");
		return -1;
	} else if(chip8->SP == 16){
		fprintf(stderr, "Out of stack space.\n");
		return -1;
	}
	chip8->stack[chip8->SP] = chip8->PC;
	chip8->SP++;
	chip8->PC = addr;
	chip8->PC -= 2;
	return 0;
}

inline void opcode_se(chip8_t* chip8, uint8_t reg, uint8_t num){
	if(chip8->V[reg] == num) chip8->PC += 2;
}

inline void opcode_sne(chip8_t* chip8, uint8_t reg, uint8_t num){
	if(chip8->V[reg] != num) chip8->PC += 2;
}

inline void opcode_ld(chip8_t* chip8, uint8_t reg, uint8_t num){
	chip8->V[reg] = num;
}

inline void opcode_ldI(chip8_t* chip8, uint16_t addr){
	chip8->I = addr;
}

inline void opcode_add(chip8_t* chip8, uint8_t reg, uint8_t num, bool carryFlag){
	if((chip8->V[reg] + num) > 256){
		// We need to simulate an overflow ourselves. (We cant overflow the register by simply adding
		// the value, because the C spec does not define this behaviour)
		chip8->V[reg] = num - (255-chip8->V[reg]) - 1; // Calculate overflow value
		if(carryFlag) chip8->V[0xF] = 1;
	} else{
		chip8->V[reg] += num;
		if(carryFlag) chip8->V[0xF] = 0;
	}	
}

inline void opcode_or(chip8_t* chip8, uint8_t reg1, uint8_t reg2){
	chip8->V[reg1] |= chip8->V[reg2];
	chip8->V[0xF] = 0;
}

inline void opcode_and(chip8_t* chip8, uint8_t reg1, uint8_t reg2){
	chip8->V[reg1] &= chip8->V[reg2];
	chip8->V[0xF] = 0;
}

inline void opcode_xor(chip8_t* chip8, uint8_t reg1, uint8_t reg2){
	chip8->V[reg1] ^= chip8->V[reg2];
	chip8->V[0xF] = 0;
}

inline void opcode_sub(chip8_t* chip8, uint8_t reg1, uint8_t reg2){
	if(chip8->V[reg1] >= chip8->V[reg2]){
		chip8->V[reg1] -= chip8->V[reg2]; 
		chip8->V[0xF] = 1;
	} else {
		chip8->V[reg1] = 0x100 - (chip8->V[reg2] - chip8->V[reg1]);
		chip8->V[0xF] = 0;
	}
}

inline void opcode_subn(chip8_t* chip8, uint8_t reg1, uint8_t reg2){
	if(chip8->V[reg2] >= chip8->V[reg1]){
		chip8->V[reg1] = chip8->V[reg2] - chip8->V[reg1];
		chip8->V[0xF] = 1;
	} else {
		chip8->V[reg1] = 0x100 - (chip8->V[reg1] - chip8->V[reg2]);
		chip8->V[0xF] = 0;
	}
}

inline void opcode_shr(chip8_t* chip8, uint8_t reg1, uint8_t reg2){
	uint8_t lsb = chip8->V[reg2] & 1;
	chip8->V[reg1] = chip8->V[reg2]/2;
	chip8->V[0xF] = lsb;
}

inline void opcode_shl(chip8_t* chip8, uint8_t reg1, uint8_t reg2){
	uint8_t msb = ((chip8->V[reg2] >> 7) & 1);
	chip8->V[reg1] = chip8->V[reg2] * 2;
	chip8->V[0xF] = msb;
}

inline void opcode_rnd(chip8_t* chip8, uint8_t reg, uint8_t num){
	srand(time(NULL));
	uint8_t random = rand() % 256;

	chip8->V[reg] = (random & num);
}

inline void opcode_drw(chip8_t* chip8, uint8_t x, uint8_t y, uint8_t h){
	chip8->V[0xF] = 0;

	for(int i = 0; i < h; i++){
		uint8_t spriteRow = chip8->memory[chip8->I+i];
		for(int j = 0; j < 8; j++){
		int dispIndex = ((x + j)%64) + (((y + i)%32)*64), currSprPixelState = ((spriteRow & (0b10000000 >> j))  >> (7-j));

		if(chip8->displayFB[dispIndex] && currSprPixelState) chip8->V[0xF] = 1;
			chip8->displayFB[dispIndex] ^= currSprPixelState;
		}
	}
}

inline void opcode_skp(chip8_t* chip8, uint8_t reg, uint8_t* keyboardState){	
	if(keyboardState[chip8->V[(reg & 0xF)]]) chip8->PC += 2;
}

inline void opcode_sknp(chip8_t* chip8, uint8_t reg, uint8_t* keyboardState){
	if(!keyboardState[chip8->V[(reg & 0xF)]]) chip8->PC += 2;
}

static int key = 0;
static bool keyPressDetected = false;
inline void opcode_wfkp(chip8_t* chip8, uint8_t reg, uint8_t* keyboardState){
	if(!keyPressDetected){
		for(int i = 0; i < 16; i++){
			if(keyboardState[i]){
				keyPressDetected = true;
				key = i;
				break;
			}
		}
	} else {
		if(!keyboardState[key]){
			keyPressDetected = false;
			chip8->V[reg] = key;
			return;
		}
	}
	// Execute this instruction again
	chip8->PC -= 2;
}

inline void opcode_sdt(chip8_t* chip8, uint8_t num){
	chip8->DT = num;
}

inline void opcode_sst(chip8_t* chip8, uint8_t num){
	chip8->ST = num;
	beeper_play();
}

inline void opcode_get_builtin_sprite(chip8_t* chip8, uint8_t spriteIdx){
	chip8->I = 5*(spriteIdx & 0xF);
}

inline void opcode_store_bcd(chip8_t* chip8, uint8_t bcdValue){
	chip8->memory[chip8->I] = bcdValue/100%10;
	chip8->memory[chip8->I+1] = bcdValue/10%10;
	chip8->memory[chip8->I+2] = bcdValue%10;
}

inline void opcode_store_regs(chip8_t* chip8, uint8_t regCount){
	memcpy((void*)&chip8->memory[chip8->I], (void*)chip8->V, regCount+1);
	chip8->I += regCount+1;
}

inline void opcode_load_regs(chip8_t* chip8, uint8_t regCount){
	memcpy((void*)chip8->V, (void*)&chip8->memory[chip8->I], regCount+1);
	chip8->I += regCount+1;
}
