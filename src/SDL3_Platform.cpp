#include <iostream>
#include "SLD3_Platform.hpp"

bool SLD3Platform :: init() {
    // initialize SDL3
    if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::cerr << "SDL3 Init Failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // create window
    window = SDL_CreateWindow("CHIP-8 Emulator", gb_width * SCALE, gb_height * SCALE, SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE);
    if(!window) {
        std::cerr << "Window Creation Failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // create renderer
    renderer = SDL_CreateRenderer(window, nullptr);
    if(!renderer) {
        std::cerr << "Renderer Creation Failed: " << SDL_GetError() << std::endl;
        return false;
    }

     if(!SDL_SetRenderVSync(renderer, 1)) {
        return false;
    }

    // create texture
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, gb_width, gb_height);
    if(!texture) {
        std::cerr << "Texture Creation Failed: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    clear();
    return true;
}

// destructor
SDL3_platform :: ~SDL3_platform() {
    if(texture) {
        SDL_DestroyTexture(texture);
    }
    if(renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if(window) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
}

void SDL3_platform :: render(const uint32_t* frame_buffer) {
    SDL_RenderClear(renderer);
    SDL_UpdateTexture(texture, nullptr, front_buffer, gb_width * sizeof(uint32_t));
    SDL_RenderTexture(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}

void SDL3_platform :: handle_events(bool& running) {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            running = false;
            return;
        }
    }
}