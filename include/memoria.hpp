#pragma once

#include <cstdint>
#include <vector>

#include "common/config.hpp"
#include "common/structs.hpp"
#include "common/tools.hpp"

struct Dma;
struct Ppu;

struct PhysicalAddr
{
    uint8_t *ponteiro = nullptr;
    uint32_t endereco_mascarado = 0;
};

struct Mem
{
    uint8_t bios[1024 * 16] = {0};
    uint8_t ewram[1024 * 256] = {0};
    uint8_t iwram[1024 * 32] = {0};
    uint8_t ior[1024] = {0};
    uint8_t palleteRam[1024] = {0}; // Mantido palleteRam para casar com ppu.cpp
    uint8_t vram[1024 * 96] = {0};
    uint8_t oam[1024] = {0};
    uint8_t sram[1024 * 64] = {0};

    std::vector<uint8_t> rom;

    Dma *dma = nullptr;
    Ppu *ppu = nullptr;

    Mem();
    ~Mem() = default;

    PhysicalAddr resolve_addr(uint32_t endereco);
    PhysicalAddr silent_resolve_addr(uint32_t endereco);

    OpResult get8(uint32_t endereco);
    OpResult get16(uint32_t endereco);
    OpResult get32(uint32_t endereco);

    void set8(uint32_t endereco, uint8_t valor);
    void set16(uint32_t endereco, uint16_t valor);
    void set32(uint32_t endereco, uint32_t valor);

    void poke16(uint32_t endereco, uint16_t valor);
    void poke32(uint32_t endereco, uint32_t valor);
};