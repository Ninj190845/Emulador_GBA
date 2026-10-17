#include "memoria.hpp"
#include <fstream>
#include <filesystem>
#include <iomanip>

static bool inicializar_sistema_de_logs()
{
    std::filesystem::create_directories("logs");
    return true;
}

// A variável global força a chamada da função antes da main()
static const bool log_iniciado = inicializar_sistema_de_logs();

std::ofstream logFileMem("logs/mem.log");
uint indent_levelMem = 0;

std::string get_indent_mem()
{
    return std::string(indent_levelMem, '\t');
}

physicalAddr Mem::resolve_addr(uint32_t endereço)
{
    logFileMem << get_indent_mem() << "[Mem::resolve_addr] {\n";
    indent_levelMem++;

    uint8_t seção = (endereço >> 24) & 0xFF;
    logFileMem << get_indent_mem() << "Endereço Solicitado: 0x" << std::hex << endereço << std::dec
               << " (Seção: 0x" << std::hex << +seção << std::dec << ")\n";

    physicalAddr res;
    const char *regiao = "Desconhecida / Inválida";

    switch (seção)
    {
    case 0x00:
        regiao = "BIOS";
        res = {bios, endereço & 0x003FFF};
        break;
    case 0x02:
        regiao = "EWRAM";
        res = {ewram, endereço & 0x03FFFF};
        break;
    case 0x03:
        regiao = "IWRAM";
        res = {iwram, endereço & 0x007FFF};
        break;
    case 0x04:
        regiao = "I/O Registers";
        res = {ior, endereço & 0x0003FF};
        break;
    case 0x05:
        regiao = "Palette RAM";
        res = {palleteRam, endereço & 0x0003FF};
        break;
    case 0x06:
    {
        regiao = "VRAM";
        uint32_t off = endereço & 0x01FFFF;
        if (off >= 0x18000)
        {
            off -= 0x8000;
            logFileMem << get_indent_mem() << "Ajuste de Mirroring VRAM (off >= 0x18000 -> offset -0x8000)\n";
        }
        res = {vram, off};
        break;
    }
    case 0x07:
        regiao = "OAM";
        res = {oam, endereço & 0x0003FF};
        break;
    case 0x08:
    case 0x09:
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0D:
        regiao = "Game Pak ROM";
        res = {rom, endereço & 0x01FFFFFF};
        break;
    case 0x0E:
    case 0x0F:
        regiao = "Game Pak SRAM";
        res = {sram, endereço & 0x00FFFF};
        break;
    default:
        res = {nullptr, 0};
        break;
    }

    logFileMem << get_indent_mem() << "Região Mapeada: " << regiao << "\n";
    logFileMem << get_indent_mem() << "Endereço Mascarado: 0x" << std::hex << res.endereçoMascarado << std::dec << "\n";
    logFileMem << get_indent_mem() << "Ponteiro Válido: " << (res.ponteiro ? "True" : "False") << "\n";

    indent_levelMem--;
    logFileMem << get_indent_mem() << "}\n";

    return res;
}

