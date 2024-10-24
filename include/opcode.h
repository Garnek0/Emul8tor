#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <chip8.h>

void opcode_clear_screen(chip8_t* chip8);
int opcode_ret(chip8_t* chip8);
int opcode_jmp(chip8_t* chip8, uint16_t addr);
int opcode_call(chip8_t* chip8, uint16_t addr);
void opcode_se(chip8_t* chip8, uint8_t reg, uint8_t num);
void opcode_sne(chip8_t* chip8, uint8_t reg, uint8_t num);
void opcode_ld(chip8_t* chip8, uint8_t reg, uint8_t num);
void opcode_ldI(chip8_t* chip8, uint16_t addr);
void opcode_add(chip8_t* chip8, uint8_t reg, uint8_t num, bool carryFlag);
void opcode_or(chip8_t* chip8, uint8_t reg1, uint8_t reg2);
void opcode_and(chip8_t* chip8, uint8_t reg1, uint8_t reg2);
void opcode_xor(chip8_t* chip8, uint8_t reg1, uint8_t reg2);
void opcode_sub(chip8_t* chip8, uint8_t reg1, uint8_t reg2);
void opcode_subn(chip8_t* chip8, uint8_t reg1, uint8_t reg2);
void opcode_shr(chip8_t* chip8, uint8_t reg1, uint8_t reg2);
void opcode_shl(chip8_t* chip8, uint8_t reg1, uint8_t reg2);
void opcode_rnd(chip8_t* chip8, uint8_t reg, uint8_t num);
void opcode_drw(chip8_t* chip8, uint8_t x, uint8_t y, uint8_t h);
void opcode_skp(chip8_t* chip8, uint8_t reg, uint8_t* keyboardState);
void opcode_sknp(chip8_t* chip8, uint8_t reg, uint8_t* keyboardState);
void opcode_wfkp(chip8_t* chip8, uint8_t reg, uint8_t* keyboardState);
void opcode_sdt(chip8_t* chip8, uint8_t num);
void opcode_sst(chip8_t* chip8, uint8_t num);
void opcode_get_builtin_sprite(chip8_t* chip8, uint8_t spriteIdx);
void opcode_store_bcd(chip8_t* chip8, uint8_t bcdValue);
void opcode_store_regs(chip8_t* chip8, uint8_t regCount);
void opcode_load_regs(chip8_t* chip8, uint8_t regCount);
