#include "memoria.hpp"

endereçoFisico Memoria::resolver_endereço(uint32_t endereço)
{
    uint8_t seção = (endereço >> 24) & 0xFF;

    switch (seção)
    {
    case 0x00:
        return {bios, endereço & 0x003FFF};
    case 0x02:
        return {ewram, endereço & 0x03FFFF};
    case 0x03:
        return {iwram, endereço & 0x007FFF};
    case 0x04:
        return {ior, endereço & 0x0003FF};
    case 0x05:
        return {palleteRam, endereço & 0x0003FF};
    case 0x06:
    {
        uint32_t off = endereço & 0x01FFFF;
        if (off >= 0x18000)
            off -= 0x8000;
        return {vram, off};
    }
    case 0x07:
        return {oam, endereço & 0x0003FF};
    case 0x08:
    case 0x09:
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0D:
        return {rom, endereço & 0x01FFFFFF};
    case 0x0E:
    case 0x0F:
        return {sram, endereço & 0x00FFFF};
    default:
        return {nullptr, 0};
    }
}

// Funções de Leitura
memReturnStatus Memoria::get8(uint32_t endereço)
{
    endereçoFisico addr = resolver_endereço(endereço);
    if (!addr.ponteiro)
        return {0, 0, false};

    uint8_t seção = (endereço >> 24) & 0xFF;
    uint8_t ciclos = 1;

    if (seção >= 0x08 && seção <= 0x0E)
        ciclos = 5;
    else if (seção == 0x02)
        ciclos = 3;

    return {addr.ponteiro[addr.endereçoMascarado], ciclos, true};
}
memReturnStatus Memoria::get16(uint32_t endereço)
{
    endereçoFisico addr = resolver_endereço(endereço);
    if (!addr.ponteiro)
        return {0, 0, false};

    uint8_t seção = (endereço >> 24) & 0xFF;
    uint8_t ciclos = 1;

    if (seção >= 0x08 && seção <= 0x0E)
        ciclos = 5;
    else if (seção == 0x02)
        ciclos = 3;

    if (seção >= 0x0E)
    {
        uint16_t b = addr.ponteiro[addr.endereçoMascarado];
        return {(uint16_t)((b << 8) | b), ciclos, true};
    }

    uint32_t base = addr.endereçoMascarado & ~1;
    uint16_t valor = addr.ponteiro[base] | (addr.ponteiro[base + 1] << 8);
    return {valor, ciclos, true};
}
memReturnStatus Memoria::get32(uint32_t endereço)
{
    endereçoFisico addr = resolver_endereço(endereço);
    if (!addr.ponteiro)
        return {0, 0, false};

    uint8_t seção = (endereço >> 24) & 0xFF;
    uint8_t ciclos = 1;

    if (seção >= 0x08 && seção <= 0x0E)
        ciclos = 8;
    else if (seção == 0x02)
        ciclos = 6;
    else if (seção == 0x05 || seção == 0x06)
        ciclos = 2;

    if (seção >= 0x0E)
    {
        uint32_t b = addr.ponteiro[addr.endereçoMascarado];
        return {(b << 24) | (b << 16) | (b << 8) | b, ciclos, true};
    }

    uint32_t base = addr.endereçoMascarado & ~3;
    uint32_t valor = (uint32_t)addr.ponteiro[base] |
                     ((uint32_t)addr.ponteiro[base + 1] << 8) |
                     ((uint32_t)addr.ponteiro[base + 2] << 16) |
                     ((uint32_t)addr.ponteiro[base + 3] << 24);
    return {valor, ciclos, true};
}

// Funções de Escrita
void Memoria::set8(uint32_t endereço, uint8_t valor)
{
    endereçoFisico addr = resolver_endereço(endereço);
    if (!addr.ponteiro)
        return;

    uint8_t seção = (endereço >> 24) & 0xFF;
    if (seção == 0x00 || (seção >= 0x08 && seção <= 0x0D) || seção == 0x07)
        return;

    if (seção == 0x05 || seção == 0x06)
    {
        uint32_t base = addr.endereçoMascarado & ~1;
        addr.ponteiro[base] = valor;
        addr.ponteiro[base + 1] = valor;
        return;
    }

    addr.ponteiro[addr.endereçoMascarado] = valor;
}
void Memoria::set16(uint32_t endereço, uint16_t valor)
{
    endereçoFisico addr = resolver_endereço(endereço);
    if (!addr.ponteiro)
        return;

    uint8_t seção = (endereço >> 24) & 0xFF;
    if (seção == 0x00 || (seção >= 0x08 && seção <= 0x0D))
        return;

    if (seção >= 0x0E)
    {
        addr.ponteiro[addr.endereçoMascarado] = valor & 0xFF;
        return;
    }

    uint32_t base = addr.endereçoMascarado & ~1;
    addr.ponteiro[base] = valor & 0xFF;
    addr.ponteiro[base + 1] = (valor >> 8) & 0xFF;
}
void Memoria::set32(uint32_t endereço, uint32_t valor)
{
    endereçoFisico addr = resolver_endereço(endereço);
    if (!addr.ponteiro)
        return;

    uint8_t seção = (endereço >> 24) & 0xFF;
    if (seção == 0x00 || (seção >= 0x08 && seção <= 0x0D))
        return;

    if (seção >= 0x0E)
    {
        addr.ponteiro[addr.endereçoMascarado] = valor & 0xFF;
        return;
    }

    uint32_t base = addr.endereçoMascarado & ~3;
    addr.ponteiro[base] = valor & 0xFF;
    addr.ponteiro[base + 1] = (valor >> 8) & 0xFF;
    addr.ponteiro[base + 2] = (valor >> 16) & 0xFF;
    addr.ponteiro[base + 3] = (valor >> 24) & 0xFF;
}