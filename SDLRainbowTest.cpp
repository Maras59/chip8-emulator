#include <stdio.h>
#include <SDL2/SDL.h>
#include "chip8.h"


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
	uint32_t scaleFactor;	// 	Amount to scale chip8 pixel 
} config_t;

typedef struct {
	emu_state_t state;
} chip8_t;

// setup emulator config from passed in args
bool set_config_from_args(config_t* config, int argc, char **argv);

void updateScreen(SDL_Renderer* renderer);

void handleInput(chip8_t *chip8) {
	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				chip8->state = QUIT; // Will exit main emulator loop
			return;

			case SDL_KEYDOWN:
			break;
			
			case SDL_KEYUP:
				switch (event.key.keysym.sym) {
					case SDLK_ESCAPE:
						chip8->state = QUIT;
						return;
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

int main(int argc, char **argv) 
{
	config_t config = {0};
	chip8_t chip8 = {};
	chip8.state = RUNNING;
	if (!set_config_from_args(&config, argc, argv)) exit(EXIT_FAILURE);

	if(argc < 2)
	{
		printf("Usage: myChip8.exe chip8application\n");
	}

	// Load game
	// if(!myChip8.loadApplication(argv[1]))		
	// 	return 1;
		
	// Setup SDL
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

	bool rFlag = true;
	bool gFlag = false;
	bool bFlag = false;

	int red = 255;
	int grn = 0;
	int blu = 0;

	// Main emulator loop
	while (chip8.state != QUIT) {
		if (rFlag) {
			if (red == 255) {
				rFlag = false;
				gFlag = true;
			} else { 
				red++;
				blu--;
			}
		}
		if (gFlag) {
			if (grn == 255) {
				gFlag = false;
				bFlag = true;
			} else { 
				grn++;
				red--;
			}
		}
		if (bFlag) {
			if (blu == 255) {
				bFlag = false;
				rFlag = true;
			} else { 
				blu++;
				grn--;
			}
		}

		SDL_SetRenderDrawColor(renderer, red,grn,blu,a);
		SDL_RenderClear(renderer);


		// Handle userinput
		handleInput(&chip8);


		// Get time()
		// TODO: Emulate chip8 instructions
		// Get time() elapsed, then calculate needed delay
		// SDL_Delay(17 - elapsed time);

		// delay (approx 60fps)
		SDL_Delay(16);
		// update window with changes on each iteration
		updateScreen(renderer);
	}



	// Shut down SDL
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	printf("Success!!\n");
	return 0;
}


void updateScreen(SDL_Renderer* renderer) {
	SDL_RenderPresent(renderer);
}


bool set_config_from_args(config_t* config, int argc, char **argv) {
	// set defaults
	*config = (config_t){
		.windowWidth = 64,		// Chip8 original x resolution
		.windowHeight = 32,		// Chip8 original y resolution
		.fgColor = 0xFFFF00FF,	// YELLOW
		.bgColor = 0xFFFF00FF,	// BLACK
		.scaleFactor = 20,		// Resolution will be 1280x640
	};

	// Overide from passed in args
	for (int i = 0; i < argc; i++)
	{
		(void)argv[i]; // TODO: Get args from commandline
	}
	return true; // Success
}