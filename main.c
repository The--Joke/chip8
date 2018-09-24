#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Global SDL stuff

SDL_Window * screen;
SDL_Renderer * rend;

// set to zero when screen needs to be flipped.
int SCREEN_IS_CUR = 1;

struct CPU {

  unsigned char memory[4096];  // a whopping 4Kb of ram

  unsigned char V[16];         // 16 general purpose registers

  unsigned short I;            // Register for to store memory addresses

  unsigned char delay;
  unsigned char sound;         // delay and sound timers.

  unsigned short pc;           // program counter

  unsigned short stack[16];    // execution stack

  unsigned char sp;        // stack pointer

};

struct CPU cpu;

// 64x32 wxh display

unsigned char screenData[32][64];

char sprites[] = {
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7   // Sprites representing characters that are stored in memory of the chip8
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

// used for deconstructing opcodes
unsigned char * s_x(unsigned short opcode){

	return &cpu.V[(opcode >> 8) & 0xF];

}


unsigned char * s_y(unsigned short opcode){

	return &cpu.V[(opcode >> 4) & 0xF];

}

int s_nnn(unsigned short opcode){

	return opcode & 0xFFF;

}

int s_n(unsigned short opcode){

	return opcode & 0xF;

}

int s_kk(unsigned short opcode){

	return opcode & 0xFF;

}




void drawSprite(int x, int y, int n){

	int i, j;
	unsigned char c;
	
	// for each row in the sprite
	for (j=0; j<n; j++){

		c = cpu.memory[cpu.I + j];

		// for each bit in the byte ( the row )
		for (i=0; i<8; i++){

			// 1 if ith bit is set; zero otherwise
			int bit = (c >> (8-i-1)) & 1;

			// only changes if bit is 1
			if (bit){
				// if bit on screen is on, a collision happens
				cpu.V[0xF] = screenData[(y + j) % 32][(x+i) % 64];

				screenData[(y+j) % 32][(x+i) % 64] ^= bit;

				SCREEN_IS_CUR = 0;
			}

			
		}

	}

}

void flipScreen(){
	int i, j;

	SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);

	SDL_RenderClear(rend);

	SDL_SetRenderDrawColor(rend, 255, 255, 255, 255);

	for (i=0; i<32; i++){
		for (j=0; j<64; j++){

			if ( screenData[i][j] == 1 ) {
				SDL_Rect r;
				r.x = j*10;
				r.y = i*10;
				r.h = 10;
				r.w = 10;

				SDL_RenderFillRect(rend, &r);
			}
		}
	}

	SCREEN_IS_CUR = 1;

	SDL_RenderPresent(rend);
}

// blank out the screen data then flip it
void cls(){
	int i, j;

	for (i=0; i<32; i++){
		for (j=0; j<64; j++){
			screenData[i][j] = 0;
		}
	}

	flipScreen();
}

// for keeping track of the keyboard state.
// keyboard looks weird:
//  1 2 3 C
//  4 5 6 D
//  7 8 9 E
//  A 0 B F

int keyboard[] = {
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0
};

// zeroes out all variables in CPU and sets the program counter to 0x200
void initializeCPU(){
	int i;

	for (i=0; i<4096; i++){
		cpu.memory[i] = 0;
	}
	for (i=0; i<16; i++){
		cpu.stack[i] = 0;
		cpu.V[i] = 0;
	}

	cpu.delay = 0;
	cpu.sound = 0;
	cpu.I = 0;
	cpu.pc = 0x200; // program instructions start at 0x200
	cpu.sp = 0;
}

// loads built-in hex sprites to memory
void loadSprites(){
	int i;

	for (i=0; i<16*5; i++){
		cpu.memory[i] = sprites[i];
	}
}


// copy contents of passed file into 0x200 and onwards
void loadROM(char * f){

	FILE * fp;
	fp = fopen(f, "r");

	int cur;
	int i = 0x200;

	while ((cur = getc(fp)) != EOF){
		cpu.memory[i] = cur;
		i++;
	}

	fclose(fp);

}

