#include <sys/stat.h> // fstat
#include <fcntl.h> // open
#include <sys/mman.h> // mmap
#include <SDL2/SDL.h>

#include "8080.h"

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
	cpu.halted = 0;

	uint8_t* memory = calloc(64000, sizeof(uint8_t));
	// load executable into memory
	memcpy(memory, bytecode, sb.st_size);

	// SDL
	SDL_Init(SDL_INIT_EVERYTHING);

	const unsigned int texWidth = 256, texHeight = 224;
	SDL_Window* window = SDL_CreateWindow(
		"space invaders emulator",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		texWidth, texHeight,
		SDL_WINDOW_SHOWN
	);

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	SDL_RendererInfo info;
	SDL_GetRendererInfo(renderer, &info);
	printf("Renderer name: %s\n", info.name);
	printf("Texture formats:\n");
	for(int i = 0;i < info.num_texture_formats;i ++) {
		printf("%s\n", SDL_GetPixelFormatName(info.texture_formats[i]));
	}

	SDL_Texture* texture = SDL_CreateTexture(
		renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
		texWidth, texHeight
	);

	while(cpu.halted == 0) {
		execute_instruction(&cpu, memory, out);

		//printf("%d\n", cpu.instr);
		if(cpu.instr % 1000 == 0) {
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
			SDL_RenderClear(renderer);

			uint8_t* lockedPixels;
			int pitch = 0;
			SDL_LockTexture(texture, NULL, (void **) &lockedPixels, &pitch);
			const int videoMemOffset = 0x2400;
			const int pixelGroupCnt = 0x3fff - videoMemOffset + 1;
			for(int pixelGroup = 0;pixelGroup <= pixelGroupCnt;pixelGroup ++) {
				for(int pgid = 0;pgid < 8;pgid ++) {
					const int texturePixelId = pixelGroup*8 + pgid;
					const int pixColor = (memory[videoMemOffset + pixelGroup] >> pgid) & 1;
					lockedPixels[texturePixelId*4 + 0] = pixColor * 255;
					lockedPixels[texturePixelId*4 + 1] = pixColor * 255;
					lockedPixels[texturePixelId*4 + 2] = pixColor * 255;
					lockedPixels[texturePixelId*4 + 3] = SDL_ALPHA_OPAQUE;
				}
			}
			SDL_UnlockTexture(texture);
			SDL_RenderCopy(renderer, texture, NULL, NULL);
			SDL_RenderPresent(renderer);
			SDL_Delay(10);
		}
		//debugp(&cpu);

		// TODO emulate input, interrupts
	}

	free(memory);
	SDL_Delay(10000);

	SDL_DestroyWindow(window);
    SDL_Quit();
}
