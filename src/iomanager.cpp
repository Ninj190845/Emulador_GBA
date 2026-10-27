#include "iomanager.hpp"
#include <iostream>
#include <cstdint>

IOManager::IOManager(int escala)
{
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0)
    {
        std::cerr << "[ERRO]:Erro no SDL_Init";
    }
    janela = SDL_CreateWindow(
        "Cotoc's GBA",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        240 * escala,
        160 * escala,
        SDL_WINDOW_SHOWN);

    renderizador = SDL_CreateRenderer(janela, -1, SDL_RENDERER_ACCELERATED);

    textura = SDL_CreateTexture(
        renderizador,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        240,
        160);
};

inline void set_bit16(uint16_t &registrador, int bit, bool estado)
{
    if (estado)
    {
        registrador |= (1 << bit); // Ativa o bit (1)
    }
    else
    {
        registrador &= ~(1 << bit); // Zera o bit (0)
    }
}

void IOManager::render_frame(const uint32_t *frameBuffer)
{
    SDL_UpdateTexture(
        textura,
        NULL,
        frameBuffer,
        (240 * sizeof(uint32_t)));

    SDL_RenderClear(renderizador);
    SDL_RenderCopy(
        renderizador,
        textura,
        NULL,
        NULL);
    SDL_RenderPresent(renderizador);
};

bool IOManager::process_events(uint16_t &gbaKeyInput)
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
        {
            return false;
        }
        // TECLA PRESSIONADA -> seta o bit para 0 (false)
        else if (event.type == SDL_KEYDOWN)
        {
            switch (event.key.keysym.sym)
            {
            case SDLK_z:
                set_bit16(gbaKeyInput, 0, false);
                break; // Botão A
            case SDLK_x:
                set_bit16(gbaKeyInput, 1, false);
                break; // Botão B
            case SDLK_BACKSPACE:
                set_bit16(gbaKeyInput, 2, false);
                break; // Select
            case SDLK_RETURN:
                set_bit16(gbaKeyInput, 3, false);
                break; // Start
            case SDLK_RIGHT:
                set_bit16(gbaKeyInput, 4, false);
                break; // Direita
            case SDLK_LEFT:
                set_bit16(gbaKeyInput, 5, false);
                break; // Esquerda
            case SDLK_UP:
                set_bit16(gbaKeyInput, 6, false);
                break; // Cima
            case SDLK_DOWN:
                set_bit16(gbaKeyInput, 7, false);
                break; // Baixo
            case SDLK_e:
                set_bit16(gbaKeyInput, 8, false);
                break; // Botão R
            case SDLK_q:
                set_bit16(gbaKeyInput, 9, false);
                break; // Botão L
            }
        }
        // TECLA SOLTA -> seta o bit para 1 (true)
        else if (event.type == SDL_KEYUP)
        {
            switch (event.key.keysym.sym)
            {
            case SDLK_z:
                set_bit16(gbaKeyInput, 0, true);
                break; // Botão A
            case SDLK_x:
                set_bit16(gbaKeyInput, 1, true);
                break; // Botão B
            case SDLK_BACKSPACE:
                set_bit16(gbaKeyInput, 2, true);
                break; // Select
            case SDLK_RETURN:
                set_bit16(gbaKeyInput, 3, true);
                break; // Start
            case SDLK_RIGHT:
                set_bit16(gbaKeyInput, 4, true);
                break; // Direita
            case SDLK_LEFT:
                set_bit16(gbaKeyInput, 5, true);
                break; // Esquerda
            case SDLK_UP:
                set_bit16(gbaKeyInput, 6, true);
                break; // Cima
            case SDLK_DOWN:
                set_bit16(gbaKeyInput, 7, true);
                break; // Baixo
            case SDLK_e:
                set_bit16(gbaKeyInput, 8, true);
                break; // Botão R
            case SDLK_q:
                set_bit16(gbaKeyInput, 9, true);
                break; // Botão L
            }
        }
    }
    return true;
};

void IOManager::sync_60fps()
{
    static uint64_t tempo_ultimo_frame = SDL_GetTicks64();
    uint64_t tempo_atual = SDL_GetTicks64();
    uint64_t tempo_decorrido = tempo_atual - tempo_ultimo_frame;

    // 1000 ms / 59.73 FPS = ~16.74 ms por frame
    if (tempo_decorrido < 16)
    {
        SDL_Delay(16 - tempo_decorrido); // Pausa a CPU do PC pelo tempo restante
    }

    tempo_ultimo_frame = SDL_GetTicks64();
};