#include <stdio.h>
#include <time.h>
#include <SDL2/SDL.h>

typedef enum {
	QUIT,
	RUNNING,
	PAUSE
} emu_state_t;

typedef struct {
	uint32_t windowWidth;	//	Emulator window width
	uint32_t windowHeight;	//	Emulator window height
	uint32_t fgColor;		// 	Foreground Color RGBA8888 
	uint32_t bgColor; 		//	Backgorund Color RGBA8888
	int32_t scaleFactor;	// 	Amount to scale chip8 pixel 
	uint32_t instPerSec; 	// CHip8 CPU Clockrate
} config_t;

// CHIP8 instruction format
typedef struct {
	uint16_t opcode;	
	uint16_t NNN;		// 12 bit constant
	uint8_t NN;			// 8 bit contsant
	uint8_t N;			// 4 bit contsant
	uint8_t X;			// 4 bit register identifier
	uint8_t Y;			// 4 bit register identifier
} instruction_t;	

// CHIP8 Machine object
typedef struct {
	emu_state_t state;
	uint8_t memory[4096];
	//Emulate original chip8 pixels
	bool display[64*32];	// Could be a boolean pointer and dynamically alloacte for different resolutions(super chip)
	uint16_t stack[12];		// Subroutine stack
	uint8_t V[16];			// Data registers
	bool keys[16];			// Hexadecimal keypad 0x0-0xF

	uint16_t pc;			// Program counter
	uint16_t I;				// Index register
	uint16_t sp;			// Stack pointer
	uint8_t delay_timer;	// Decrements at 60hz when >0
	uint8_t sound_timer;	// Decrements at 60hz and plays tone when >0
	instruction_t inst;		// currently executing instruction

	char *romName;			// Currently running rom filepath

} chip8_t;

// setup emulator config from passed in args
bool set_config_from_args(config_t* config, int argc, char **argv);

void updateScreen(SDL_Renderer* renderer, const chip8_t *chip8, const config_t *config);
void handleInput(chip8_t *chip8);
void emulateInstruction(chip8_t *chip8, const config_t *config);
void updateTimers(chip8_t *chip8);
void printDebugInfo(chip8_t *chip8);

