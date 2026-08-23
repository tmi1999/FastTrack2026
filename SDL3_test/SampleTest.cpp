/* clear.c ... */

/*
 * This example code creates an SDL window and renderer, and then clears the
 * window to a different color every frame, so you'll effectively get a window
 * that's smoothly fading between colors.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_rect.h>
#include <iostream>
#include <cstdlib>
#include <ctime>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture_X = NULL;
static SDL_Texture *texture_O = NULL;

static int i_x = 0;
static int i_y = 0;
static int j_x = 0;
static int j_y = 0;
static Uint64 last_cell_update = 0;

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 500
#define BOARD_DIMENSION 10
#define CELL_SIZE 40

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_SetAppMetadata("Example Renderer Clear", "1.0", "com.example.renderer-clear");
    SDL_Surface *surface_X = NULL;
    SDL_Surface *surface_O = NULL;
    char *X_png_path = NULL;
    char *O_png_path = NULL;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("Test 1", WINDOW_WIDTH, WINDOW_HEIGHT, NULL, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);

     /* Textures are pixel data that we upload to the video hardware for fast drawing. Lots of 2D
       engines refer to these as "sprites." We'll do a static texture (upload once, draw many
       times) with data from a png file. */

    /* SDL_Surface is pixel data the CPU can access. SDL_Texture is pixel data the GPU can access.
       Load a .png into a surface, move it to a texture from there. */
    SDL_asprintf(&X_png_path, "%sX.png", SDL_GetBasePath());  /* allocate a string of the full file path */
    SDL_asprintf(&O_png_path, "%sO.png", SDL_GetBasePath());  /* allocate a string of the full file path */
    surface_X = SDL_LoadPNG(X_png_path);
    surface_O = SDL_LoadPNG(O_png_path);
    if (!surface_X || !surface_O) {
        SDL_Log("Couldn't load png: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_free(X_png_path);  /* done with this, the file is loaded. */
    SDL_free(O_png_path);  /* done with this, the file is loaded. */

    texture_X = SDL_CreateTextureFromSurface(renderer, surface_X);
    texture_O = SDL_CreateTextureFromSurface(renderer, surface_O);
    if (!texture_X || !texture_O) {
        SDL_Log("Couldn't create static texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_DestroySurface(surface_X);  /* done with this, the texture has a copy of the pixels now. */
    SDL_DestroySurface(surface_O);  /* done with this, the texture has a copy of the pixels now. */

    std::srand (std::time ( nullptr ));

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

void ChangeColor()
{
    /* choose the color for the frame we will draw. The sine wave trick makes it fade between colors smoothly. */
    const float red = (float) (0.5 + 0.5 * SDL_sin(SDL_GetTicks() / 1000.0));
    const float green = (float) (0.5 + 0.5 * SDL_sin(SDL_GetTicks() / 1000.0 + SDL_PI_D * 2 / 3));
    const float blue = (float) (0.5 + 0.5 * SDL_sin(SDL_GetTicks() / 1000.0 + SDL_PI_D * 4 / 3));
    SDL_SetRenderDrawColorFloat(renderer, red, green, blue, SDL_ALPHA_OPAQUE_FLOAT);  /* new color, full alpha. */
}

void FPSCounter(Uint64 start, Uint64 end)
{
    const float elapsed = (end - start) / (float)SDL_GetPerformanceFrequency();
    float framerate = 1.f / elapsed;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);  /* black, full alpha */
    SDL_RenderDebugTextFormat(renderer, 5, 5, "FPS: %.0f", framerate);
}

void DrawText()
{
    const int charsize = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;


    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);  /* white, full alpha */
    SDL_RenderDebugText(renderer, 272, 100, "Hello world!");
    SDL_RenderDebugText(renderer, 224, 150, "This is some debug text.");

    SDL_SetRenderDrawColor(renderer, 51, 102, 255, SDL_ALPHA_OPAQUE);  /* light blue, full alpha */
    SDL_RenderDebugText(renderer, 184, 200, "You can do it in different colors.");
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);  /* white, full alpha */

    SDL_SetRenderScale(renderer, 4.0f, 4.0f);
    SDL_RenderDebugText(renderer, 14, 65, "It can be scaled.");
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
    SDL_RenderDebugText(renderer, 64, 350, "This only does ASCII chars. So this laughing emoji won't draw: 🤣");

    SDL_RenderDebugTextFormat(renderer, ((float) (WINDOW_WIDTH - (charsize * 46)) / 2), 400, "(This program has been running for %" SDL_PRIu64 " seconds.)", SDL_GetTicks() / 1000);
}

void DrawCaroBoard()
{
    float board_size = BOARD_DIMENSION * CELL_SIZE;
    float offset_width = WINDOW_WIDTH / 2.0f - board_size / 2.0f;
    float offset_height = WINDOW_HEIGHT / 2.0f - board_size / 2.0f;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    for (int i = 0; i <= BOARD_DIMENSION; ++i) {
        // Vertical lines
        SDL_RenderLine(renderer, offset_width + i * CELL_SIZE, offset_height, offset_width + i * CELL_SIZE, offset_height + board_size);
        // Horizontal lines
        SDL_RenderLine(renderer, offset_width, offset_height + i * CELL_SIZE, offset_width + board_size, offset_height + i * CELL_SIZE);
    }
    Uint64 current_time = SDL_GetTicks();

    int interval = 1000;
    if (current_time - last_cell_update >= interval)
    {
        i_x = std::rand() % BOARD_DIMENSION;
        i_y = std::rand() % BOARD_DIMENSION;

        j_x = std::rand() % BOARD_DIMENSION;
        j_y = std::rand() % BOARD_DIMENSION;

        last_cell_update = current_time;
    }

    SDL_FRect cell_i_rect = {offset_width + i_x * CELL_SIZE, offset_height + i_y * CELL_SIZE, CELL_SIZE, CELL_SIZE};
    SDL_FRect cell_j_rect = {offset_width + j_x * CELL_SIZE, offset_height + j_y * CELL_SIZE, CELL_SIZE, CELL_SIZE};
    SDL_RenderTexture(renderer, texture_O, NULL, &cell_i_rect);
    SDL_RenderTexture(renderer, texture_X, NULL, &cell_j_rect);
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    const Uint64 start = SDL_GetPerformanceCounter();

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
    ChangeColor();
    SDL_RenderClear(renderer);
    DrawCaroBoard();

    // DrawText();
    
    const Uint64 end = SDL_GetPerformanceCounter();
    FPSCounter(start, end);

    SDL_RenderPresent(renderer);  /* put it all on the screen! */

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    /* SDL will clean up the window/renderer for us. */
}