#include <sys/stat.h> // fstat
#include <fcntl.h> // open
#include <sys/mman.h> // mmap
#include <SDL2/SDL.h>
#include <time.h>

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
	printf("OUT %02x: %02x\n", port, data);
}

void drawFBToSDL(uint8_t *fb, uint8_t *sdlbuf) {
	// 7168 bytes to be read from the fb - 224x256 pixels, 1 bit per pixel

	uint8_t pgid = 0;
	for(int sdlx = 0;sdlx < 224;sdlx ++) {
		for(int sdly = 255;sdly >= 0;sdly --) {
			const int sdl_pixel_idx = sdly * 224 + sdlx;

			const uint8_t fbColor = ( (*fb) >> pgid ) & 1;
			pgid ++;

			if(pgid == 8) {
				fb ++;
				pgid = 0;
			}

			/* I thought this would be faster:
			 *
			 *    fb += (pgid >> 3);
			 *    pgid &= 0b0111;
			 *
			 * But it looks like my Ryzen 5 3500U is correctly predicting it
			 * can be skipped when there is branching.
			*/

			sdlbuf[sdl_pixel_idx * 4 + 0] = fbColor * 255;
			sdlbuf[sdl_pixel_idx * 4 + 1] = fbColor * 255;
			sdlbuf[sdl_pixel_idx * 4 + 2] = fbColor * 255;
			sdlbuf[sdl_pixel_idx * 4 + 3] = SDL_ALPHA_OPAQUE;
		}
	}
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
	uint8_t credit = 1, p1start = 1, p2start = 1;
	cpu.input_ports[0] = 0b00001110; // bits 1-3 should be always set
	cpu.input_ports[1] = 0b00001000; // bit 3 should be always set
	cpu.input_ports[2] = 0b00000000;

	cpu.input_ports[2] |= credit;
	cpu.input_ports[2] |= p1start << 2;
	cpu.input_ports[2] |= p2start << 1;


	uint8_t* memory = calloc(64000, sizeof(uint8_t));
	// load executable into memory
	memcpy(memory, bytecode, sb.st_size);

	// SDL
	SDL_Init(SDL_INIT_EVERYTHING);

	//const unsigned int texWidth = 256, texHeight = 224;
	const unsigned int texWidth = 224, texHeight = 256;
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
	/*
	printf("Texture formats:\n");
	for(int i = 0;i < info.num_texture_formats;i ++) {
		printf("%s\n", SDL_GetPixelFormatName(info.texture_formats[i]));
	}
	*/

	SDL_Texture* texture = SDL_CreateTexture(
		renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
		texWidth, texHeight
	);

	clock_t begin = clock();
	clock_t last_screen_interrupt = begin;
	uint8_t screen_interrupt_parity = 0;
	uint8_t debug = 0;
	while(getFlag(&cpu, HLT) == 0) {
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			switch(event.type) {
				case SDL_KEYDOWN:
					debug = !debug;
					printf("Key press detected: %d\n", event.key.keysym.scancode);
					break;

				case SDL_KEYUP:
					printf("Key release detected: %d\n", event.key.keysym.scancode);
					break;
				case SDL_WINDOWEVENT:
					if(event.window.event == SDL_WINDOWEVENT_CLOSE) {
						return 0;
					}
					break;
				default: break;
			}
		}

		execute_instruction(&cpu, memory, out);
		if(debug) {
		}

		if(cpu.instr % 1000 != 0) continue;

		clock_t now = clock();
		if(now - last_screen_interrupt > 1.0/60/2 * CLOCKS_PER_SEC) {
			last_screen_interrupt = now;
			screen_interrupt_parity ++;
			if(screen_interrupt_parity == 2) {
				request_interrupt(&cpu, memory, 2);
				screen_interrupt_parity = 0;

				SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
				SDL_RenderClear(renderer);

				uint8_t* lockedPixels;
				int pitch = 0;
				SDL_LockTexture(texture, NULL, (void **) &lockedPixels, &pitch);
				drawFBToSDL(memory + 0x2400, lockedPixels);

				SDL_UnlockTexture(texture);
				SDL_RenderCopy(renderer, texture, NULL, NULL);
				SDL_RenderPresent(renderer);
			}
		}
	}

	free(memory);
	//SDL_Delay(10000);

	SDL_DestroyWindow(window);
    SDL_Quit();
}
