#include <sys/stat.h> // fstat
#include <fcntl.h> // open
#include <sys/mman.h> // mmap

#include "8080.h"
#include "other.h"

void debugp(struct i8080* cpu) {
	char flags[] = ".....";
	if(getFlag(cpu, Z )) flags[Z ] = 'Z';
	if(getFlag(cpu, S )) flags[S ] = 'S';
	if(getFlag(cpu, P )) flags[P ] = 'P';
	if(getFlag(cpu, CY)) flags[CY] = 'C';
	if(getFlag(cpu, AC)) flags[AC] = 'A';

	printf("%04d) A=%02x, BC=%04x, DE=%04x, HL=%04x, pc=%04x, sp=%04x, flags=%s\n",
			cpu->instr, cpu->A, rpBC(cpu), rpDE(cpu), rpHL(cpu), cpu->pc, cpu->sp, flags);
}

void debugp_other(struct State8080* cpu) {
	char flags[] = ".....";
	if(cpu->cc.z ) flags[Z ] = 'Z';
	if(cpu->cc.s ) flags[S ] = 'S';
	if(cpu->cc.p ) flags[P ] = 'P';
	if(cpu->cc.cy) flags[CY] = 'C';
	if(cpu->cc.ac) flags[AC] = 'A';

	printf("other) A=%02x, BC=%02x%02x, DE=%02x%02x, HL=%02x%02x, pc=%04x, sp=%04x, flags=%s\n",
			cpu->a, cpu->b, cpu->c, cpu->d, cpu->e, cpu->h, cpu->l, cpu->pc, cpu->sp, flags);
}

void out(uint8_t port, uint8_t data) {
	printf("OUT %02x: %02x (%c)\n", port, data, data);
}

int main(int argc, char** argv) {
	if(argc < 2) {
		printf("Usage: %s ROM filename\n", argv[0]);
		return 1;
	}

	const int fd = open(argv[1], O_RDONLY);

	if(fd < 0) {
		printf("Failed to open %s\n", argv[1]);
		return 1;
	}

	struct stat sb;
	if(fstat(fd, &sb) == -1) {
		printf("Couldn't get file size of %s\n", argv[1]);
		return 1;
	}

	unsigned char* bytecode = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if(bytecode == MAP_FAILED) {
		printf("Couldn't mmap %s\n", argv[1]);
		return 1;
	}

	struct i8080 cpu;
	memset(&cpu, 0, sizeof(struct i8080));

	uint8_t* memory = calloc(64000, sizeof(uint8_t));
	// load executable into memory
	memcpy(memory, bytecode, sb.st_size);

	// other
	struct State8080 cpu_2;
	cpu_2.memory = calloc(64000, sizeof(uint8_t));
	memcpy(cpu_2.memory, bytecode, sb.st_size);

	while(1) {
		//printf("pc=%04x: mem[pc]={%02x,%02x,%02x}\n", cpu->pc, b[0], b[1], b[2]);
		//debugp(cpu);
		execute_instruction(&cpu, memory, out);
		//debugp(cpu);
		Emulate8080Op(&cpu_2);
		printf("===========\n");
		debugp(&cpu);
		debugp_other(&cpu_2);

		int diff = -1;
		for(int i = 0;i < 64000;i ++) {
			if(memory[i] != cpu_2.memory[i]) {
				diff = i;
				break;
			}
		}
		if(diff == -1 || diff == 0x23f6 /* emulator101 has mistake */) {
			printf("Memory is the same\n");
		} else {
			printf("Warn - memory different: %04x\n", diff);
			printf("%02x | %02x\n", memory[diff], cpu_2.memory[diff]);
			return 1;
		}
		//memcmp(memory, cpu_2.memory, 64000)
		// TODO emulate fb, input, interrupts
	}
}
