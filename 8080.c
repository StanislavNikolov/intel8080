// Reference: 4-1 in Intel's 8080 Microprocessor System User's Manual

//#include <sys/types.h>
#include <stdio.h> // printf
#include <stdlib.h> // exit()

#include "8080.h"

// get register pair
inline uint16_t rpBC(struct i8080* cpu) { return cpu->B << 8 | cpu->C; };
inline uint16_t rpDE(struct i8080* cpu) { return cpu->D << 8 | cpu->E; };
inline uint16_t rpHL(struct i8080* cpu) { return cpu->H << 8 | cpu->L; };
// set register pair
void srpBC(struct i8080* cpu, uint16_t val) { cpu->B = (val&0xFF00)>>8; cpu->C = val&0xFF; };
void srpDE(struct i8080* cpu, uint16_t val) { cpu->D = (val&0xFF00)>>8; cpu->E = val&0xFF; };
void srpHL(struct i8080* cpu, uint16_t val) { cpu->H = (val&0xFF00)>>8; cpu->L = val&0xFF; };

uint8_t getFlag(struct i8080 *cpu, enum flag flag) {
	return (cpu->flags >> flag) & 1;
}
void setFlag(struct i8080 *cpu, const uint8_t flag, const uint8_t val) {
	cpu->flags &= ~(1 << flag); // reset
	cpu->flags |= (1 << flag) * val; // set
}

void unimplemented(struct i8080 *cpu, uint8_t *memory) {
	printf("Unimplemented instruction, pc=%02x, mem[pc]=%02x\n", cpu->pc, memory[cpu->pc]);
	setFlag(cpu, HLT, 1);
	exit(1);
}

uint8_t parity(uint8_t val) {
	return ((val>>0)
	      ^ (val>>1)
	      ^ (val>>2)
	      ^ (val>>3)
	      ^ (val>>4)
	      ^ (val>>5)
	      ^ (val>>6)
	      ^ (val>>7)) & 1;
}

// RST 0 means "jump to 0x0", RST 1 means "vector to 0x8" and so on
#define RST(n) { \
		memory[cpu->sp-1] = (cpu->pc & 0xFF00) >> 8; \
		memory[cpu->sp-2] = (cpu->pc & 0x00FF); \
		cpu->sp -= 2; \
		cpu->pc = n*8; \
	}

void request_interrupt(struct i8080 *cpu, uint8_t *memory, uint8_t RST_n) {
	// if the CPU does not have interrupts enabled right now, ignore it
	//printf("Called %d\n", RST_n);
	if(!getFlag(cpu, EI)) return;

	//printf("LETS GOO\n");
	// disable interrupts
	setFlag(cpu, EI, 0);

	RST(RST_n);
}