void cycle(){

	unsigned short instruction;

	// read two bytes from memory
	instruction = (cpu.memory[cpu.pc] << 8) | (cpu.memory[cpu.pc+1]);

	// increment pc here
	cpu.pc += 2;

	// opcodes vary mostly by their most significant nibble
	
	int result;
	int i;

	//printf("Current inst: %x\n", instruction);

	switch ( instruction & 0xF000 ){

		case 0x0000:
			// clear display
			if (instruction == 0x00E0){
				cls();
			}
			// return
			if (instruction == 0x00EE){
				cpu.sp--;
				cpu.pc = cpu.stack[cpu.sp];
			}

			break;

		// jump instruction
		case 0x1000:
			cpu.pc = s_nnn(instruction);
			break;

		// call subroutine
		case 0x2000:
			// add current address to the stack
			cpu.stack[cpu.sp] = cpu.pc;
			cpu.sp++;
			cpu.pc = s_nnn(instruction);
			break;

		// skip if Vx == kk
		case 0x3000:
			if (*s_x(instruction) == s_kk(instruction)){
				cpu.pc += 2;
			}
			break;

		// skip if Vx != kk
		case 0x4000:
			if (*s_x(instruction) != s_kk(instruction)){
				cpu.pc += 2;
			}
			break;

		// skip if Vx == Vy
		case 0x5000:
			if (*s_x(instruction) == *s_y(instruction)){
				cpu.pc += 2;
			}
			break;

		// Put kk into Vx
		case 0x6000:
			*s_x(instruction) = s_kk(instruction);
			break;

		// Vx += kk
		case 0x7000:
			*s_x(instruction) += s_kk(instruction);
			break;

		// lots of stuff
		case 0x8000:

			switch ( instruction & 0xF ){
				// Vx = Vy
				case 0x0:
					*s_x(instruction) = *s_y(instruction);
					break;

				// Vx = Vx | Vy
				case 0x1:
					*s_x(instruction) |= *s_y(instruction);
					break;

				// Vx = Vx & Vy
				case 0x2:
					*s_x(instruction) &= *s_y(instruction);
					break;

				// Vx = Vx XOR Vy
				case 0x3:
					*s_x(instruction) ^= *s_y(instruction);
					break;

				// Vx += Vy ; VF = carry bit
				case 0x4:
					result = *s_x(instruction) + *s_y(instruction);
					cpu.V[0xF] = result > 255;
					// let the overflow happen if it did
					*s_x(instruction) += *s_y(instruction);
					break;

				// Vx = Vx - Vy ; VF = Vx > Vy
				case 0x5:
					cpu.V[0xF] = *s_x(instruction) > *s_y(instruction);

					*s_x(instruction) -= *s_y(instruction);

					break;

				// Vx = Vx >> 1 ; VF = Vx & 1 (before shift)
				case 0x6:
					cpu.V[0xF] = *s_x(instruction) & 1;
					*s_x(instruction) >>= 1;
					break;

				// Vx = Vy - Vx ; VF = Vy > Vx
				case 0x7:
					cpu.V[0xF] = *s_y(instruction) > *s_x(instruction);
					*s_x(instruction) = *s_y(instruction) - *s_x(instruction);
					break;

				// Vx  = Vx << 1 ; VF = MSB of VX before shift
				case 0xE:
					cpu.V[0xF] = (*s_x(instruction) & (1 << 7)) >> 7;

					*s_x(instruction) <<= 1;
					break;
			}
		
			break;	

		// Skip if Vx != Vy
		case 0x9000:
			if ( *s_x(instruction) != *s_y(instruction) ){
				cpu.pc += 2;
			}
			break;

		// I = nnn
		case 0xA000:
			cpu.I = s_nnn(instruction);
			break;

		// jump to V0 + nnn
		case 0xB000:
			cpu.pc = cpu.V[0] + s_nnn(instruction);
			break;

		// Vx = Random byte & kk
		case 0xC000:
			*s_x(instruction) = rand() & 0xFFFF & s_kk(instruction);
			break;

		// Draw memory address I at (Vx, Vy), VF = collision
		case 0xD000:
			drawSprite((int) *s_x(instruction),
				   (int) *s_y(instruction),
				   s_n(instruction));
			break;

		// keyboard
		case 0xE000:
			// skip if key with value Vx is pressed
			if ( (instruction & 0xF) == 0xE ){
				if (keyboard[*s_x(instruction)]){
					cpu.pc += 2;
				}
				break;
			}
			// skip if key is not pressed
			if ( (instruction & 0xF) == 0x1 ){
				if (!keyboard[*s_x(instruction)]){
					cpu.pc += 2;
				}
				break;
			}

		// register stuff
		case 0xF000:
			switch ( instruction & 0xFF ){
				// Vx = delay timer
				case 0x07:
					*s_x(instruction) = cpu.delay;
					break;

				// Wait for keypress and store in Vx
				case 0x0A:
					break;

				// Delay = Vx
				case 0x15:
					cpu.delay = *s_x(instruction);
					break;

				// sound timer = Vx
				case 0x18:
					cpu.sound = *s_x(instruction);
					break;

				// I = I + Vx
				case 0x1E:
					cpu.I += *s_x(instruction);
					break;

				// I = location for digit in Vx
				case 0x29:
					cpu.I = 0x05 * *s_x(instruction);
					break;

				// BCD representation of Vx in I, I+1, I+2
				case 0x33:
					result = *s_x(instruction);
					cpu.memory[cpu.I+2] = result%10;
					result /= 10;
					cpu.memory[cpu.I+1] = result%10;
					result /= 10;
					cpu.memory[cpu.I] = result%10;
					break;

				// Store registers V0 through Vx in memory starting at I
				case 0x55:
					result = s_x(instruction)-cpu.V;
					for (i=0; i < result; i++){
						cpu.memory[cpu.I+i] = cpu.V[i];
					}
					break;

				// Read V0 through Vx starting at I
				case 0x65:
					result = s_x(instruction)-cpu.V;
					for (i=0; i < result; i++){
						cpu.V[i] = cpu.memory[cpu.I+i];
					}
					break;



			}
			break;

	}

}