// Funções de Leitura
OpResult Mem::get8(uint32_t endereço)
{
    logFileMem << get_indent_mem() << "[Mem::get8] {\n";
    indent_levelMem++;

    logFileMem << get_indent_mem() << "Endereço Solicitado: 0x" << std::hex << endereço << std::dec << "\n";

    physicalAddr addr = resolve_addr(endereço);
    if (!addr.ponteiro)
    {
        logFileMem << get_indent_mem() << "Erro: Ponteiro inválido (Falha no mapeamento de memória)\n";
        logFileMem << get_indent_mem() << "Sucesso: False\n";
        indent_levelMem--;
        logFileMem << get_indent_mem() << "}\n";
        return {0, 0, false};
    }

    uint8_t seção = (endereço >> 24) & 0xFF;
    uint8_t ciclos = 1;

    if (seção >= 0x08 && seção <= 0x0E)
        ciclos = 5;
    else if (seção == 0x02)
        ciclos = 3;

    uint8_t valor = addr.ponteiro[addr.endereçoMascarado];

    logFileMem << get_indent_mem() << "Seção: 0x" << std::hex << +seção << std::dec << "\n";
    logFileMem << get_indent_mem() << "Endereço Mascarado: 0x" << std::hex << addr.endereçoMascarado << std::dec << "\n";
    logFileMem << get_indent_mem() << "Ciclos: " << +ciclos << "\n";
    logFileMem << get_indent_mem() << "Valor Lido: 0x" << std::hex << +valor << std::dec << " (" << +valor << ")\n";
    logFileMem << get_indent_mem() << "Sucesso: True\n";

    indent_levelMem--;
    logFileMem << get_indent_mem() << "}\n";

    return {valor, ciclos, true};
}
OpResult Mem::get16(uint32_t endereço)
{
    logFileMem << get_indent_mem() << "[Mem::get16] {\n";
    indent_levelMem++;

    logFileMem << get_indent_mem() << "Endereço Solicitado: 0x" << std::hex << endereço << std::dec << "\n";

    physicalAddr addr = resolve_addr(endereço);
    if (!addr.ponteiro)
    {
        logFileMem << get_indent_mem() << "Erro: Ponteiro inválido (Falha no mapeamento de memória)\n";
        logFileMem << get_indent_mem() << "Sucesso: False\n";
        indent_levelMem--;
        logFileMem << get_indent_mem() << "}\n";
        return {0, 0, false};
    }

    uint8_t seção = (endereço >> 24) & 0xFF;
    uint8_t ciclos = 1;

    if (seção >= 0x08 && seção <= 0x0E)
        ciclos = 5;
    else if (seção == 0x02)
        ciclos = 3;

    logFileMem << get_indent_mem() << "Seção: 0x" << std::hex << +seção << std::dec << "\n";
    logFileMem << get_indent_mem() << "Endereço Mascarado: 0x" << std::hex << addr.endereçoMascarado << std::dec << "\n";
    logFileMem << get_indent_mem() << "Ciclos: " << +ciclos << "\n";

    if (seção >= 0x0E)
    {
        uint16_t b = addr.ponteiro[addr.endereçoMascarado];
        uint16_t valor = (uint16_t)((b << 8) | b);

        logFileMem << get_indent_mem() << "Tratamento Especial (Seção >= 0x0E - Duplicando byte 0x"
                   << std::hex << +b << std::dec << ")\n";
        logFileMem << get_indent_mem() << "Valor Lido: 0x" << std::hex << valor << std::dec << " (" << valor << ")\n";
        logFileMem << get_indent_mem() << "Sucesso: True\n";

        indent_levelMem--;
        logFileMem << get_indent_mem() << "}\n";

        return {valor, ciclos, true};
    }

    uint32_t base = addr.endereçoMascarado & ~1;
    uint16_t valor = addr.ponteiro[base] | (addr.ponteiro[base + 1] << 8);

    logFileMem << get_indent_mem() << "Base Alinhada (16-bit): 0x" << std::hex << base << std::dec << "\n";
    logFileMem << get_indent_mem() << "Valor Lido: 0x" << std::hex << valor << std::dec << " (" << valor << ")\n";
    logFileMem << get_indent_mem() << "Sucesso: True\n";

    indent_levelMem--;
    logFileMem << get_indent_mem() << "}\n";

    return {valor, ciclos, true};
}
OpResult Mem::get32(uint32_t endereço)
{
    logFileMem << get_indent_mem() << "[Mem::get32] {\n";
    indent_levelMem++;

    logFileMem << get_indent_mem() << "Endereço Solicitado: 0x" << std::hex << endereço << std::dec << "\n";

    physicalAddr addr = resolve_addr(endereço);
    if (!addr.ponteiro)
    {
        logFileMem << get_indent_mem() << "Erro: Ponteiro inválido (Falha no mapeamento de memória)\n";
        logFileMem << get_indent_mem() << "Sucesso: False\n";
        indent_levelMem--;
        logFileMem << get_indent_mem() << "}\n";
        return {0, 0, false};
    }

    uint8_t seção = (endereço >> 24) & 0xFF;
    uint8_t ciclos = 1;

    if (seção >= 0x08 && seção <= 0x0E)
        ciclos = 8;
    else if (seção == 0x02)
        ciclos = 6;
    else if (seção == 0x05 || seção == 0x06)
        ciclos = 2;

    logFileMem << get_indent_mem() << "Seção: 0x" << std::hex << +seção << std::dec << "\n";
    logFileMem << get_indent_mem() << "Endereço Mascarado: 0x" << std::hex << addr.endereçoMascarado << std::dec << "\n";
    logFileMem << get_indent_mem() << "Ciclos: " << +ciclos << "\n";

    if (seção >= 0x0E)
    {
        uint32_t b = addr.ponteiro[addr.endereçoMascarado];
        uint32_t valor = (b << 24) | (b << 16) | (b << 8) | b;

        logFileMem << get_indent_mem() << "Tratamento Especial (Seção >= 0x0E - Replicando byte 0x"
                   << std::hex << +b << std::dec << " nos 4 bytes)\n";
        logFileMem << get_indent_mem() << "Valor Lido: 0x" << std::hex << valor << std::dec << " (" << valor << ")\n";
        logFileMem << get_indent_mem() << "Sucesso: True\n";

        indent_levelMem--;
        logFileMem << get_indent_mem() << "}\n";

        return {valor, ciclos, true};
    }

    uint32_t base = addr.endereçoMascarado & ~3;
    uint32_t valor = (uint32_t)addr.ponteiro[base] |
                     ((uint32_t)addr.ponteiro[base + 1] << 8) |
                     ((uint32_t)addr.ponteiro[base + 2] << 16) |
                     ((uint32_t)addr.ponteiro[base + 3] << 24);

    logFileMem << get_indent_mem() << "Base Alinhada (32-bit): 0x" << std::hex << base << std::dec << "\n";
    logFileMem << get_indent_mem() << "Valor Lido: 0x" << std::hex << valor << std::dec << " (" << valor << ")\n";
    logFileMem << get_indent_mem() << "Sucesso: True\n";

    indent_levelMem--;
    logFileMem << get_indent_mem() << "}\n";

    return {valor, ciclos, true};
}

