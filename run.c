#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> // fstat
#include <fcntl.h> // open
#include <sys/mman.h> // mmap

#include "8080.h"

void debugp(struct i8080* cpu, char* buff) {
	char flags[] = ".....";
	if(getFlag(cpu, Z )) flags[Z ] = 'Z';
	if(getFlag(cpu, S )) flags[S ] = 'S';
	if(getFlag(cpu, P )) flags[P ] = 'P';
	if(getFlag(cpu, CY)) flags[CY] = 'C';
	if(getFlag(cpu, AC)) flags[AC] = 'A';

	sprintf(buff, "A=%02x, BC=%04x, DE=%04x, HL=%04x, pc=%04x, sp=%04x, flags=%s\n",
		cpu->A, rpBC(cpu), rpDE(cpu), rpHL(cpu), cpu->pc, cpu->sp, flags);
}

void out(uint8_t port, uint8_t data) {
	printf("OUT on port %02x: %02x\n", port, data);
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

	// 0x5 is special print routine in CP/M
	memory[5] = 0x10; // DEB
	memory[6] = 0xc9; // RET

	// jump to 0x100
	memory[0]=0xc3;
	memory[1]=0;
	memory[2]=0x01;

	// fix SP
	memory[368] = 0x7;


	char* d8 = malloc(100);

	while(1) {
		execute_instruction(&cpu, memory, out);

		debugp(&cpu, d8);
		printf("%s", d8);
	}
}