int main(int argc, char **argv) {
	// Initialize chip8
	/*****************************************************************************************************************************/
	chip8_t chip8 = {};
	memset(&chip8, 0, sizeof(chip8_t));

	const uint32_t entryPoint = 0x200; // Roms loaded into 0x200

	// Load Font
	const uint8_t font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0,   // 0   
        0x20, 0x60, 0x20, 0x20, 0x70,   // 1  
        0xF0, 0x10, 0xF0, 0x80, 0xF0,   // 2 
        0xF0, 0x10, 0xF0, 0x10, 0xF0,   // 3
        0x90, 0x90, 0xF0, 0x10, 0x10,   // 4    
        0xF0, 0x80, 0xF0, 0x10, 0xF0,   // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0,   // 6
        0xF0, 0x10, 0x20, 0x40, 0x40,   // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0,   // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0,   // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90,   // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0,   // B
        0xF0, 0x80, 0x80, 0x80, 0xF0,   // C
        0xE0, 0x90, 0x90, 0x90, 0xE0,   // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0,   // E
        0xF0, 0x80, 0xF0, 0x80, 0x80,   // F
    };
	memcpy(&chip8.memory[0], font, sizeof(font));

	// Load ROM
	// TODO: Define romName
	chip8.romName = argv[1];
	FILE *rom = fopen(chip8.romName, "rb");
	if (!rom) {
		SDL_Log("Romfile %s is invalid or does not exist\n", chip8.romName);
		return 1;
	}
	fseek(rom, 0, SEEK_END);		// Go to end of file
	const size_t romSize = ftell(rom);// Get number of bytes we need read into memory
	const size_t maxSize = sizeof chip8.memory - entryPoint;
	rewind(rom);					// Go back to behgining of file 
	if (romSize > maxSize) {
		SDL_Log("Romfile %s is too big! Rom size: %zu\nMax size allowed: %zu\n", chip8.romName, romSize, maxSize);
		return 1;
	}
	if (fread(&chip8.memory[entryPoint], romSize, 1, rom) != 1) {
		SDL_Log("Could not read Rom file %s into memory\n", chip8.romName);
		return 1;
	}
	fclose(rom);


	chip8.state = RUNNING;
	chip8.pc = entryPoint;
	chip8.sp = 0;

	/*****************************************************************************************************************************/
	// Set configs
	config_t config;
	if (!set_config_from_args(&config, argc, argv)) exit(EXIT_FAILURE);
	if(argc < 2)
	{
		printf("Usage: myChip8.exe chip8application\n");
	}
	// Init rand
	srand(time(NULL));
		

	// Setup SDL
	/*------------------------------------------------------------------------------------------------*/
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		SDL_Log("Unable to initialize SDL: %s\n", SDL_GetError());
		return 1;
	}
	SDL_Window *window = SDL_CreateWindow("Chip-8 by Marcos Espino", 
											SDL_WINDOWPOS_CENTERED, 
											SDL_WINDOWPOS_CENTERED, 
											config.windowWidth * config.scaleFactor, 
											config.windowHeight * config.scaleFactor, 
											0);
											if (!window) {
	SDL_Log("Could not create window %s\n", SDL_GetError());
		return 1;
	}
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer) {
		SDL_Log("Could not create renderer %s\n", SDL_GetError());
		return 1;
	}
	// Initial screen clear 
	const uint8_t r = (config.bgColor >> 24) & 0xFF;
	const uint8_t g = (config.bgColor >> 16) & 0xFF;
	const uint8_t b = (config.bgColor >> 8) & 0xFF;
	const uint8_t a = (config.bgColor >> 0) & 0xFF;

	SDL_SetRenderDrawColor(renderer, r,g,b,a);
	SDL_RenderClear(renderer);
	/*------------------------------------------------------------------------------------------------*/

	// Main emulator loop
	while (chip8.state != QUIT) {

		// Handle userinput
		handleInput(&chip8);


		// Get time() before running inst
		const uint64_t startFrameTime = SDL_GetPerformanceCounter();

		for (uint32_t i = 0; i < config.instPerSec / 60; i++)
			emulateInstruction(&chip8, &config);

		const uint64_t endFrameTime = SDL_GetPerformanceCounter();

		const double timeElapsed = (double)((endFrameTime - startFrameTime) * 1000) / SDL_GetPerformanceFrequency();
		// printf("Time Elapsed: %f\n", timeElapsed);
		// printf("PF: %f\n", SDL_GetPerformanceFrequency());
		// 60fps = 16.67
		// 30fps = 33.34
		// 15fps = 66.68
		SDL_Delay(16.67f > timeElapsed ? 16.67f - timeElapsed : 0);

		// update window with changes on each iteration
		updateScreen(renderer, &chip8, &config);
		updateTimers(&chip8);
	}

	// Shut down SDL
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	printf("Success!!\n");
	return 0;
}


void updateScreen(SDL_Renderer* renderer, const chip8_t *chip8, const config_t *config) {
	// Draw each pixel as a rect
	SDL_Rect rect = {.x = 0, .y = 0, .w = config->scaleFactor, .h = config->scaleFactor};

	// Grab color values to draw
	const uint8_t fgR = (config->fgColor >> 24) & 0xFF;
	const uint8_t fgG = (config->fgColor >> 16) & 0xFF; 
	const uint8_t fgB = (config->fgColor >>  8) & 0xFF; 
	const uint8_t fgA = (config->fgColor >>  0) & 0xFF;

	const uint8_t bgR = (config->bgColor >> 24) & 0xFF;
	const uint8_t bgG = (config->bgColor >> 16) & 0xFF; 
	const uint8_t bgB = (config->bgColor >>  8) & 0xFF; 
	const uint8_t bgA = (config->bgColor >>  0) & 0xFF;

	// Loop through display pixels, draw a rectangle per pixel to the SDL window
	for (uint32_t i = 0; i < sizeof chip8->display; i++) {

		// Translate 1D I value to 2D x/y coords
		// x = i % window width
		// y = i / window width

		rect.x = (i % config->windowWidth) * config->scaleFactor;
		rect.y = (i / config->windowWidth) * config->scaleFactor;

		if (chip8->display[i]) {
			// If pixel is on, draw fg color
			SDL_SetRenderDrawColor(renderer, fgR, fgG, fgB, fgA);
			SDL_RenderFillRect(renderer, &rect);
		} else {
			SDL_SetRenderDrawColor(renderer, bgR, bgG, bgB, bgA);
			SDL_RenderFillRect(renderer, &rect);
		}
	}
	SDL_RenderPresent(renderer);
}