// Funções de Escrita
void Mem::set8(uint32_t endereço, uint8_t valor)
{
    physicalAddr addr = resolve_addr(endereço);
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
void Mem::set16(uint32_t endereço, uint16_t valor)
{
    physicalAddr addr = resolve_addr(endereço);
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
void Mem::set32(uint32_t endereço, uint32_t valor)
{
    logFileMem << get_indent_mem() << "[Mem::set32] {\n";
    indent_levelMem++;

    logFileMem << get_indent_mem() << "Endereço Solicitado: 0x" << std::hex << endereço << std::dec << "\n";
    logFileMem << get_indent_mem() << "Valor para Escrita: 0x" << std::hex << valor << std::dec << " (" << valor << ")\n";

    physicalAddr addr = resolve_addr(endereço);
    if (!addr.ponteiro)
    {
        logFileMem << get_indent_mem() << "Erro: Ponteiro inválido (Falha no mapeamento de memória)\n";
        indent_levelMem--;
        logFileMem << get_indent_mem() << "}\n";
        return;
    }

    uint8_t seção = (endereço >> 24) & 0xFF;
    logFileMem << get_indent_mem() << "Seção: 0x" << std::hex << +seção << std::dec << "\n";
    logFileMem << get_indent_mem() << "Endereço Mascarado: 0x" << std::hex << addr.endereçoMascarado << std::dec << "\n";

    if (seção == 0x00 || (seção >= 0x08 && seção <= 0x0D))
    {
        logFileMem << get_indent_mem() << "Aviso: Escrita ignorada (Região protegida/somente leitura)\n";
        indent_levelMem--;
        logFileMem << get_indent_mem() << "}\n";
        return;
    }

    if (seção >= 0x0E)
    {
        addr.ponteiro[addr.endereçoMascarado] = valor & 0xFF;
        logFileMem << get_indent_mem() << "Escrita Especial SRAM (Apenas byte inferior gravado: 0x"
                   << std::hex << (valor & 0xFF) << std::dec << ")\n";

        indent_levelMem--;
        logFileMem << get_indent_mem() << "}\n";
        return;
    }

    uint32_t base = addr.endereçoMascarado & ~3;
    addr.ponteiro[base] = valor & 0xFF;
    addr.ponteiro[base + 1] = (valor >> 8) & 0xFF;
    addr.ponteiro[base + 2] = (valor >> 16) & 0xFF;
    addr.ponteiro[base + 3] = (valor >> 24) & 0xFF;

    logFileMem << get_indent_mem() << "Base Alinhada (32-bit): 0x" << std::hex << base << std::dec << "\n";
    logFileMem << get_indent_mem() << "Escrita Realizada com Sucesso\n";

    indent_levelMem--;
    logFileMem << get_indent_mem() << "}\n";
}

physicalAddr Mem::silent_resolve_addr(uint32_t endereço)
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

void Mem::poke32(uint32_t endereço, uint32_t valor)
{
    physicalAddr addr = silent_resolve_addr(endereço);
    if (!addr.ponteiro)
        return;

    uint8_t seção = (endereço >> 24) & 0xFF;

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