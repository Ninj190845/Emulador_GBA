#pragma once

#include <cstdint>
#include "memoria.hpp"

using uint = unsigned int;

class Ppu
{
private:
    Mem *memoria = nullptr;

    uint ciclos_acumulados = 0;
    uint16_t yPointer = 0;
    bool pixelNulo = false;

    // Leitura rápida de registradores I/O (DISPCNT, BGxCNT, etc.)
    uint16_t read_io16(uint32_t addr);
    uint32_t read_io32(uint32_t addr);

    // Métodos de renderização usando a struct Mem
    void render_scanline();
    void render_mode0();
    void render_mode1();
    void render_mode2();
    void render_mode3();
    void render_mode4();
    void render_mode5();
    void render_sprites();

    void render_text_bg(int bg, uint32_t line_buffer[240]);
    void render_affine_bg(int bg, uint32_t line_buffer[240]);

public:
    uint32_t frameBuffer[240 * 160] = {0};

    Ppu(Mem *mem) : memoria(mem) {}
    ~Ppu() = default;

    void step(uint ciclos);
};