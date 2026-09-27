#ifndef SDL3_PLATFORM_HPP
#define SDL3_PLATFORM_HPP
#include <SDL3/SDL.h>

class SDL3Platform {
    private:
        SDL_Window* window{};
        SDL_Renderer* renderer{};
        SDL_Texture* texture{};

        static constexpr int gb_width  = 160;
        static constexpr int gb_height = 144;
        int scale = 10;

    public:
        // constructor and destructor
        SDL3Platform() = default;
        ~SDL3Platform();

        // delete copy constructor and assignment operator
        SDL3_platform(const SDL3_platform&) = delete;
        SDL3_platform& operator=(const SDL3_platform&) = delete;

        // initialize SDL3, create window, renderer, and texture
        bool init();
        void render_frame(const uint32_t* frame_buffer);
        void handle_events(bool& running);

};


#endif // SDL3_PLATFORM_HPP