bool set_config_from_args(config_t* config, int argc, char **argv) {
	// set defaults
	*config = (config_t){
		.windowWidth = 64,		// Chip8 original x resolution
		.windowHeight = 32,		// Chip8 original y resolution
		.fgColor = 0x000000FF,	// YELLOW
		.bgColor = 0xFFFFFFFF,	// BLACK
		.scaleFactor = 20,		// Resolution will be 1280x640
		.instPerSec = 500,  	// # of instructions to emulate per second
	};

	// Overide from passed in args
	for (int i = 0; i < argc; i++)
	{
		(void)argv[i]; // TODO: Get args from commandline
	}
	return true; // Success
}

// Handle User input
// Chip8 Keypad 	QWERTY
// 123C				1234
// 456D				QWER
// 789E				ASDF
// A0BF				ZXCV
void handleInput(chip8_t *chip8) {
	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				chip8->state = QUIT; // Will exit main emulator loop
			return;

			case SDL_KEYUP:
				switch (event.key.keysym.sym) {
					case SDLK_1: chip8->keys[0x1] = false; break;
					case SDLK_2: chip8->keys[0x2] = false; break;
					case SDLK_3: chip8->keys[0x3] = false; break;
					case SDLK_4: chip8->keys[0xC] = false; break;

					case SDLK_q: chip8->keys[0x4] = false; break;
					case SDLK_w: chip8->keys[0x5] = false; break;
					case SDLK_e: chip8->keys[0x6] = false; break;
					case SDLK_r: chip8->keys[0xD] = false; break;

					case SDLK_a: chip8->keys[0x7] = false; break;
					case SDLK_s: chip8->keys[0x8] = false; break;
					case SDLK_d: chip8->keys[0x8] = false; break;
					case SDLK_f: chip8->keys[0xE] = false; break;

					case SDLK_z: chip8->keys[0xA] = false; break;
					case SDLK_x: chip8->keys[0x0] = false; break;
					case SDLK_c: chip8->keys[0xB] = false; break;
					case SDLK_v: chip8->keys[0xF] = false; break;

					default: break;
				}
				break;

			case SDL_KEYDOWN:
				switch (event.key.keysym.sym) {
					case SDLK_ESCAPE:
						chip8->state = QUIT;
						return;
						break;
					
					case SDLK_SPACE:
						// Space bar
						if (chip8->state == RUNNING) {
							chip8->state = PAUSE;
							printf("===== PAUSED =====\n");
						}
						else chip8->state = RUNNING;
						return;
						break;
					
					case SDLK_1: chip8->keys[0x1] = true; break;
					case SDLK_2: chip8->keys[0x2] = true; break;
					case SDLK_3: chip8->keys[0x3] = true; break;
					case SDLK_4: chip8->keys[0xC] = true; break;

					case SDLK_q: chip8->keys[0x4] = true; break;
					case SDLK_w: chip8->keys[0x5] = true; break;
					case SDLK_e: chip8->keys[0x6] = true; break;
					case SDLK_r: chip8->keys[0xD] = true; break;

					case SDLK_a: chip8->keys[0x7] = true; break;
					case SDLK_s: chip8->keys[0x8] = true; break;
					case SDLK_d: chip8->keys[0x8] = true; break;
					case SDLK_f: chip8->keys[0xE] = true; break;

					case SDLK_z: chip8->keys[0xA] = true; break;
					case SDLK_x: chip8->keys[0x0] = true; break;
					case SDLK_c: chip8->keys[0xB] = true; break;
					case SDLK_v: chip8->keys[0xF] = true; break;

					default: break;
				}
				break;
			
			default:
				break;

		}
	}
}

