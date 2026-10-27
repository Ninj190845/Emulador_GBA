#include "memoria.hpp"

#include <sstream>
#include <iomanip>

Mem::Mem()
{
    // Redimensiona o buffer da ROM para 32 MB de forma segura no Heap
    rom.resize(1024 * 1024 * 32, 0);
}

PhysicalAddr Mem::resolve_addr(uint32_t endereco)
{
    uint8_t secao = (endereco >> 24) & 0xFF;
    PhysicalAddr res;

    switch (secao)
    {
    case 0x00:
        res = {bios, endereco & 0x003FFF};
        break;
    case 0x02:
        res = {ewram, endereco & 0x03FFFF};
        break;
    case 0x03:
        res = {iwram, endereco & 0x007FFF};
        break;
    case 0x04:
        res = {ior, endereco & 0x0003FF};
        break;
    case 0x05:
        res = {palleteRam, endereco & 0x0003FF};
        break;
    case 0x06:
    {
        uint32_t off = endereco & 0x01FFFF;
        if (off >= 0x18000)
        {
            off -= 0x8000; // Espelhamento (Mirroring) da VRAM
        }
        res = {vram, off};
        break;
    }
    case 0x07:
        res = {oam, endereco & 0x0003FF};
        break;
    case 0x08:
    case 0x09:
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0D:
        res = {rom.data(), endereco & 0x01FFFFFF};
        break;
    case 0x0E:
    case 0x0F:
        res = {sram, endereco & 0x00FFFF};
        break;
    default:
        res = {nullptr, 0};
        break;
    }

    if (!res.ponteiro)
    {
        std::ostringstream ss;
        ss << "[MEM][resolve_addr]: ERRO - Tentativa de acessar endereco nao mapeado/invalido: 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << endereco
           << " (Secao: 0x" << std::setw(2) << static_cast<int>(secao) << ")";
        LOG_MSG(ss.str());
    }

    return res;
}

// Funções de Leitura
OpResult Mem::get8(uint32_t endereco)
{
    PhysicalAddr addr = resolve_addr(endereco);
    if (!addr.ponteiro)
    {
        std::ostringstream ss;
        ss << "[MEM][get8]: ERRO - Falha na leitura de 8-bit no endereco 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << endereco;
        LOG_MSG(ss.str());
        return {0, 0, StCode::FAIL};
    }

    uint8_t secao = (endereco >> 24) & 0xFF;
    uint8_t ciclos = 1;

    if (secao >= 0x08 && secao <= 0x0E)
        ciclos = 5;
    else if (secao == 0x02)
        ciclos = 3;

    uint8_t valor = addr.ponteiro[addr.endereco_mascarado];
    return {valor, ciclos, StCode::SUCESS};
}

OpResult Mem::get16(uint32_t endereco)
{
    if (endereco & 0x1)
    {
        std::ostringstream ss;
        ss << "[MEM][get16]: WARN - Leitura de 16-bit desalinhada no endereco 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << endereco;
        LOG_MSG(ss.str());
    }

    PhysicalAddr addr = resolve_addr(endereco);
    if (!addr.ponteiro)
    {
        std::ostringstream ss;
        ss << "[MEM][get16]: ERRO - Falha na leitura de 16-bit no endereco 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << endereco;
        LOG_MSG(ss.str());
        return {0, 0, StCode::FAIL};
    }

    uint8_t secao = (endereco >> 24) & 0xFF;
    uint8_t ciclos = 1;

    if (secao >= 0x08 && secao <= 0x0E)
        ciclos = 5;
    else if (secao == 0x02)
        ciclos = 3;

    if (secao >= 0x0E)
    {
        uint16_t b = addr.ponteiro[addr.endereco_mascarado];
        uint16_t valor = static_cast<uint16_t>((b << 8) | b);
        return {valor, ciclos, StCode::SUCESS};
    }

    uint32_t base = addr.endereco_mascarado & ~1;
    uint16_t valor = addr.ponteiro[base] | (addr.ponteiro[base + 1] << 8);

    return {valor, ciclos, StCode::SUCESS};
}

