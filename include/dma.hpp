#pragma once

#include <cstdint>

enum class DmaTiming
{
    Immediate = 0,
    VBlank = 1,
    HBlank = 2,
    Special = 3
};

struct Mem;

class Dma
{
private:
    Mem *memoria = nullptr;

    // Executa a transferência de um canal específico (0 a 3)
    void transferir_canal(int canal);

public:
    Dma(Mem *mem) : memoria(mem) {}
    ~Dma() = default;

    // Disparado quando a CPU escreve nos registradores I/O de controle do DMA
    void check_dma_write(uint32_t endereco, uint16_t valor);

    // Disparado pela PPU durante VBlank ou HBlank
    void trigger(DmaTiming timing);
};