void emulateInstruction(chip8_t *chip8, const config_t *config) {
	// get next opcode from ram
	if (chip8->state != PAUSE) {
		chip8->inst.opcode = (chip8->memory[chip8->pc] << 8) | chip8->memory[chip8->pc+1];
		chip8->pc += 2;

		// Fill out instruction format
		chip8->inst.NNN = chip8->inst.opcode & 0x0FFF;
		chip8->inst.NN = chip8->inst.opcode & 0x0FF;
		chip8->inst.N = chip8->inst.opcode & 0x0F;
		chip8->inst.X = (chip8->inst.opcode >> 8) & 0x0F;
		chip8->inst.Y = (chip8->inst.opcode >> 4) & 0x0F;

	#ifdef DEBUG
		printDebugInfo(chip8);
	#endif

		// Emulate opcode
		switch ((chip8->inst.opcode >> 12) & 0x0F)
		{
		case 0x00:
			if (chip8->inst.NN == 0xE0) {
				// 0x00E0: clear screen
				memset(&chip8->display[0], false, sizeof(chip8->display));
			} else if (chip8->inst.NN == 0xEE) {
				// 0x00EE: Return from subroutine
				chip8->sp--;
				chip8->pc = chip8->stack[chip8->sp];
			} else {
				// Unimplemented /invalid opcode, may be 0xNNN for callling machine code for RCA1802
			}
			break;
		
		case 0x01:
			// 0x1NNN jump to adress NNN
			chip8->pc = chip8->inst.NNN;
			break;
		
		case 0x02:
			// 0x02NNN: call subroutine at NNN
			chip8->stack[chip8->sp] = chip8->pc;
			chip8->sp++;
			chip8->pc = chip8->inst.NNN;
			break;
		
		case 0x03:
			// 0x3XNN: check if VX == NN, if so, skip next inst
			if (chip8->V[chip8->inst.X] == chip8->inst.NN)
				chip8->pc += 2; // Skip next opcode
			break;

		case 0x04:
			// 0x4XNN: check if VX != NN, if so, skip next inst
			if (chip8->V[chip8->inst.X] != chip8->inst.NN)
				chip8->pc += 2; // Skip next opcode
			break;
		
		case 0x05:
			// 0x5XY0: check if VX == VY, skip next inst if so
			if (chip8->inst.N != 0) break; // wrong opcode

			if (chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y])
				chip8->pc += 2; // Skip next opcode
			break;

		case 0x06:
			// 0x6XNN: Set register VX to NN
			chip8->V[chip8->inst.X] = chip8->inst.NN;
			break;
		
		case 0x07:
			// 076XNN: Set register VX += NN
			chip8->V[chip8->inst.X] += chip8->inst.NN;
			break;

		case 0x08:
			switch(chip8->inst.N) {
				case 0:
					// 0x8XY0: Set register VX = VY
					chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y];
					break;
				case 1:
					// 0x8XY1: Set register VX |= VY
					chip8->V[chip8->inst.X] |= chip8->V[chip8->inst.Y];
					break;
				case 2:
					// 0x8XY2: Set register VX &= VY
					chip8->V[chip8->inst.X] &= chip8->V[chip8->inst.Y];
					break;
				case 3:
					// 0x8XY3: Set register VX ^= VY
					chip8->V[chip8->inst.X] ^= chip8->V[chip8->inst.Y];
					break;
				case 4:
					// 0x8XY4: Set register VX += VY, set VF to 1 if carry
					if ((uint16_t)(chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y]) > 255)
						chip8->V[0xF] = 1;
					chip8->V[chip8->inst.X] += chip8->V[chip8->inst.Y];
					break;
				case 5:
					// 0x8XY5: Set register VX -= VY, set VF to 1 if there is not a borrow (result is positive)
					// if (chip8->V[chip8->inst.X] >= chip8->V[chip8->inst.Y])
					// 	chip8->V[0xF] = 1;
					chip8->V[0xF] = chip8->V[chip8->inst.X] >= chip8->V[chip8->inst.Y];
					chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];
					break;
				case 6:
					// 0x8XY6: Set register VX >>= 1, Store shifted off bit in VF
					chip8->V[0xF] = ((chip8->V[chip8->inst.X] & 1) << 7) >> 7;
					chip8->V[chip8->inst.X] >>= 1;
					break;
				case 7:
					// 0x8XY7: Set register VX = VY - VX, set VF to 1 if there is not a borrow (result is positive)
					chip8->V[0xF] = chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y];
					chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X];
					break;
				case 0xE:
					// 0x8XY6: Set register VX <<= 1, Store shifted off bit in VF
					chip8->V[0xF] = (chip8->V[chip8->inst.X] & 0x80) >> 7;
					chip8->V[chip8->inst.X] <<= 1;
					break;
				default:
				 	// Wong/unimplemeted
					break;
			}
			break;
		
		case 0x09:
			// Check if VX != VY; skip next inst if so
			if (chip8->V[chip8->inst.X] != chip8->V[chip8->inst.Y])
				chip8->pc += 2; // Skip next opcode
			break;

		case 0x0A:
			// 0xANNN: Set index register I to NNN
			chip8->I = chip8->inst.NNN;
			break;
		
		case 0x0B:
			// 0xBNNN: Jump to V0 + NNN
			chip8->pc = chip8->V[0] + chip8->inst.NNN;
			break;

		case 0x0C:
			// 0xCXNN: Sets VX = rand(% 256 & NN) bitwise and
			chip8->V[chip8->inst.X] = (rand() % 256) & chip8->inst.NN;
			break;

		case 0x0D: {
			// 0xDXYN, draws N height sprite at coord X,Y, read from mem location I 
			// Screen pixels are XOR'd with sprite bits, 
			// VF (carry flag) is set if any screen pixels are set off; useful for collision detection
			uint8_t Xcoord = chip8->V[chip8->inst.X] % config->windowWidth;
			uint8_t Ycoord = chip8->V[chip8->inst.Y] % config->windowHeight;
			const uint8_t origX = Xcoord;

			chip8->V[0xF] = 0; // Init carry flag to zero

			// Read each row of sprite
			for (uint8_t i = 0; i < chip8->inst.N; i++) {
				// Get next row of sprite data
				const uint8_t spriteData = chip8->memory[chip8->I + i];
				Xcoord = origX; // Reset X for next row to draw

				for (int8_t j = 7; j >= 0; j--) {
					// If sprite pixel bit is on and display pixel is on, set carry flag
					bool *pixel = &chip8->display[Ycoord * config->windowWidth + Xcoord];
					const bool spriteBit = (spriteData & (1 << j));

					if (spriteBit && *pixel) {
						chip8->V[0xF] = 1;
					}
					// XOR display pixel with sprite pixel/bit
					*pixel ^= spriteBit;

					// Stop drawing if hit right edge of screen
					if (++Xcoord >= config->windowWidth) break;
				}

				// Stop drawing entire sprite if hit bottom edge of screen 
				if (++Ycoord >= config->windowHeight) break;	
			}
			break;
		}

		case 0x0E:
			if (chip8->inst.NN == 0x9E) {
				// 0xEX9E: Skip next inst if key in VX is pressed
				if (chip8->keys[chip8->V[chip8->inst.X]])
					chip8->pc += 2;
			} else if (chip8->inst.NN == 0xA1) {
				// 0xEXA1: Skip next inst if key in VX is not pressed
				if (!chip8->keys[chip8->V[chip8->inst.X]])
					chip8->pc += 2;
			}
			break;
		
		case 0x0F:
			switch(chip8->inst.NN) {
				case 0x0A: {
					// 0xFX0A: VX = get_key, wait until key pressed and store in VX
					bool anyKeyPressed = false;
					for (uint8_t i = 0; i < sizeof chip8->keys; i++) {
						if (chip8->keys[i]) {
							chip8->V[chip8->inst.X];
							break;
						}
					}
					// Keep getting current opcode and running this inst if no key pressed
					if (!anyKeyPressed) chip8->pc -= 2;
					break;
				}
				
				case 0x1E:
					// 0xFX1E: I += VX; For non Amiga Chip-8, does not affect VF
					chip8->I += chip8->V[chip8->inst.X];
					break;
				
				case 0x07:
					// 0xFX07: VX = Delay timer
					chip8->V[chip8->inst.X] = chip8->delay_timer;
					break;
				
				case 0x15:
					// 0xFX07: Delay timer = VX
					chip8->delay_timer = chip8->V[chip8->inst.X];
					break;
				
				case 0x18:
					// 0xFX07: sound timer = VX
					chip8->sound_timer = chip8->V[chip8->inst.X];
					break;
				
				case 0x29:
					// 0xFX29: Set register I to sprite location in memory for char in VX (0x0-0xF) 
					chip8->I = chip8->V[chip8->inst.X] * 5;
					break;
				
				case 0x33: {
					// 0xFX33: Store BCD representation at memory offset from I
					// I = Hundreds place, I+1 = tens, I+2 = one's
					uint8_t bcd = chip8->V[chip8->inst.X];
					chip8->memory[chip8->I+2] = bcd % 10;
					bcd /= 10;
					chip8->memory[chip8->I+1] = bcd % 10;
					bcd /= 10;
					chip8->memory[chip8->I] = bcd;
					break;
				}
				case 0x55:
					// 0xFX55: Register dump V0-VX inclusive to memory offset from I, CHIP8 increments I, SCHIP DOES NOT
					for (uint8_t i = 0; i <= chip8->inst.X; i++)
						chip8->memory[chip8->I + i] = chip8->V[i];
					break;

				case 0x65:
					// 0xFX65: Register load V0-VX inclusive to memory offset from I, CHIP8 increments I
					for (uint8_t i = 0; i<= chip8->inst.X; i++)
						chip8->V[i] = chip8->memory[chip8->I + i];
					break;
				default:
					break;
			}
			break;
		default:
			break;
		}
	}
}