void executionLoop(){

	SDL_Event e;

	int quit = 0;

	while (!quit){
		while (SDL_PollEvent(&e)){

			if (e.type == SDL_QUIT){
				quit = 1;
			}

			if (e.type == SDL_KEYDOWN){
				switch (e.key.keysym.sym){
					case SDLK_1:
						keyboard[0x1] = 1;
						break;
					case SDLK_2:
						keyboard[0x2] = 1;
						break;
					case SDLK_3:
						keyboard[0x3] = 1;
						break;
					case SDLK_4:
						keyboard[0xC] = 1;
						break;
					case SDLK_q:
						keyboard[0x4] = 1;
						break;
					case SDLK_w:
						keyboard[0x5] = 1;
						break;
					case SDLK_e:
						keyboard[0x6] = 1;
						break;
					case SDLK_r:
						keyboard[0xD] = 1;
						break;
					case SDLK_a:
						keyboard[0x7] = 1;
						break;
					case SDLK_s:
						keyboard[0x8] = 1;
						break;
					case SDLK_d:
						keyboard[0x9] = 1;
						break;
					case SDLK_f:
						keyboard[0xE] = 1;
						break;
					case SDLK_z:
						keyboard[0xA] = 1;
						break;
					case SDLK_x:
						keyboard[0x0] = 1;
						break;
					case SDLK_c:
						keyboard[0xB] = 1;
						break;
					case SDLK_v:
						keyboard[0xF] = 1;
						break;
				}

			}

			if (e.type == SDL_KEYUP){
				switch (e.key.keysym.sym){
					case SDLK_1:
						keyboard[0x1] = 0;
						break;
					case SDLK_2:
						keyboard[0x2] = 0;
						break;
					case SDLK_3:
						keyboard[0x3] = 0;
						break;
					case SDLK_4:
						keyboard[0xC] = 0;
						break;
					case SDLK_q:
						keyboard[0x4] = 0;
						break;
					case SDLK_w:
						keyboard[0x5] = 0;
						break;
					case SDLK_e:
						keyboard[0x6] = 0;
						break;
					case SDLK_r:
						keyboard[0xD] = 0;
						break;
					case SDLK_a:
						keyboard[0x7] = 0;
						break;
					case SDLK_s:
						keyboard[0x8] = 0;
						break;
					case SDLK_d:
						keyboard[0x9] = 0;
						break;
					case SDLK_f:
						keyboard[0xE] = 0;
						break;
					case SDLK_z:
						keyboard[0xA] = 0;
						break;
					case SDLK_x:
						keyboard[0x0] = 0;
						break;
					case SDLK_c:
						keyboard[0xB] = 0;
						break;
					case SDLK_v:
						keyboard[0xF] = 0;
						break;
				}


			}

		}

		cycle();

		if (!SCREEN_IS_CUR) flipScreen();

		// for now 100ms between steps
		SDL_Delay(5);

		if (cpu.delay > 0){
			cpu.delay--;
		}
		if (cpu.sound > 0){
			cpu.sound--;
		}

	}

}

void initSDL(){
	SDL_Init(SDL_INIT_VIDEO);

	// 64 x 32 screen
	screen = SDL_CreateWindow("Chip8", 100, 100, 640, 320, SDL_WINDOW_SHOWN);

	rend = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED);

	cls();
}

int main(int argc, char ** argv){

	srand(time(0));

	if (argc < 2){
		printf("Usage: ./main {ROM}\n");
		exit(1);
	}

	initializeCPU();
	loadSprites();

	loadROM(argv[1]);

	initSDL();

	flipScreen();

	executionLoop();

}






