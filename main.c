#include <stdio.h>
#include <stdbool.h>
#include "SDL.h"

#define WINDOW_H 500
#define WINDOW_W 500

#define SAMPLE_RATE 48000 /* Samples per second */
#define GLOBAL_AMP 0.8 /* Never allow sample amplitudes to exceed 80% of maximum value */
#define CHUNK_LEN_SAMPLES 1024 /* How many sample frames each invocation of callback should ask for */

double tone_freq = 0; /* Frequency in hz (1/s) */
double tone_amp = 0; /* Should range betw. 0 and 1 */

 /* At the end of each invocation of callback, save current pos in saw wav so we can start the next one at the same place */
double saved_amplitude = 0;


/* Write the most "current" audio samples in a global buffer so we can graph them */
Sint16 global_buffer[CHUNK_LEN_SAMPLES];

/* Use global values of tone_amp and tone_freq */
void generate_saw_wav(Sint16 *buffer_to_fill, int len_samples)
{
    /* wavelen in seconds = 1 / tone_freq. To get in samples, multiply by samples/sec (sample rate) */
    double wavelen_samples = SAMPLE_RATE / tone_freq;

    double max_amp_range = INT16_MAX * 2; /* Sample values could range from INT16_MIN to INT16_MAX (-32768 -> 32768) */
    double amp_increment = max_amp_range * tone_amp * GLOBAL_AMP / wavelen_samples;
    double max_amp = INT16_MAX * tone_amp * GLOBAL_AMP;
    
    double sample_amp = saved_amplitude + amp_increment; /* Start where we left off (+ one increment) */
    for (int i=0; i<len_samples; i++) {
	buffer_to_fill[i] = (Sint16) sample_amp;
	if (sample_amp >= max_amp) {
	    sample_amp = -1 * max_amp; /* When max amp value reached, send the amplitude back to min value */
	}
	sample_amp += amp_increment;
    }

    /* Save the amplitude of the last written sample. When this fn is called again, we start there. */
    saved_amplitude = sample_amp;
}


/* SDL calls this function on a high-priority thread when the audio device asks for more data.
 We are responsible for filling the array of bytes pointed to by "buffer" with audio data, as quickly as possible. */
void audio_callback(void *userdata, Uint8 *buffer, int len)
{
    /* Initialize the audio buffer with silence. Good practice to avoid playing (potentially harmful) garbage. */
    memset(buffer, '\0', len);

    /* len is buffer len in bytes; size of individual sample in bytes is sizeof(Sint16)  */
    int samples_len = len / sizeof(Sint16);

    /* Populate the "global buffer" with the saw waveform */
    generate_saw_wav(global_buffer, samples_len);

    /* Copy the global buffer to the device buffer for immediate playback */
    memcpy(buffer, global_buffer, len);
}

/* Draw the waveform to a target rect.
   Don't worry about aliasing for now bc ain't nobody got time for that */
void draw_waveform(SDL_Renderer *rend, SDL_Rect *target_rect)
{
    int x = target_rect->x;
    int mid_y = target_rect->y + (target_rect->h / 2);
    
    double samples_per_draw_coord = (double) CHUNK_LEN_SAMPLES / target_rect->w;
    double sample_index = 0;

    
    while (x < target_rect->x + target_rect->w) {
	double amp_scaled = (double)global_buffer[(int)sample_index] / INT16_MAX * target_rect->h;

	SDL_RenderDrawLine(rend, x, mid_y, x, mid_y + amp_scaled);
	sample_index += samples_per_draw_coord;
	x++;
    }
}
    

int main()
{

    /* Initialize SDL before you do anything */
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) != 0) {
	fprintf(stderr, "Failed to initialize SDL\n");
	exit(1);
    }

    /* Create a window and check for errors */
    SDL_Window *win = SDL_CreateWindow("theremin synth", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, 0);
    if (!win) {
	fprintf(stderr, "Fatal error: failed to create window. %s\n", SDL_GetError());
	exit(1);
    }

    /* Create an accompanying renderer and check for errors */
    SDL_Renderer *rend = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
    if (!rend) {
	fprintf(stderr, "Fatal error: failed to create renderer. %s\n", SDL_GetError());
	exit(1);
    }
    
    /* Allocate space on stack for two SDL_AudioSpec structs */
    SDL_AudioSpec desired, obtained;

    /* Set the members of "desired" to the audio settings we want */
    desired.channels = 1;
    desired.freq = SAMPLE_RATE;
    desired.format = AUDIO_S16SYS;
    desired.samples = CHUNK_LEN_SAMPLES;
    desired.callback = audio_callback;

    /* Open the audio device, communicating desired settings via third arg */
    SDL_AudioDeviceID device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);

    
    /* TODO, maybe: query "obtained" audio spec to see what settings we ACTUALLY opened the device with. */
    

    /* Set default freq and amp values */
    tone_freq = 220;
    tone_amp = 0.1;

    /* Define rectangle in which to draw waveform  ({x, y, w, h}, where (x,y) is the top-left corner) */
    int waveform_x_pad = 20;
    int waveform_h = 200;
    int waveform_y = WINDOW_H / 2 - waveform_h / 2;
    SDL_Rect waveform_rect = (SDL_Rect) {
	waveform_x_pad,
	waveform_y,
	WINDOW_W - (2 * waveform_x_pad),
	waveform_h
    };
    

    /* MAIN LOOP. Each iteration is a frame. We draw stuff and handle events every frame. */
    bool quit = false;
    while (!quit) {
	
	/* Declare an SDL_Event union which will store data for the event currently being handled */
	SDL_Event e;

	/* SDL_PollEvent pops the next event off the queue, storing it in 'e',
	   and return 0 when there are no more events in the queue to handle. */
	
	while (SDL_PollEvent(&e)) {
	    if (e.type == SDL_QUIT) {
		quit = true;
	    } else if (e.type == SDL_KEYDOWN) {
		switch (e.key.keysym.scancode) {
		case SDL_SCANCODE_L:
		    SDL_PauseAudioDevice(device, 0);
		    break;
		case SDL_SCANCODE_K:
		    SDL_PauseAudioDevice(device, 1);
		    break;
		default:
		    break;
		}
	    } else if (e.type == SDL_MOUSEMOTION) {
		/* Get mouse pos as a proportion of window dimensions */
		double mouse_x_relative = (double)e.motion.x / WINDOW_W;
		double mouse_y_relative = 1.0 - (double)e.motion.y / WINDOW_H;
		
		tone_amp = mouse_y_relative;

		/* Scale freq between reasonable frequency values */
		tone_freq = 40 + 1000 * mouse_x_relative;
		
	    }
	}

	/* Set a color and call SDL_RenderClear at the beginning of every frame. In basically every SDL application. */
	SDL_SetRenderDrawColor(rend, 255, 255, 255, 255);
	SDL_RenderClear(rend);

	/* Set the draw color to cyan and fill in the waveform rect */
	SDL_SetRenderDrawColor(rend, 0, 255, 255, 255);
	SDL_RenderFillRect(rend, &waveform_rect);

	/* Set the draw color to red and draw the waveform */
	SDL_SetRenderDrawColor(rend, 255, 0, 0, 255);
	draw_waveform(rend, &waveform_rect);


	/* Present the backbuffer (everything drawn to the renderer up until this point) to the screen */
	SDL_RenderPresent(rend);

	/* Free up some CPU cycles for other programs. We don't need much. */
	SDL_Delay(10);
    }
}
