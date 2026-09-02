#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_rect.h>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <vector>

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *background_texture = NULL;
SDL_Texture *texture_X = NULL;
SDL_Texture *texture_O = NULL;
SDL_Texture *cell = NULL;
SDL_Texture *cell_highlight = NULL;
SDL_Texture *cell_lastmove = NULL;
SDL_Texture *texture_O_animation = NULL;
SDL_Texture *texture_X_animation = NULL;
int new_piece_x = -1; // New piece's x-index for animation
int new_piece_y = -1; // New piece's y-index for animation
int cell_highlight_x = -1; // Highlighted cell's x-index
int cell_highlight_y = -1; // Highlighted cell's y-index
int last_move_x = -1; // Last move's x-index
int last_move_y = -1; // Last move's y-index
Uint64 last_cell_update_time = 0;

#define WINDOW_WIDTH 900
#define WINDOW_HEIGHT 900
#define BOARD_DIMENSION 10
#define CELL_SIZE 80
#define ANIMATION_FRAMES 20

float board_size = BOARD_DIMENSION * CELL_SIZE;
float offset_width = WINDOW_WIDTH / 2.0f - board_size / 2.0f;
float offset_height = WINDOW_HEIGHT / 2.0f - board_size / 2.0f;

enum piece {
    PIECE_EMPTY = 0,
    PIECE_X = 1,
    PIECE_O = 2
};

piece board[BOARD_DIMENSION][BOARD_DIMENSION] = { PIECE_EMPTY };

void GenerateRandomMove();
bool DrawNewPieceAnimation(int x, int y);
void DrawPieces();
void DrawCaroBoard();
void FPSCounter(Uint64 start, Uint64 end);
SDL_AppResult Load_Texture(SDL_Texture*& texture, const char* file_path);
SDL_AppResult Load_Textures();