OpResult Mem::get32(uint32_t endereco)
{
    if (endereco & 0x3)
    {
        std::ostringstream ss;
        ss << "[MEM][get32]: WARN - Leitura de 32-bit desalinhada no endereco 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << endereco;
        LOG_MSG(ss.str());
    }

    PhysicalAddr addr = resolve_addr(endereco);
    if (!addr.ponteiro)
    {
        std::ostringstream ss;
        ss << "[MEM][get32]: ERRO - Falha na leitura de 32-bit no endereco 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << endereco;
        LOG_MSG(ss.str());
        return {0, 0, StCode::FAIL};
    }

    uint8_t secao = (endereco >> 24) & 0xFF;
    uint8_t ciclos = 1;

    if (secao >= 0x08 && secao <= 0x0E)
        ciclos = 8;
    else if (secao == 0x02)
        ciclos = 6;
    else if (secao == 0x05 || secao == 0x06)
        ciclos = 2;

    if (secao >= 0x0E)
    {
        uint32_t b = addr.ponteiro[addr.endereco_mascarado];
        uint32_t valor = (b << 24) | (b << 16) | (b << 8) | b;
        return {valor, ciclos, StCode::SUCESS};
    }

    uint32_t base = addr.endereco_mascarado & ~3;
    uint32_t valor = static_cast<uint32_t>(addr.ponteiro[base]) |
                     (static_cast<uint32_t>(addr.ponteiro[base + 1]) << 8) |
                     (static_cast<uint32_t>(addr.ponteiro[base + 2]) << 16) |
                     (static_cast<uint32_t>(addr.ponteiro[base + 3]) << 24);

    return {valor, ciclos, StCode::SUCESS};
}

// Funções de Escrita
void Mem::set8(uint32_t endereco, uint8_t valor)
{
    PhysicalAddr addr = resolve_addr(endereco);
    if (!addr.ponteiro)
    {
        std::ostringstream ss;
        ss << "[MEM][set8]: ERRO - Escrita de byte cancelada (ponteiro invalido) no endereco 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << endereco;
        LOG_MSG(ss.str());
        return;
    }

    uint8_t secao = (endereco >> 24) & 0xFF;

    if (secao == 0x00 || (secao >= 0x08 && secao <= 0x0D) || secao == 0x07)
    {
        std::ostringstream ss;
        ss << "[MEM][set8]: WARN - Tentativa de escrita em regiao protegida/somente leitura (Secao 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(secao)
           << ") no endereco 0x" << std::setw(8) << endereco;
        LOG_MSG(ss.str());
        return;
    }

    if (secao == 0x05 || secao == 0x06)
    {
        uint32_t base = addr.endereco_mascarado & ~1;
        addr.ponteiro[base] = valor;
        addr.ponteiro[base + 1] = valor;
        return;
    }

    addr.ponteiro[addr.endereco_mascarado] = valor;
}

void Mem::set16(uint32_t endereco, uint16_t valor)
{
    if (endereco & 0x1)
    {
        std::ostringstream ss;
        ss << "[MEM][set16]: WARN - Escrita de 16-bit desalinhada no endereco 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << endereco;
        LOG_MSG(ss.str());
    }

    PhysicalAddr addr = resolve_addr(endereco);
    if (!addr.ponteiro)
    {
        std::ostringstream ss;
        ss << "[MEM][set16]: ERRO - Escrita de halfword cancelada (ponteiro invalido) no endereco 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << endereco;
        LOG_MSG(ss.str());
        return;
    }

    uint8_t secao = (endereco >> 24) & 0xFF;

    if (secao == 0x00 || (secao >= 0x08 && secao <= 0x0D))
    {
        std::ostringstream ss;
        ss << "[MEM][set16]: WARN - Tentativa de escrita em regiao protegida/somente leitura (Secao 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(secao)
           << ") no endereco 0x" << std::setw(8) << endereco;
        LOG_MSG(ss.str());
        return;
    }

    if (secao >= 0x0E)
    {
        addr.ponteiro[addr.endereco_mascarado] = valor & 0xFF;
        return;
    }

    uint32_t base = addr.endereco_mascarado & ~1;
    addr.ponteiro[base] = valor & 0xFF;
    addr.ponteiro[base + 1] = (valor >> 8) & 0xFF;
}

