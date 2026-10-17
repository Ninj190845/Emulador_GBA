#pragma once

#include <cstdint>
#include "common/structs.hpp"


struct physicalAddr
{
    uint8_t *ponteiro = nullptr;
    uint32_t endereçoMascarado = 0;
};

struct Mem
{
    // Buffers expostos para debug/cheats/salvamento
    uint8_t bios[1024 * 16] = {0};
    uint8_t ewram[1024 * 256] = {0};
    uint8_t iwram[1024 * 32] = {0};
    uint8_t ior[1024] = {0};
    uint8_t palleteRam[1024] = {0};
    uint8_t vram[1024 * 96] = {0};
    uint8_t oam[1024] = {0};
    uint8_t rom[1024 * 1024 * 32] = {0};
    uint8_t sram[1024 * 32] = {0};

    Mem() = default;
    ~Mem() = default;

    // Apenas os protótipos das funções (terminam com ;)
    physicalAddr resolve_addr(uint32_t endereço);

    OpResult get8(uint32_t endereço);
    OpResult get16(uint32_t endereço);
    OpResult get32(uint32_t endereço);

    void set8(uint32_t endereço, uint8_t valor);
    void set16(uint32_t endereço, uint16_t valor);
    void set32(uint32_t endereço, uint32_t valor);

    physicalAddr silent_resolve_addr(uint32_t endereço);
    void poke32(uint32_t endereço, uint32_t valor);
};