SDL_AppResult Load_Texture(SDL_Texture*& texture, const char* file_path) {
    SDL_Surface *surface = NULL;
    char *png_path = NULL;

    SDL_asprintf(&png_path, "%s%s", SDL_GetBasePath(), file_path);
    surface = SDL_LoadPNG(png_path);
    if (!surface) {
        SDL_Log("Couldn't load png: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_free(png_path);

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_Log("Couldn't create texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_DestroySurface(surface);

    return SDL_APP_CONTINUE;
}

SDL_AppResult Load_Textures() {
    SDL_AppResult result = SDL_APP_CONTINUE;
    
    result = Load_Texture(background_texture, "Resources/background.png");
    if (result != SDL_APP_CONTINUE) {
        SDL_Log("Failed to load background texture");
        return result;
    }    
    
    result = Load_Texture(texture_X, "Resources/X.png");
    if (result != SDL_APP_CONTINUE) {
        SDL_Log("Failed to load texture X");
        return result;
    }

    result = Load_Texture(texture_O, "Resources/O.png");
    if (result != SDL_APP_CONTINUE) {
        SDL_Log("Failed to load texture O");
        return result;
    }

    result = Load_Texture(cell, "Resources/cell.png");
    if (result != SDL_APP_CONTINUE) {
        SDL_Log("Failed to load texture Cell");
        return result;
    }

    result = Load_Texture(cell_highlight, "Resources/cell_highlight.png");
    if (result != SDL_APP_CONTINUE) {
        SDL_Log("Failed to load texture Cell_highlight");
        return result;
    }

    result = Load_Texture(cell_lastmove, "Resources/cell_last_move.png");
    if (result != SDL_APP_CONTINUE) {
        SDL_Log("Failed to load texture Cell_lastmove");
        return result;
    }

    result = Load_Texture(texture_O_animation, "Resources/O_animation.png");
    if (result != SDL_APP_CONTINUE) {
        SDL_Log("Failed to load texture O_animation");
        return result;
    }

    result = Load_Texture(texture_X_animation, "Resources/X_animation.png");
    if (result != SDL_APP_CONTINUE) {
        SDL_Log("Failed to load texture O_animation");
        return result;
    }

    return result;
}

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_SetAppMetadata("Caro", "1.0", "QuangTV.Caro");
    SDL_Log("App Init");


    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("Caro", WINDOW_WIDTH, WINDOW_HEIGHT, NULL, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    std::srand (std::time ( nullptr ));

    return Load_Textures();
}


piece piece_choose = PIECE_X;
/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }

    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        float mouse_x = event->motion.x;
        float mouse_y = event->motion.y;

        if (mouse_x < offset_width || mouse_x >= offset_width + board_size ||
            mouse_y < offset_height || mouse_y >= offset_height + board_size)
        {
            cell_highlight_x = -1;
            cell_highlight_y = -1;
        } else {
            cell_highlight_x = ((mouse_x - offset_width) / CELL_SIZE);
            cell_highlight_y = ((mouse_y - offset_height) / CELL_SIZE);
        }
    }

    if (event->type ==SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT) {
        SDL_Log("Left button down at (%.0f, %.0f)", event->button.x, event->button.y);
        if (new_piece_x != -1 && new_piece_y != -1) {
            SDL_Log("Animation in progress, ignoring click.");
            return SDL_APP_CONTINUE;
        }

        float mouse_x = event->button.x;
        float mouse_y = event->button.y;

        // Check if mouse is outside board
        if (mouse_x < offset_width || mouse_x >= offset_width + board_size ||
            mouse_y < offset_height || mouse_y >= offset_height + board_size)
        {
            SDL_Log("Click outside board, %.0f, %.0f, %.0f, %.0f", offset_width, offset_height, offset_width + board_size, offset_height + board_size);
            return SDL_APP_CONTINUE;
        }

        int x = ((mouse_x - offset_width) / CELL_SIZE);

        int y = ((mouse_y - offset_height) / CELL_SIZE);

        SDL_Log("Clicked cell [%d][%d]", x, y);

        if (board[x][y] != PIECE_EMPTY) {
            return SDL_APP_CONTINUE;
        }

        piece_choose = (piece_choose == PIECE_X) ? PIECE_O : PIECE_X; // Toggle piece for next move
        board[x][y] = piece_choose;
        new_piece_x = x;
        new_piece_y = y;
        last_move_x = x;
        last_move_y = y;
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

void FPSCounter(Uint64 start, Uint64 end)
{
    const float elapsed = (end - start) / (float)SDL_GetPerformanceFrequency();
    float framerate = 1.f / elapsed;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);  /* black, full alpha */
    SDL_RenderDebugTextFormat(renderer, 5, 5, "FPS: %.0f", framerate);
}

static size_t animation_index = 0;
static Uint64 last_animation_update = 0;
void DrawCaroBoard()
{

    SDL_RenderTexture(renderer, background_texture, NULL, NULL);

    for (int i = 0; i < BOARD_DIMENSION; ++i) {
        for (int j = 0; j < BOARD_DIMENSION; ++j) {
            SDL_FRect cell_rect = {offset_width + i * CELL_SIZE, offset_height + j * CELL_SIZE, CELL_SIZE, CELL_SIZE};
            if (i == cell_highlight_x && j == cell_highlight_y) {
                SDL_RenderTexture(renderer, cell_highlight, NULL, &cell_rect);
            } else {
                SDL_RenderTexture(renderer, cell, NULL, &cell_rect);
            }
        }        
    }

    SDL_FRect last_move_rect = {offset_width + last_move_x * CELL_SIZE, offset_height + last_move_y * CELL_SIZE, CELL_SIZE, CELL_SIZE};
    if (last_move_x != -1 && last_move_y != -1) {
        SDL_RenderTexture(renderer, cell_lastmove, NULL, &last_move_rect);
    }

    Uint64 current_time = SDL_GetTicks();
    int interval = 1000;
    if (current_time - last_cell_update_time >= interval)
    {
        // GenerateRandomMove();
        last_cell_update_time = current_time;     
    }

    if (new_piece_x != -1 && new_piece_y != -1 && DrawNewPieceAnimation(new_piece_x, new_piece_y)) {
        animation_index = 0; // Reset animation index for next time
        // Reset new_x and new_y after the animation is done
        new_piece_x = -1; 
        new_piece_y = -1;
    }
    DrawPieces();
}

void GenerateRandomMove()
{
    new_piece_x = std::rand() % BOARD_DIMENSION;
    new_piece_y = std::rand() % BOARD_DIMENSION;

    if (board[new_piece_x][new_piece_y] != PIECE_EMPTY) {
        GenerateRandomMove(); // Try again if the cell is not empty
    } else {
        // board[new_x][new_y] = (std::rand() % 2 == 0) ? PIECE_X : PIECE_O;
        board[new_piece_x][new_piece_y] = PIECE_O;
    }
}

bool DrawNewPieceAnimation(int x, int y)
{
    Uint64 current_time = SDL_GetTicks();
    SDL_Texture* texture_to_draw = (board[x][y] == PIECE_X) ? texture_X_animation : texture_O_animation;
    SDL_FRect cell_i_rect = {offset_width + x * CELL_SIZE, offset_height + y * CELL_SIZE, CELL_SIZE, CELL_SIZE};

    size_t nbRow = 3;
    size_t nbCol = 4;
    size_t widthSpr = 512;
    size_t heightSpr = 512;
    std::vector<SDL_FRect> rects;
    for (size_t i = 0; i < nbRow; i++) {
        for (size_t j = 0; j < nbCol; j++) {
            rects.push_back(SDL_FRect{  static_cast<float>(j * widthSpr),
                                        static_cast<float>(i * heightSpr),
                                        static_cast<float>(widthSpr), 
                                        static_cast<float>(heightSpr) });
        }
    }
    std::vector<std::pair<size_t, size_t>> animation { 
        {0, 0}, {0, 1}, {0, 2}, {0, 3},
        {1, 0}, {1, 1}, {1, 2}, {1, 3},
        {2, 0}, {2, 1}, {2, 2}, {2, 3}
    };

    if (current_time - last_animation_update >= ANIMATION_FRAMES) {
        animation_index++;

        if (animation_index >= animation.size()) {
            return true; // Animation finished
        }

        last_animation_update = current_time;
    }

    size_t row = animation[animation_index].first;
    size_t col = animation[animation_index].second;

    size_t pos = col + row * nbCol;

    SDL_FRect src_rect = rects[pos];

    SDL_FRect dst_rect = {
        offset_width + x * CELL_SIZE,
        offset_height + y * CELL_SIZE,
        CELL_SIZE,
        CELL_SIZE
    };

    SDL_RenderTexture(
        renderer,
        texture_to_draw,
        &src_rect,
        &dst_rect
    );

    return false; // Animation not finished
}

void DrawPieces()
{
    for (int i = 0; i < BOARD_DIMENSION; ++i) {
        for (int j = 0; j < BOARD_DIMENSION; ++j) {
            if (i == new_piece_x && j == new_piece_y) {
                continue; // Skip drawing the new piece here, it's handled in the animation
            }

            SDL_FRect piece_rect = {offset_width + i * CELL_SIZE, offset_height + j * CELL_SIZE, CELL_SIZE, CELL_SIZE};
            if (board[i][j] == PIECE_X) {
                SDL_RenderTexture(renderer, texture_X, NULL, &piece_rect);
            } else if (board[i][j] == PIECE_O) {
                SDL_RenderTexture(renderer, texture_O, NULL, &piece_rect);
            }
        }
    }
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    const Uint64 start = SDL_GetPerformanceCounter();

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
    DrawCaroBoard();

    const Uint64 end = SDL_GetPerformanceCounter();
    FPSCounter(start, end);

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    /* SDL will clean up the window/renderer for us. */
}