void updateTimers(chip8_t *chip8) {
	if (chip8->delay_timer > 0) chip8->delay_timer--;
	if (chip8->sound_timer > 0) chip8->sound_timer--; // TODO: Play sound
}

void printDebugInfo(chip8_t *chip8) {
	printf("Address 0x%04X, Opcode: 0x%04X Desc: ", chip8->pc-2, chip8->inst.opcode);
	switch ((chip8->inst.opcode >> 12) & 0x0F)
	{
	case 0x00:
		if (chip8->inst.NN == 0xE0) {
			// 0x00E0: clear screen
			printf("Clear Screen\n");
		} else if (chip8->inst.NN == 0xEE) {
			// 0x00EE: Return from subroutine
			printf("Return from subroutine to address 0x0%04X\n", chip8->stack[chip8->sp-1]);
		}
		break;
	case 0x01:
		// 0x1NNN jump to adress NNN
		printf("Jump to address 0x%04X\n", chip8->inst.NNN);
		break;
	case 0x02:
		// 0x02NNN: call subroutine at NNN
		printf("Call subroutine at NNN: 0x%04X\n", chip8->inst.NNN);
		break;
	
	case 0x03:
		printf("Check if V%X (0x%02X)X == NN (0x%02X), skip next inst if true\n", 
		chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN);
		break;

	case 0x04:
		printf("Check if V%X (0x%02X)X != NN (0x%02X), skip next inst if true\n", 
		chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN);
		break;

	case 0x05:
		printf("Check if V%X (0x%02X)X == V%X (0x%02X)Y, skip next inst if true\n", 
		chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y]);
		break;

	case 0x06:
		// 0x6XNN: Set register VX to NN
		printf("Set register X: 0x%X = NN: (0x%02X)\n", chip8->inst.X, chip8->inst.NN);
		break;

	case 0x07:
			// 076XNN: Set register VX += NN
			printf("Set register X: 0x%X += NN: (0x%02X)\n", chip8->inst.X, chip8->inst.NN);
			break;

	case 0x08:
			switch(chip8->inst.N) {
				case 0:
					// 0x8XY0: Set register VX = VY
					printf("Set register V%X = V%X (0x%02X)\n", chip8->inst.X, chip8->inst.Y, chip8->V[chip8->inst.Y]);
					break;
				case 1:
					// 0x8XY1: Set register VX |= VY
					printf("Set register V%X (0x%02X) |= V%X (0x%02X); Result: (0x%02X)\n", 
					chip8->inst.X, chip8->V[chip8->inst.X], 
					chip8->inst.Y, chip8->V[chip8->inst.Y], 
					chip8->V[chip8->inst.X] | chip8->V[chip8->inst.Y]);
					break;
				case 2:
					// 0x8XY2: Set register VX &= VY
					printf("Set register V%X (0x%02X) &= V%X (0x%02X); Result: (0x%02X)\n", 
					chip8->inst.X, chip8->V[chip8->inst.X], 
					chip8->inst.Y, chip8->V[chip8->inst.Y], 
					chip8->V[chip8->inst.X] & chip8->V[chip8->inst.Y]);
					break;
				case 3:
					// 0x8XY3: Set register VX ^= VY
					printf("Set register V%X (0x%02X) ^= V%X (0x%02X); Result: (0x%02X)\n", 
					chip8->inst.X, chip8->V[chip8->inst.X], 
					chip8->inst.Y, chip8->V[chip8->inst.Y], 
					chip8->V[chip8->inst.X] ^ chip8->V[chip8->inst.Y]);
					break;
				case 4:
					// 0x8XY4: Set register VX += VY, set VF to 1 if carry
					printf("Set register V%X (0x%02X) += V%X (0x%02X), VF = 1 if carry; Result: (0x%02X), VF = %X\n", 
					chip8->inst.X, chip8->V[chip8->inst.X], 
					chip8->inst.Y, chip8->V[chip8->inst.Y], 
					chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y],
					((uint16_t)(chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y]) > 255));
					break;
				case 5:
					// 0x8XY5: Set register VX -= VY, set VF to 1 if there is not a borrow (result is positive)
					printf("Set register V%X (0x%02X) -= V%X (0x%02X), VF = 1 if no borrow; Result: (0x%02X), VF = %X\n", 
					chip8->inst.X, chip8->V[chip8->inst.X], 
					chip8->inst.Y, chip8->V[chip8->inst.Y], 
					chip8->V[chip8->inst.X] - chip8->V[chip8->inst.Y],
					(chip8->V[chip8->inst.X] >= chip8->V[chip8->inst.Y]));
					break;
				case 6:
					// 0x8XY6: Set register VX >>= 1, Store shifted off bit in VF
					printf("Set register V%X (0x%02X) >>= 1 VF = Shifted off bits (%X); Result: (0x%02X)\n", 
					chip8->inst.X, chip8->V[chip8->inst.X],
					chip8->V[chip8->inst.X] & 1,
					chip8->V[chip8->inst.X] >> 1);
					break;
				case 7:
					// 0x8XY7: Set register VX = VY - VX, set VF to 1 if there is not a borrow (result is positive)
					printf("Set register V%X = V%X (0x%02X) - V%X (0x%02X), VF = 1 if no borrow; Result: (0x%02X), VF = %X\n", 
					chip8->inst.X,
					chip8->inst.Y, chip8->V[chip8->inst.Y],
					chip8->inst.X, chip8->V[chip8->inst.X], 
					chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X],
					(chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y]));
					break;
				case 0xE:
					// 0x8XY6: Set register VX <<= 1, Store shifted off bit in VF
					printf("Set register V%X (0x%02X) <<= 1 VF = Shifted off bits (%X); Result: (0x%02X)\n", 
					chip8->inst.X, chip8->V[chip8->inst.X],
					(chip8->V[chip8->inst.X] & 0x80) >> 7,
					chip8->V[chip8->inst.X] >> 1);
					break;
			}
	
	case 0x09:
		// 0x9XY0: Check if VX != VY; skip next inst if so
		printf("Check if V%X (0x%02X)X != V%X (0x%02X)Y, skip next inst if true\n", 
		chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y]);
		break;

	case 0x0A:
		// 0xANNN: Set index register I to NNN
		printf("Set index register I to NNN: 0x%04X\n", chip8->inst.NNN);
	break;

	case 0x0B:
		// 0xBNNN: Jump to V0 + NNN
		printf("Set PC to V0 (0x%02X) + NNN (0x%02X); Result PC = 0x%04X\n", chip8->V[0], chip8->inst.NNN, chip8->V[0] + chip8->inst.NNN);
		break;

	case 0x0C:
		// 0xCXNN: Sets VX = rand(% 256 & NN) bitwise and
		printf("Set PC to V%X = rand() %% 256 & NN (0x%02X)", chip8->inst.X, chip8->inst.NN);
		break;

	case 0x0D:
		// 0xDXYN, draws N height sprite at coord X,Y, read from mem location I 
		printf("draws (N) %u height sprite at coord V0x%X (0x%02X), V0x%X (0x%02X),"
		"from mem location I (0x%04X)\nSet VF = 1 if any pixels are turned off\n",
		 chip8->inst.N, chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y],
		 chip8->I);
	break;

	case 0x0E:
			if (chip8->inst.NN == 0x9E) {
				printf("Skip next inst if key in V%X (0x%02X) is pressed; Keypad value: %d\n", 
				chip8->inst.X, chip8->V[chip8->inst.X], chip8->keys[chip8->V[chip8->inst.X]]);
			} else if (chip8->inst.NN == 0xA1) {
				// 0xEXA1: Skip next inst if key in VX is not pressed
				printf("Skip next inst if key in V%X (0x%02X) is not pressed; Keypad value: %d\n", 
					chip8->inst.X, chip8->V[chip8->inst.X], chip8->keys[chip8->V[chip8->inst.X]]);
			}
			break;

	case 0x0F:
		switch(chip8->inst.NN) {
			case 0x0A:
				// 0xFX0A: VX = get_key, wait until key pressed and store in VX
				printf("Await until key is pressed, store key in V%X\n", chip8->inst.X);
				break;
			
			case 0x1E:
				// 0xFX1E: I += VX; For non Amiga Chip-8, does not affect VF
				printf("I (0x%04X) += V%X (0x%02X); Result (I): 0x%04X\n", 
				chip8->I, chip8->inst.X, chip8->V[chip8->inst.X], chip8->I + chip8->V[chip8->inst.X]);
				break;
				
			case 0x07:
				// 0xFX07: VX = Delay timer
				printf("Set V%X to Delay Timer (0x%02X)\n", chip8->V[chip8->inst.X], chip8->delay_timer);
				break;
			
			case 0x15:
				// 0xFX07: Delay timer = VX
				printf("Set Delay Timer to V%X (0x%02X)\n", chip8->inst.X, chip8->V[chip8->inst.X]);
				break;
			
			case 0x18:
				// 0xFX07: sound timer = VX
				printf("Set Sound Timer to V%X (0x%02X)\n", chip8->inst.X, chip8->V[chip8->inst.X]);
				break;
			
			case 0x29:
				printf("Set I to sprite location in memory for character in V%X (0x%02X); Result(VX * 5): (0x%02X)\n", 
						chip8->inst.X, chip8->V[chip8->inst.X], chip8->V[chip8->inst.X] * 5);
				break;
			
			case 0x33:
				// 0xFX33: Store BCD representation at memory offset from I
				printf("Store BCD representation of V%X (0x%02X) at memory offset from I (0x%04X)\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->I);
				break;

			case 0x55:
				// 0xFX55: Register dump V0-VX inclusive to memory offset from I, CHIP8 increments I
				printf("Register dump V0-V%X inclusive to memory offset from I (0x%04X)\n", chip8->inst.X, chip8->I);
				break;

			case 0x65:
				// 0xFX65: Register load V0-VX inclusive to memory offset from I, CHIP8 increments I
				printf("Register load V0-V%X inclusive to memory offset from I (0x%04X)\n", chip8->inst.X, chip8->I);
				break;

			default:
				break;
		}
		break;

	default:
		printf("Uninplemented or invalid opcode\n");
		break;
	}
}