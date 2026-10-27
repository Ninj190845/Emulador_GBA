#pragma once

#include <SDL2/SDL.h>
#include <cstdint>
#include <vector>

class IOManager
{
private:
    SDL_Window *janela;
    SDL_Renderer *renderizador;
    SDL_Texture *textura;

public:
    IOManager(int escala);
    ~IOManager() = default;

    void render_frame(const uint32_t* frameBuffer);
    bool process_events(uint16_t &gbaInputBytes);
    void sync_60fps();
};
