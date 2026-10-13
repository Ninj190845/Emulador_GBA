#pragma once
#include <cstdint>

struct endereçoFisico
{
    uint8_t *ponteiro = nullptr;
    uint32_t endereçoMascarado = 0;
};

struct memReturnStatus
{
    uint32_t valor = 0;
    uint8_t ciclos = 0;
    bool sucesso = false;
};

struct Memoria
{
    // Buffers expostos para debug/cheats/salvamento
    uint8_t bios[1024 * 16];
    uint8_t ewram[1024 * 256];
    uint8_t iwram[1024 * 32];
    uint8_t ior[1024];
    uint8_t palleteRam[1024];
    uint8_t vram[1024 * 96];
    uint8_t oam[1024];
    uint8_t rom[1024 * 1024 * 32];
    uint8_t sram[1024 * 32];

    Memoria() = default;
    ~Memoria() = default;

    // Apenas os protótipos das funções (terminam com ;)
    endereçoFisico resolver_endereço(uint32_t endereço);

    memReturnStatus get8(uint32_t endereço);
    memReturnStatus get16(uint32_t endereço);
    memReturnStatus get32(uint32_t endereço);

    void set8(uint32_t endereço, uint8_t valor);
    void set16(uint32_t endereço, uint16_t valor);
    void set32(uint32_t endereço, uint32_t valor);
};