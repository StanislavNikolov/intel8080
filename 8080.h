#include <stdint.h> // uint8_t, uint16_t

struct i8080 {
	uint8_t A, B, C, D, E, H, L; // registers
	uint8_t flags;
	uint16_t sp, pc;
	int clock_cnt;//TODO
	int instr; // for debug purposes only
	uint8_t input_ports[256];
};

void execute_instruction(struct i8080 *cpu, uint8_t *memory, void (*out)(uint8_t,uint8_t));
void request_interrupt(struct i8080 *cpu, uint8_t *memory, uint8_t RST);

// for debugging purposes

enum flag {
	Z,  // zero
	S,  // sign
	P,  // parity
	CY, // carry
	AC, // accumulator
	EI, // interruputs enabled (not really a flag as per the 8080 manual)
	HLT, // whether or not is halted
};

uint8_t getFlag(struct i8080* cpu, enum flag flag);

// get register pair
uint16_t rpBC(struct i8080* cpu);
uint16_t rpDE(struct i8080* cpu);
uint16_t rpHL(struct i8080* cpu);

// set register pair
void srpBC(struct i8080* cpu, uint16_t val);
void srpDE(struct i8080* cpu, uint16_t val);
void srpHL(struct i8080* cpu, uint16_t val);