void execute_instruction(struct i8080 *cpu, uint8_t *memory, void (*out)(uint8_t,uint8_t)) {
	// TODO many instructions do not set AC flag at all
#define SWAP(x,y) {x^=y;y^=x;x^=y;}
#define D16 (b[2] << 8 | b[1])
#define gBC rpBC(cpu)
#define gDE rpDE(cpu)
#define gHL rpHL(cpu)
#define sBC(x) srpBC(cpu, (x))
#define sDE(x) srpDE(cpu, (x))
#define sHL(x) srpHL(cpu, (x))

#define fZ(x) setFlag(cpu, Z, (x) == 0)
#define fS(x) setFlag(cpu, S, ((x)&0x80) >> 7)
#define fP(x) setFlag(cpu, P, !parity((x)))

#define DAD(x) {setFlag(cpu, CY, gHL > 0xFFFF - x); sHL(gHL + x);}

#define XRA(x) {cpu->A^=x; fZ(cpu->A); fS(cpu->A); fP(cpu->A); setFlag(cpu, CY, 0); setFlag(cpu, AC, 0);}
#define ANA(x) {cpu->A&=x; fZ(cpu->A); fS(cpu->A); fP(cpu->A); setFlag(cpu, CY, 0); /* TODO AC */}
#define ADD(x) {\
	setFlag(cpu, CY, cpu->A > 255 - x);\
	cpu->A += x;\
	fZ(cpu->A); fS(cpu->A); fP(cpu->A);\
}
#define ADC(x) {\
	const uint16_t sum = cpu->A + x + getFlag(cpu, CY); \
	setFlag(cpu, CY, sum > 255); \
	fZ(cpu->A); fS(cpu->A); fP(cpu->A); \
}

	uint8_t* b = memory + cpu->pc;
	// setFlag(cpu, CY, cpu->B == 0); // will result in a borrow

	uint8_t bit;
	switch(b[0]) {
/*NOP*/	case 0x00: /* do nothing :D */; cpu->pc += 1; break;
/*LXI*/	case 0x01: cpu->B = b[2]; cpu->C = b[1]; cpu->pc += 3; break;
/*STAX*/case 0x02: memory[gBC] = cpu->A; cpu->pc += 1; break;
/*INX*/	case 0x03: sBC(gBC+1); cpu->pc += 1; break;
		case 0x04: unimplemented(cpu, memory); cpu->pc += 1; break;
/*DCR*/	case 0x05: cpu->B --; fZ(cpu->B); fS(cpu->B); fP(cpu->B); cpu->pc += 1; break;
/*MVI*/	case 0x06: cpu->B = b[1]; cpu->pc += 2; break;
		case 0x07: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x08: unimplemented(cpu, memory); cpu->pc += 1; break;
/*DAD*/	case 0x09: DAD(gBC); cpu->pc += 1; break;
		case 0x0a: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x0b: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x0c: unimplemented(cpu, memory); cpu->pc += 1; break;
/*DCR*/	case 0x0d: cpu->C --; fZ(cpu->C); fS(cpu->C); fP(cpu->C); cpu->pc += 1; break;
/*MVI*/ case 0x0e: cpu->C = b[1]; cpu->pc += 2; break;
		case 0x0f:
			bit = cpu->A & 1;
			cpu->A >>= 1;
			cpu->A |= (bit << 8);
			setFlag(cpu, CY, bit);
			cpu->pc += 1;
			break;
/*DEB*/	case 0x10: printf("DEB\n"); cpu->pc += 1; break;
/*LXI*/	case 0x11: cpu->D = b[2]; cpu->E = b[1]; cpu->pc += 3; break;
		case 0x12: unimplemented(cpu, memory); cpu->pc += 1; break;
/*INX*/	case 0x13: sDE(gDE+1); cpu->pc += 1; break;
		case 0x14: unimplemented(cpu, memory); cpu->pc += 1; break;
/*DCR*/	case 0x15: cpu->D --; fZ(cpu->D); fS(cpu->D); fP(cpu->D); cpu->pc += 1; break;
/*MVI*/	case 0x16: cpu->D = b[1]; cpu->pc += 2; break;
		case 0x17: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x18: unimplemented(cpu, memory); cpu->pc += 1; break;
/*DAD*/	case 0x19: DAD(gDE); cpu->pc += 1; break;
/*LDAX*/case 0x1a: cpu->A = memory[gDE]; cpu->pc += 1; break;
		case 0x1b: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x1c: unimplemented(cpu, memory); cpu->pc += 1; break;
/*DCR*/	case 0x1d: cpu->E --; fZ(cpu->E); fS(cpu->E); fP(cpu->E); cpu->pc += 1; break;
/*MVI*/	case 0x1e: cpu->E = b[1]; cpu->pc += 2; break;
		case 0x1f: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x20: unimplemented(cpu, memory); cpu->pc += 1; break;
/*LXI*/	case 0x21: cpu->H = b[2]; cpu->L = b[1]; cpu->pc += 3; break;
		case 0x22: unimplemented(cpu, memory); cpu->pc += 3; break;
/*INX*/	case 0x23: sHL(gHL + 1); cpu->pc += 1; break;
		case 0x24: unimplemented(cpu, memory); cpu->pc += 1; break;
/*DCR*/	case 0x25: cpu->H --; fZ(cpu->H); fS(cpu->H); fP(cpu->H); cpu->pc += 1; break;
/*MVI*/	case 0x26: cpu->H = b[1]; cpu->pc += 2; break;
		case 0x27: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x28: unimplemented(cpu, memory); cpu->pc += 1; break;
/*DAD*/	case 0x29: DAD(gHL); cpu->pc += 1; break;
		case 0x2a: unimplemented(cpu, memory); cpu->pc += 3; break;
		case 0x2b: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x2c: unimplemented(cpu, memory); cpu->pc += 1; break;
/*DCR*/	case 0x2d: cpu->L --; fZ(cpu->L); fS(cpu->L); fP(cpu->L); cpu->pc += 1; break;
/*MVI*/	case 0x2e: cpu->L = b[1]; cpu->pc += 2; break;
		case 0x2f: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x30: unimplemented(cpu, memory); cpu->pc += 1; break;
/*LXI*/	case 0x31: cpu->sp = D16; cpu->pc += 3; break;
/*STA*/ case 0x32: memory[D16] = cpu->A; cpu->pc += 3; break;
/*INX*/	case 0x33: cpu->sp ++; cpu->pc += 1; break;
		case 0x34: unimplemented(cpu, memory); cpu->pc += 1; break;
/*DCR*/	case 0x35: memory[gHL] --; fZ(memory[gHL]); fS(memory[gHL]); fP(memory[gHL]); cpu->pc += 1; break;
/*MVI*/	case 0x36: memory[gHL] = b[1]; cpu->pc += 2; break;
		case 0x37: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x38: unimplemented(cpu, memory); cpu->pc += 1; break;
/*DAD*/	case 0x39: DAD(cpu->sp); cpu->pc += 1; break;
/*LDA*/	case 0x3a: cpu->A = memory[D16]; cpu->pc += 3; break;
		case 0x3b: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x3c: unimplemented(cpu, memory); cpu->pc += 1; break;
/*DCR*/	case 0x3d: cpu->A --; fZ(cpu->A); fS(cpu->A); fP(cpu->A); cpu->pc += 1; break;
/*MVI*/ case 0x3e: cpu->A = b[1]; cpu->pc += 2; break;
		case 0x3f: unimplemented(cpu, memory); cpu->pc += 1; break;

/* block of a lot of MOVs */

/*MOV*/	case 0x40: cpu->B = cpu->B;      cpu->pc += 1; break;
/*MOV*/	case 0x41: cpu->B = cpu->C;      cpu->pc += 1; break;
/*MOV*/	case 0x42: cpu->B = cpu->D;      cpu->pc += 1; break;
/*MOV*/	case 0x43: cpu->B = cpu->E;      cpu->pc += 1; break;
/*MOV*/	case 0x44: cpu->B = cpu->H;      cpu->pc += 1; break;
/*MOV*/	case 0x45: cpu->B = cpu->L;      cpu->pc += 1; break;
/*MOV*/	case 0x46: cpu->B = memory[gHL]; cpu->pc += 1; break;
/*MOV*/	case 0x47: cpu->B = cpu->A;      cpu->pc += 1; break;

/*MOV*/	case 0x48: cpu->C = cpu->B;      cpu->pc += 1; break;
/*MOV*/	case 0x49: cpu->C = cpu->C;      cpu->pc += 1; break;
/*MOV*/	case 0x4a: cpu->C = cpu->D;      cpu->pc += 1; break;
/*MOV*/	case 0x4b: cpu->C = cpu->E;      cpu->pc += 1; break;
/*MOV*/	case 0x4c: cpu->C = cpu->H;      cpu->pc += 1; break;
/*MOV*/	case 0x4d: cpu->C = cpu->L;      cpu->pc += 1; break;
/*MOV*/	case 0x4e: cpu->C = memory[gHL]; cpu->pc += 1; break;
/*MOV*/	case 0x4f: cpu->C = cpu->A;      cpu->pc += 1; break;

/*MOV*/	case 0x50: cpu->D = cpu->B;      cpu->pc += 1; break;
/*MOV*/	case 0x51: cpu->D = cpu->C;      cpu->pc += 1; break;
/*MOV*/	case 0x52: cpu->D = cpu->D;      cpu->pc += 1; break;
/*MOV*/	case 0x53: cpu->D = cpu->E;      cpu->pc += 1; break;
/*MOV*/	case 0x54: cpu->D = cpu->H;      cpu->pc += 1; break;
/*MOV*/	case 0x55: cpu->D = cpu->L;      cpu->pc += 1; break;
/*MOV*/	case 0x56: cpu->D = memory[gHL]; cpu->pc += 1; break;
/*MOV*/	case 0x57: cpu->D = cpu->A;      cpu->pc += 1; break;

/*MOV*/	case 0x58: cpu->E = cpu->B;      cpu->pc += 1; break;
/*MOV*/	case 0x59: cpu->E = cpu->C;      cpu->pc += 1; break;
/*MOV*/	case 0x5a: cpu->E = cpu->D;      cpu->pc += 1; break;
/*MOV*/	case 0x5b: cpu->E = cpu->E;      cpu->pc += 1; break;
/*MOV*/	case 0x5c: cpu->E = cpu->H;      cpu->pc += 1; break;
/*MOV*/	case 0x5d: cpu->E = cpu->L;      cpu->pc += 1; break;
/*MOV*/	case 0x5e: cpu->E = memory[gHL]; cpu->pc += 1; break;
/*MOV*/	case 0x5f: cpu->E = cpu->A;      cpu->pc += 1; break;

/*MOV*/	case 0x60: cpu->H = cpu->B;      cpu->pc += 1; break;
/*MOV*/	case 0x61: cpu->H = cpu->C;      cpu->pc += 1; break;
/*MOV*/	case 0x62: cpu->H = cpu->D;      cpu->pc += 1; break;
/*MOV*/	case 0x63: cpu->H = cpu->E;      cpu->pc += 1; break;
/*MOV*/	case 0x64: cpu->H = cpu->H;      cpu->pc += 1; break;
/*MOV*/	case 0x65: cpu->H = cpu->L;      cpu->pc += 1; break;
/*MOV*/	case 0x66: cpu->H = memory[gHL]; cpu->pc += 1; break;
/*MOV*/	case 0x67: cpu->H = cpu->A;      cpu->pc += 1; break;

/*MOV*/	case 0x68: cpu->L = cpu->B;      cpu->pc += 1; break;
/*MOV*/	case 0x69: cpu->L = cpu->C;      cpu->pc += 1; break;
/*MOV*/	case 0x6a: cpu->L = cpu->D;      cpu->pc += 1; break;
/*MOV*/	case 0x6b: cpu->L = cpu->E;      cpu->pc += 1; break;
/*MOV*/	case 0x6c: cpu->L = cpu->H;      cpu->pc += 1; break;
/*MOV*/	case 0x6d: cpu->L = cpu->L;      cpu->pc += 1; break;
/*MOV*/	case 0x6e: cpu->L = memory[gHL]; cpu->pc += 1; break;
/*MOV*/	case 0x6f: cpu->L = cpu->A;      cpu->pc += 1; break;

/*MOV*/	case 0x70: memory[gHL] = cpu->B; cpu->pc += 1; break;
/*MOV*/	case 0x71: memory[gHL] = cpu->C; cpu->pc += 1; break;
/*MOV*/	case 0x72: memory[gHL] = cpu->D; cpu->pc += 1; break;
/*MOV*/	case 0x73: memory[gHL] = cpu->E; cpu->pc += 1; break;
/*MOV*/	case 0x74: memory[gHL] = cpu->H; cpu->pc += 1; break;
/*MOV*/	case 0x75: memory[gHL] = cpu->L; cpu->pc += 1; break;
/*HLT*/ case 0x76: unimplemented(cpu, memory); cpu->pc += 1; break;
/*MOV*/	case 0x77: memory[gHL] = cpu->A; cpu->pc += 1; break;

/*MOV*/	case 0x78: cpu->A = cpu->B;      cpu->pc += 1; break;
/*MOV*/	case 0x79: cpu->A = cpu->C;      cpu->pc += 1; break;
/*MOV*/	case 0x7a: cpu->A = cpu->D;      cpu->pc += 1; break;
/*MOV*/	case 0x7b: cpu->A = cpu->E;      cpu->pc += 1; break;
/*MOV*/	case 0x7c: cpu->A = cpu->H;      cpu->pc += 1; break;
/*MOV*/	case 0x7d: cpu->A = cpu->L;      cpu->pc += 1; break;
/*MOV*/	case 0x7e: cpu->A = memory[gHL]; cpu->pc += 1; break;
/*MOV*/	case 0x7f: cpu->A = cpu->A;      cpu->pc += 1; break; // WTF why is this needed

/*ADD*/	case 0x80: ADD(cpu->B     ); cpu->pc += 1; break;
/*ADD*/	case 0x81: ADD(cpu->C     ); cpu->pc += 1; break;
/*ADD*/	case 0x82: ADD(cpu->D     ); cpu->pc += 1; break;
/*ADD*/	case 0x83: ADD(cpu->E     ); cpu->pc += 1; break;
/*ADD*/	case 0x84: ADD(cpu->H     ); cpu->pc += 1; break;
/*ADD*/	case 0x85: ADD(cpu->L     ); cpu->pc += 1; break;
/*ADD*/	case 0x86: ADD(memory[gHL]); cpu->pc += 1; break;
/*ADD*/	case 0x87: ADD(cpu->A     ); cpu->pc += 1; break;

/*ADC*/	case 0x88: ADC(cpu->B     ); cpu->pc += 1; break;
/*ADC*/	case 0x89: ADC(cpu->C     ); cpu->pc += 1; break;
/*ADC*/	case 0x8a: ADC(cpu->D     ); cpu->pc += 1; break;
/*ADC*/	case 0x8b: ADC(cpu->E     ); cpu->pc += 1; break;
/*ADC*/	case 0x8c: ADC(cpu->H     ); cpu->pc += 1; break;
/*ADC*/	case 0x8d: ADC(cpu->L     ); cpu->pc += 1; break;
/*ADC*/	case 0x8e: ADC(memory[gHL]); cpu->pc += 1; break;
/*ADC*/	case 0x8f: ADC(cpu->A     ); cpu->pc += 1; break;

		case 0x90: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x91: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x92: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x93: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x94: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x95: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x96: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x97: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x98: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x99: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x9a: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x9b: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x9c: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x9d: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x9e: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0x9f: unimplemented(cpu, memory); cpu->pc += 1; break;

/*ANA*/	case 0xa0: ANA(cpu->B     ); cpu->pc += 1; break;
/*ANA*/	case 0xa1: ANA(cpu->C     ); cpu->pc += 1; break;
/*ANA*/	case 0xa2: ANA(cpu->D     ); cpu->pc += 1; break;
/*ANA*/	case 0xa3: ANA(cpu->E     ); cpu->pc += 1; break;
/*ANA*/	case 0xa4: ANA(cpu->H     ); cpu->pc += 1; break;
/*ANA*/	case 0xa5: ANA(cpu->L     ); cpu->pc += 1; break;
/*ANA*/	case 0xa6: ANA(memory[gHL]); cpu->pc += 1; break;
/*ANA*/	case 0xa7: ANA(cpu->A     ); cpu->pc += 1; break;

/*XRA*/	case 0xa8: XRA(cpu->B     ); cpu->pc += 1; break;
/*XRA*/	case 0xa9: XRA(cpu->C     ); cpu->pc += 1; break;
/*XRA*/	case 0xaa: XRA(cpu->D     ); cpu->pc += 1; break;
/*XRA*/	case 0xab: XRA(cpu->E     ); cpu->pc += 1; break;
/*XRA*/	case 0xac: XRA(cpu->H     ); cpu->pc += 1; break;
/*XRA*/	case 0xad: XRA(cpu->L     ); cpu->pc += 1; break;
/*XRA*/	case 0xae: XRA(memory[gHL]); cpu->pc += 1; break;
/*XRA*/	case 0xaf: XRA(cpu->A     ); cpu->pc += 1; break;

		case 0xb0: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xb1: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xb2: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xb3: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xb4: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xb5: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xb6: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xb7: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xb8: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xb9: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xba: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xbb: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xbc: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xbd: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xbe: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xbf: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xc0: unimplemented(cpu, memory); cpu->pc += 1; break;
/*POP*/	case 0xc1: cpu->C=memory[cpu->sp]; cpu->B=memory[cpu->sp+1]; cpu->sp += 2; ; cpu->pc += 1; break;
/*JNZ*/	case 0xc2:
			if(!getFlag(cpu, Z)) { cpu->pc = D16; }
			else { cpu->pc += 3; }
		break;
/*JMP*/	case 0xc3: cpu->pc = D16; break;
		case 0xc4: unimplemented(cpu, memory); cpu->pc += 3; break;
/*PUSH*/case 0xc5:
			memory[cpu->sp-1] = cpu->B;
			memory[cpu->sp-2] = cpu->C;
			cpu->sp -= 2;
			cpu->pc += 1;
		break;
/*ADI*/	case 0xc6:
		setFlag(cpu, CY, cpu->A > 255 - b[1]);
		cpu->A += b[1];
		fZ(cpu->A); fS(cpu->A); fP(cpu->A);
		cpu->pc += 2;
		break;
/*RST*/	case 0xc7: RST(0); break;
/*RZ*/	case 0xc8: if(getFlag(cpu, Z) == 0) break; // else, waterfall to RET
/*RET*/	case 0xc9:
			cpu->pc = ((uint16_t)memory[cpu->sp+1] << 8) | memory[cpu->sp];
			cpu->sp += 2;
			break;
		case 0xca: unimplemented(cpu, memory); cpu->pc += 3; break;
		case 0xcb: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xcc: unimplemented(cpu, memory); cpu->pc += 3; break;
/*CALL*/case 0xcd:
			cpu->pc += 3; // if CALL saved its own address in the stack, RET would call again
			// leading to infinite recursion. Instead, call saves the address of the next instruction
			memory[cpu->sp-1] = (cpu->pc & 0xFF00) >> 8;
			memory[cpu->sp-2] = (cpu->pc & 0x00FF);
			cpu->sp -= 2;
			cpu->pc = D16;
		break;
		case 0xce: unimplemented(cpu, memory); cpu->pc += 2; break;
/*RST*/	case 0xcf: RST(1); break;
		case 0xd0: unimplemented(cpu, memory); cpu->pc += 1; break;
/*POP*/	case 0xd1: cpu->E=memory[cpu->sp]; cpu->D=memory[cpu->sp+1]; cpu->sp += 2; cpu->pc += 1; break;
		case 0xd2: unimplemented(cpu, memory); cpu->pc += 3; break;
/*OUT*/	case 0xd3: out(b[1], cpu->A); cpu->pc += 2; break;
		case 0xd4: unimplemented(cpu, memory); cpu->pc += 3; break;
/*PUSH*/case 0xd5:
			memory[cpu->sp-1] = cpu->D;
			memory[cpu->sp-2] = cpu->E;
			cpu->sp -= 2;
			cpu->pc += 1;
		break;
		case 0xd6: unimplemented(cpu, memory); cpu->pc += 2; break;
/*RST*/	case 0xd7: RST(2); break;
		case 0xd8: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xd9: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xda: unimplemented(cpu, memory); cpu->pc += 3; break;
/*IN*/	case 0xdb: printf("IN %02x\n", b[1]); cpu->A = cpu->input_ports[b[1]]; cpu->pc += 2; break;
		case 0xdc: unimplemented(cpu, memory); cpu->pc += 3; break;
		case 0xdd: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xde: unimplemented(cpu, memory); cpu->pc += 2; break;
/*RST*/	case 0xdf: RST(3); break;
		case 0xe0: unimplemented(cpu, memory); cpu->pc += 1; break;
/*POP*/	case 0xe1: cpu->L=memory[cpu->sp]; cpu->H=memory[cpu->sp+1]; cpu->sp += 2; cpu->pc += 1; break;
		case 0xe2: unimplemented(cpu, memory); cpu->pc += 3; break;
		case 0xe3: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xe4: unimplemented(cpu, memory); cpu->pc += 3; break;
/*PUSH*/case 0xe5:
			memory[cpu->sp-1] = cpu->H;
			memory[cpu->sp-2] = cpu->L;
			cpu->sp -= 2;
			cpu->pc += 1;
		break;
		case 0xe6: cpu->A &= b[1]; setFlag(cpu, CY, 0); cpu->pc += 2; break;
/*RST*/	case 0xe7: RST(4); break;
		case 0xe8: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xe9: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xea: unimplemented(cpu, memory); cpu->pc += 3; break;
/*XCHG*/case 0xeb: SWAP(cpu->H, cpu->D); SWAP(cpu->L, cpu->E); cpu->pc += 1; break;
		case 0xec: unimplemented(cpu, memory); cpu->pc += 3; break;
		case 0xed: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xee: unimplemented(cpu, memory); cpu->pc += 2; break;
/*RST*/	case 0xef: RST(5); break;
/*RP*/	case 0xf0:
			if(getFlag(cpu, P)) {
				printf("HERE %02x %02x\n", memory[cpu->sp+1], memory[cpu->sp]);
				cpu->pc = ((uint16_t)memory[cpu->sp+1] << 8) | memory[cpu->sp];
				cpu->sp += 2;
			} else {
				cpu->pc += 1;
			}
			break;
/*POP*/	case 0xf1: // POP PSW
			cpu->A = memory[cpu->sp+1];
			setFlag(cpu, CY, (memory[cpu->sp] >> 0) & 1);
			setFlag(cpu, P , (memory[cpu->sp] >> 2) & 1);
			setFlag(cpu, AC, (memory[cpu->sp] >> 4) & 1);
			setFlag(cpu, Z , (memory[cpu->sp] >> 6) & 1);
			setFlag(cpu, S , (memory[cpu->sp] >> 7) & 1);
			cpu->sp += 2;
			cpu->pc += 1;
			break;
		case 0xf2: unimplemented(cpu, memory); cpu->pc += 3; break;
/*DI*/	case 0xf3: setFlag(cpu, EI, 0); cpu->pc += 1; break;
		case 0xf4: unimplemented(cpu, memory); cpu->pc += 3; break;
/*PUSH*/case 0xf5: // PUSH PSW - saves flags into memory
			memory[cpu->sp-1] = cpu->A;
			memory[cpu->sp-2] = (getFlag(cpu, CY) << 0)
			                  | (0                << 1) // TODO should be 1 per intel docs
			                  | (getFlag(cpu, P ) << 2)
			                  | (0                << 3)
			                  | (getFlag(cpu, AC) << 4) // TODO change to getFlag(cpu, AC) when AC is working correctly
			                  | (0                << 5)
			                  | (getFlag(cpu, Z ) << 6)
			                  | (getFlag(cpu, S ) << 7);
			cpu->sp -= 2;
			cpu->pc += 1;
			break;
		case 0xf6: unimplemented(cpu, memory); cpu->pc += 2; break;
/*RST*/	case 0xf7: RST(6); break;
		case 0xf8: unimplemented(cpu, memory); cpu->pc += 1; break;
		case 0xf9: unimplemented(cpu, memory); cpu->pc += 1; break;
/*JM*/	case 0xfa: if(getFlag(cpu, S)) cpu->pc = D16; break;
/*EI*/	case 0xfb: setFlag(cpu, EI, 1); cpu->pc += 1; break;
		case 0xfc: unimplemented(cpu, memory); cpu->pc += 3; break;
		case 0xfd: unimplemented(cpu, memory); cpu->pc += 1; break;
/*CPI*/	case 0xfe:
			setFlag(cpu, CY, cpu->A < b[1]);
			const uint8_t tmp = cpu->A - b[1];
			fZ(tmp); fS(tmp); fP(tmp);
			cpu->pc += 2;
		break;
/*RST*/	case 0xff: RST(7); break;
	}
	cpu->instr ++;

#undef D16

}