void Mem::set32(uint32_t endereco, uint32_t valor)
{
    if (endereco & 0x3)
    {
        std::ostringstream ss;
        ss << "[MEM][set32]: WARN - Escrita de 32-bit desalinhada no endereco 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << endereco;
        LOG_MSG(ss.str());
    }

    PhysicalAddr addr = resolve_addr(endereco);
    if (!addr.ponteiro)
    {
        std::ostringstream ss;
        ss << "[MEM][set32]: ERRO - Escrita de word cancelada (ponteiro invalido) no endereco 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << endereco;
        LOG_MSG(ss.str());
        return;
    }

    uint8_t secao = (endereco >> 24) & 0xFF;

    if (secao == 0x00 || (secao >= 0x08 && secao <= 0x0D))
    {
        std::ostringstream ss;
        ss << "[MEM][set32]: WARN - Tentativa de escrita em regiao protegida/somente leitura (Secao 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(secao)
           << ") no endereco 0x" << std::setw(8) << endereco;
        LOG_MSG(ss.str());
        return;
    }

    if (secao >= 0x0E)
    {
        addr.ponteiro[addr.endereco_mascarado] = valor & 0xFF;
        return;
    }

    uint32_t base = addr.endereco_mascarado & ~3;
    addr.ponteiro[base] = valor & 0xFF;
    addr.ponteiro[base + 1] = (valor >> 8) & 0xFF;
    addr.ponteiro[base + 2] = (valor >> 16) & 0xFF;
    addr.ponteiro[base + 3] = (valor >> 24) & 0xFF;
}

PhysicalAddr Mem::silent_resolve_addr(uint32_t endereco)
{
    uint8_t secao = (endereco >> 24) & 0xFF;

    switch (secao)
    {
    case 0x00:
        return {bios, endereco & 0x003FFF};
    case 0x02:
        return {ewram, endereco & 0x03FFFF};
    case 0x03:
        return {iwram, endereco & 0x007FFF};
    case 0x04:
        return {ior, endereco & 0x0003FF};
    case 0x05:
        return {palleteRam, endereco & 0x0003FF};
    case 0x06:
    {
        uint32_t off = endereco & 0x01FFFF;
        if (off >= 0x18000)
            off -= 0x8000;
        return {vram, off};
    }
    case 0x07:
        return {oam, endereco & 0x0003FF};
    case 0x08:
    case 0x09:
    
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0D:
        return {rom.data(), endereco & 0x01FFFFFF};
    case 0x0E:
    case 0x0F:
        return {sram, endereco & 0x00FFFF};
    default:
        return {nullptr, 0};
    }
}

void Mem::poke16(uint32_t endereco, uint16_t valor)
{
    PhysicalAddr addr = silent_resolve_addr(endereco);
    if (!addr.ponteiro)
        return;

    uint8_t secao = (endereco >> 24) & 0xFF;

    if (secao >= 0x0E)
    {
        addr.ponteiro[addr.endereco_mascarado] = valor & 0xFF;
        return;
    }

    uint32_t base = addr.endereco_mascarado & ~1;
    addr.ponteiro[base] = valor & 0xFF;
    addr.ponteiro[base + 1] = (valor >> 8) & 0xFF;
}

void Mem::poke32(uint32_t endereco, uint32_t valor)
{
    PhysicalAddr addr = silent_resolve_addr(endereco);
    if (!addr.ponteiro)
        return;

    uint8_t secao = (endereco >> 24) & 0xFF;

    if (secao >= 0x0E)
    {
        addr.ponteiro[addr.endereco_mascarado] = valor & 0xFF;
        return;
    }

    uint32_t base = addr.endereco_mascarado & ~3;
    addr.ponteiro[base] = valor & 0xFF;
    addr.ponteiro[base + 1] = (valor >> 8) & 0xFF;
    addr.ponteiro[base + 2] = (valor >> 16) & 0xFF;
    addr.ponteiro[base + 3] = (valor >> 24) & 0xFF;
}