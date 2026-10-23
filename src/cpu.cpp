#include "cpu.hpp"
#include "memoria.hpp"
#include <bit>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>

inline void set_bit(uint32_t &reg, uint8_t bit, bool value)
{
    if (value)
    {
        reg |= (1U << bit);
    }
    else
    {
        reg &= ~(1U << bit);
    }
}
inline bool get_bit(uint32_t reg, uint8_t bit)
{
    return (reg >> bit) & 0x1;
}

Cpu::Cpu(Mem *mem) : memoria(mem) {}

// 1. Garante que a pasta "logs" existe na raiz do projeto
// Função auxiliar privada de inicialização
static bool inicializar_sistema_de_logs()
{
    std::filesystem::create_directories("logs");
    return true;
}

// A variável global força a chamada da função antes da main()
static const bool log_iniciado = inicializar_sistema_de_logs();

std::ofstream logFile("logs/cpu.log");
uint indent_level = 0;

std::string get_indent()
{
    return std::string(indent_level, '\t');
}

void Cpu::update_reg_map()
{
    CpuMode mode = static_cast<CpuMode>(CPSR & 0x1F);

    for (uint8_t i = 0; i < 16; i++)
    {
        regMap[i] = &R[i];
    }

    switch (mode)
    {
    case CpuMode::IRQ:
        regMap[13] = &R13_irq[0];
        regMap[14] = &R13_irq[1];
        SPSRatual = &SPSR_irq;
        break;
    case CpuMode::FIQ:
        for (uint8_t i = 0; i < 7; i++)
        {
            regMap[i + 7] = &R8_fiq[i];
        }
        SPSRatual = &SPSR_fiq;
        break;
    case CpuMode::Abort:
        regMap[13] = &R13_abt[0];
        regMap[14] = &R13_abt[1];
        SPSRatual = &SPSR_abt;
        break;
    case CpuMode::Supervisor:
        regMap[13] = &R13_svc[0];
        regMap[14] = &R13_svc[1];
        SPSRatual = &SPSR_svc;
        break;
    case CpuMode::Undefined:
        regMap[13] = &R13_und[0];
        regMap[14] = &R13_und[1];
        SPSRatual = &SPSR_und;
        break;

    case CpuMode::System:
    case CpuMode::User:
        SPSRatual = nullptr;
        break;

    default:
        break;
    }
};

void Cpu::set_reg(uint8_t index, uint32_t valor)
{
    // pra tratamento com registradores de interrupções
    if (index < 16)
    {
        R[index] = valor;
    }
    if (index == 15)
    {
        pc_alterado = true;
    }
};

uint32_t Cpu::get_reg(uint8_t index)
{
    // pra tratamento com registradores de interrupções
    if (index < 15)
    {
        return R[index];
    }
    if (index == 15)
    {
        return (get_bit(CPSR, 5)) ? R[15] + 4 : R[15] + 8;
    }
    return 0;
};

OpResult Cpu::shift_barrier(uint32_t shiftType, uint32_t valor, uint8_t count, bool origemImediato, bool flagC)
{
    // pre-setando o resultado
    uint32_t resultado = 0;
    shiftType &= 0b11; // mascarando shifttype para apenas 2 bits

    logFile << get_indent() << "[Shift Barrier] {\n";
    indent_level++;

    logFile << get_indent() << "Valor: " << valor << "\n";
    logFile << get_indent() << "Vem de registrador: " << ((origemImediato) ? "False" : "True") << "\n";
    /*
    Todas os shifts tem tratamento
    especial dependendo da quantidade
    no Gba original isso foi replicado
    */
    logFile << get_indent() << "Shift Count: " << +count << "\n";
    logFile << get_indent() << "Shift Type: ";
    switch (shiftType)
    {
    case 0b00:
    {
        logFile << "LSL\n";
        if (count == 0)
        {
            resultado = valor;
        }
        else if (count == 32)
        {
            flagC = get_bit(valor, 0);
        }
        else if (count > 32)
        {
            flagC = 0;
            break;
        }
        else
        {
            resultado = valor << count;
            flagC = get_bit(valor, 32 - count);
        }
        break;
    }
    case 0b01:
    {

        logFile << "LSR\n";
        if (count == 0)
        {
            if (origemImediato)
            {
                flagC = get_bit(valor, 31);
            }
            else
            {
                resultado = valor;
            }
        }
        else if (count == 32)
        {
            flagC = get_bit(valor, 31);
        }
        else if (count > 32)
        {
            flagC = 0;
        }
        else
        {
            resultado = valor >> count;
            flagC = get_bit(valor, count - 1);
        }

        break;
    }
    case 0b10:
    {
        logFile << "ASR\n";
        if (count == 0)
        {
            if (origemImediato)
            {
                if (get_bit(valor, 31))
                {
                    resultado = UINT32_MAX;
                }
                else
                {
                    resultado = 0x00000000;
                }
                flagC = get_bit(valor, 31);
            }
            else
            {
                resultado = valor;
            }
        }
        else if (count >= 32)
        {
            flagC = get_bit(valor, 31);
            if (get_bit(valor, 31))
            {
                resultado = UINT32_MAX;
            }
            else
            {
                resultado = 0x00000000;
            }
        }
        else
        {
            resultado = (int32_t)valor >> count;
            flagC = get_bit(valor, count - 1);
        }

        break;
    }
    case 0b11:
    {
        if (count == 0)
        {
            logFile << "RRX\n";
            if (origemImediato)
            {
                flagC = get_bit(valor, 0);

                resultado = valor >> 1;
                set_bit(resultado, 31, flagC);
            }
            else
            {
                resultado = valor;
            }
        }
        else
        {
            logFile << "ROR\n";
            resultado = std::rotr(valor, count);
            flagC = get_bit(resultado, 31);
        }

        break;
    }
    default:
        break;
    }
    logFile << get_indent() << "Resultado: " << std::hex << resultado << std::dec << "\n";
    indent_level--;
    logFile << get_indent() << "}\n";
    OpResult returning(resultado, flagC, true);
    return returning;
}
bool Cpu::checar_condicao(uint32_t opcode)
{
    uint8_t cond = opcode >> 28;

    // Se for AL (0xE - Always), nem perde tempo lendo as flags
    if (cond == 0xE)
        return true;

    bool N = (CPSR >> 31) & 1;
    bool Z = (CPSR >> 30) & 1;
    bool C = (CPSR >> 29) & 1;
    bool V = (CPSR >> 28) & 1;

    switch (cond)
    {
    case 0x0:
        return Z; // EQ
    case 0x1:
        return !Z; // NE
    case 0x2:
        return C; // CS / HS
    case 0x3:
        return !C; // CC / LO
    case 0x4:
        return N; // MI
    case 0x5:
        return !N; // PL
    case 0x6:
        return V; // VS
    case 0x7:
        return !V; // VC
    case 0x8:
        return C && !Z; // HI
    case 0x9:
        return !C || Z; // LS
    case 0xA:
        return N == V; // GE
    case 0xB:
        return N != V; // LT
    case 0xC:
        return !Z && (N == V); // GT
    case 0xD:
        return Z || (N != V); // LE
    case 0xF:
        return false; // NV (Never)
    default:
        return false;
    }
}

uint32_t Cpu::cpu_read32(uint32_t addr)
{
    // Tentativa de ler a BIOS estando fora dela?
    if (addr < 0x00004000 && R[15] >= 0x00004000)
    {
        logFile << get_indent() << "[PROTEÇÃO BIOS] Leitura 32-bit bloqueada em 0x"
                << std::hex << addr << " -> Retornando Latch: 0x" << bios_latch << std::dec << std::endl;
        return bios_latch;
    }

    // Leitura normal da memória
    return memoria->get32(addr).result;
}
uint16_t Cpu::cpu_read16(uint32_t addr)
{
    if (addr < 0x00004000 && R[15] >= 0x00004000)
    {
        // Retorna a halfword correspondente ao alinhamento do endereço no Latch
        uint32_t shift = (addr & 2) * 8;
        return static_cast<uint16_t>((bios_latch >> shift) & 0xFFFF);
    }

    return memoria->get16(addr).result;
}
uint8_t Cpu::cpu_read8(uint32_t addr)
{
    if (addr < 0x00004000 && R[15] >= 0x00004000)
    {
        // Retorna o byte correspondente ao alinhamento do endereço no Latch
        uint32_t shift = (addr & 3) * 8;
        return static_cast<uint8_t>((bios_latch >> shift) & 0xFF);
    }

    return memoria->get8(addr).result;
}

void Cpu::arm_alu(uint32_t opcode)
{
    logFile << get_indent() << "[ALU] {" << std::endl;
    indent_level++;
    // Registradores
    uint8_t Rn = (opcode >> 16) & 0xF;
    uint8_t Rd = (opcode >> 12) & 0xF;
    logFile << get_indent() << "Rn: 0x" << std::hex << get_reg(Rn) << " Rd: 0x" << get_reg(Rd) << std::dec << std::endl;

    // flags da alu
    bool updateFlag = (opcode >> 20) & 0x1;
    bool imediatoFlag = (opcode >> 25) & 0x1;
    logFile << get_indent() << "Atualizar flags: " << ((updateFlag) ? "True" : "False") << std::endl;
    logFile << get_indent() << "Operação imediata: " << ((imediatoFlag) ? "True" : "False") << std::endl;

    // flags para o CPSR
    bool flagN;
    bool flagZ;
    bool flagC = get_bit(CPSR, 29);
    bool flagV = get_bit(CPSR, 28);
    bool Carry = flagC;

    // valor para ser o segundo operando
    uint32_t val;

    if (imediatoFlag)
    {
        uint8_t rorCount = ((opcode >> 8) & 0xF) * 2;
        uint32_t nn = opcode & 0xFF;
        logFile << get_indent() << "Valor imediato: " << std::hex << nn << std::dec << std::endl;
        logFile << get_indent() << "Contagem de ROR: " << +rorCount << std::endl;

        val = std::rotr(nn, rorCount);
        if (rorCount != 0)
        {
            flagC = get_bit(val, 31);
        }
    }
    else
    {
        uint8_t Rm = opcode & 0xF;

        uint8_t count = 0;
        bool origemImediato = !((opcode >> 4) & 0x1);

        if (origemImediato)
        {
            count = (opcode >> 7) & 0x1F;
        }
        else
        {
            uint8_t Rs = (opcode >> 8) & 0xF;
            count = get_reg(Rs) & 0xFF;
        }

        uint8_t shiftType = (opcode >> 5) & 0b11;

        OpResult op2 = shift_barrier(shiftType, get_reg(Rm), count, origemImediato, flagC);

        val = op2.result;
        flagC = op2.extra;
    }

    // resultado e se pode escrever o resultado em rd

    uint32_t resultado;
    bool escrever_rd = true;

    // identificador de tipo de operação
    // atualiza flags se necessario
    // pode setar a escrita de rd como falso
    // atualiza o resultado
    logFile << get_indent() << "Segundo Operando: " << std::hex << val << std::dec << std::endl;
    logFile << get_indent() << "Tipo de operação: ";
    uint8_t alu_op = (opcode >> 21) & 0xF;
    switch (alu_op)
    {
    case 0x0:
        logFile << "AND" << std::endl;
        resultado = get_reg(Rn) & val;
        break;

    case 0x1:
        logFile << "EOR" << std::endl;
        resultado = get_reg(Rn) ^ val;
        break;

    case 0x2:
        logFile << "SUB" << std::endl;
        resultado = get_reg(Rn) - val;
        flagC = (get_reg(Rn) >= val);
        flagV = ((get_reg(Rn) ^ val) & (get_reg(Rn) ^ resultado)) >> 31;
        break;

    case 0x3:
        logFile << "RSB" << std::endl;
        resultado = val - get_reg(Rn);
        flagC = (val >= get_reg(Rn));
        flagV = ((get_reg(Rn) ^ val) & (val ^ resultado)) >> 31;
        break;

    case 0x4:
        logFile << "ADD" << std::endl;
        resultado = get_reg(Rn) + val;
        flagC = ((uint64_t)get_reg(Rn) + (uint64_t)val) > UINT32_MAX;
        flagV = ((get_reg(Rn) ^ resultado) & (val ^ resultado)) >> 31;
        break;

    case 0x5:
        logFile << "ADC" << std::endl;
        resultado = get_reg(Rn) + val + Carry;
        flagC = ((uint64_t)get_reg(Rn) + (uint64_t)val + Carry) > UINT32_MAX;
        flagV = ((get_reg(Rn) ^ resultado) & (val ^ resultado)) >> 31;
        break;

    case 0x6:
        logFile << "SBC" << std::endl;
        resultado = get_reg(Rn) - val - !Carry;
        flagC = ((uint64_t)get_reg(Rn) - (uint64_t)val - !Carry) <= UINT32_MAX;
        flagV = ((get_reg(Rn) ^ val) & (get_reg(Rn) ^ resultado)) >> 31;
        break;

    case 0x7:
        logFile << "RSC" << std::endl;
        resultado = val - get_reg(Rn) - !Carry;
        flagC = ((uint64_t)val - (uint64_t)get_reg(Rn) - !Carry) <= UINT32_MAX;
        flagV = ((get_reg(Rn) ^ val) & (val ^ resultado)) >> 31;
        break;

    case 0x8:
        logFile << "TST" << std::endl;
        resultado = get_reg(Rn) & val;
        escrever_rd = false;
        break;

    case 0x9:
        logFile << "TEQ" << std::endl;
        resultado = get_reg(Rn) ^ val;
        escrever_rd = false;
        break;

    case 0xA:
        logFile << "CMP" << std::endl;
        resultado = get_reg(Rn) - val;
        escrever_rd = false;
        flagC = (get_reg(Rn) >= val);
        flagV = ((get_reg(Rn) ^ val) & (get_reg(Rn) ^ resultado)) >> 31;
        break;

    case 0xB:
        logFile << "CMN" << std::endl;
        resultado = get_reg(Rn) + val;
        flagC = ((uint64_t)get_reg(Rn) + (uint64_t)val) > UINT32_MAX;
        flagV = ((get_reg(Rn) ^ resultado) & (val ^ resultado)) >> 31;
        escrever_rd = false;
        break;

    case 0xC:
        logFile << "ORR" << std::endl;
        resultado = get_reg(Rn) | val;
        break;

    case 0xD:
        logFile << "MOV" << std::endl;
        resultado = val;
        break;

    case 0xE:
        logFile << "BIC" << std::endl;
        resultado = get_reg(Rn) & (~val);
        break;

    case 0xF:
        logFile << "MVN" << std::endl;
        resultado = ~val;
        break;

    default:
        break;
    }

    bool bitS = (opcode >> 20) & 0x1;

    if (Rd == 15 && bitS)
    {
        logFile << get_indent() << "[EXCEÇÃO] Retornando via SPSR para CPSR" << std::endl;
        CPSR = *SPSRatual;
        update_reg_map();
        set_reg(15, resultado);

        indent_level--;
        logFile << get_indent() << "}" << std::endl;
        return; // Sai sem alterar as flags do novo CPSR
    }

    // é pra atualizar as flags?
    if (updateFlag)
    {
        flagN = resultado >> 31;
        flagZ = resultado == 0;
        set_bit(CPSR, 31, flagN);
        set_bit(CPSR, 30, flagZ);
        set_bit(CPSR, 29, flagC);
        set_bit(CPSR, 28, flagV);
        logFile << get_indent() << "Flags atualizadas: N=" << flagN << " Z=" << flagZ << " C=" << flagC << " V=" << flagV << std::endl;
    }

    // pode escrever o resultador em Rd ?
    if (escrever_rd)
    {
        logFile << get_indent() << "Resultado escrito em get_reg(" << +Rd << "): 0x" << std::hex << resultado << std::dec << std::endl;
        set_reg(Rd, resultado);
    }
    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::arm_mul(uint32_t opcode)
{
    logFile << get_indent() << "[MUL] {" << std::endl;
    indent_level++;
    // é pra atualizar flags?

    bool updateFlag = (opcode >> 20) & 0x1;
    logFile << get_indent() << "Atualizar flags: " << ((updateFlag) ? "True" : "False") << std::endl;

    // registradores
    uint8_t Rd = (opcode >> 16) & 0xF;
    uint8_t Rn = (opcode >> 12) & 0xF;
    uint8_t Rs = (opcode >> 8) & 0xF;
    uint8_t Rm = opcode & 0xF;
    uint8_t RdHigh = Rd;
    uint8_t RdLow = Rn;
    logFile << get_indent() << "Rd: " << std::hex << get_reg(Rd) << " Rn: " << get_reg(Rn) << " Rs: " << get_reg(Rs) << " Rm: " << get_reg(Rm) << std::dec << std::endl;

    // identificador e modificadores da multiplicação
    uint8_t mulType = (opcode >> 4) & 0xF;
    bool longFlag = (opcode >> 23) & 0x1;
    bool signedFlag = ((opcode >> 22) & 0x1);
    bool aculmulativo = (opcode >> 21) & 0x1;
    logFile << get_indent() << "Tipo de multiplicação: " << std::hex << (int)mulType << std::dec << std::endl;
    logFile << get_indent() << "Multiplicação longa: " << ((longFlag) ? "True" : "False") << std::endl;
    logFile << get_indent() << "Multiplicação com sinal: " << ((signedFlag) ? "True" : "False") << std::endl;
    logFile << get_indent() << "Multiplicação acumulativa: " << ((aculmulativo) ? "True" : "False") << std::endl;

    uint32_t resultado = 0;
    uint32_t resultadoLow = 0;
    if (longFlag)
    {
        // long flag significa que o resultado de 64 bits vai ser divido em 2 registradores
        if (RdHigh == RdLow || RdHigh == Rm || RdLow == Rm || RdHigh == 15 || RdLow == 15 || Rm == 15 || Rs == 15)
        {
            // Garante que todas os registradores sejam validos
            return;
        }

        // buffer temporario para os 64 bits
        uint64_t buffer_result = 0;
        if (aculmulativo)
        {
            // no acumulativo o resultado é somado no final por outro valor no caso aqui o proprio bit 64 do Rd baixo com Rd alto
            uint64_t Rd_64 = ((uint64_t)get_reg(RdHigh) << 32) | get_reg(RdLow);

            if (signedFlag)
            {
                /*
                com o signed é pra a mutiplicação respeitar o sinal usei o cast do proprio c++
                para passar pra int32 preservando o sinal e dpeois pra int64 se eu passasse
                direto pra int64 ele n traduziria o sinal e daria um positivo
                */
                buffer_result = ((int64_t)(int32_t)get_reg(Rm) * (int64_t)(int32_t)get_reg(Rs)) + (int64_t)Rd_64;
            }
            else
            {
                buffer_result = ((uint64_t)get_reg(Rm) * (uint64_t)get_reg(Rs)) + Rd_64;
            }
        }
        else
        {
            // mesma logica mas sem somar com Rd_64
            if (signedFlag)
            {
                buffer_result = (int64_t)((int32_t)get_reg(Rm) * (int64_t)(int32_t)get_reg(Rs));
            }
            else
            {
                buffer_result = ((uint64_t)get_reg(Rm) * (uint64_t)get_reg(Rs));
            }
        }
        // pega o resultado normal e deixa com os bits altos e já seta os Rd baixo com o resultado baixo
        resultado = (buffer_result >> 32) & UINT32_MAX;
        resultadoLow = buffer_result & UINT32_MAX;
        set_reg(RdLow, resultadoLow);
        logFile << get_indent() << "Resultado baixo escrito em get_reg(" << +RdLow << "): 0x" << std::hex << resultadoLow << std::dec << std::endl;
    }
    else
    {
        // garante que n há forma invalida de reigstradores
        if (Rd == Rm || Rd == 15 || Rm == 15 || Rs == 15 || Rn == 15)
        {
            return;
        }
        if (aculmulativo)
        {
            // quando acumu8lativo vc mutiplica e soma
            resultado = get_reg(Rm) * get_reg(Rs) + get_reg(Rn);
        }
        else
        {
            resultado = get_reg(Rm) * get_reg(Rs);
        }
    }

    // é pra atualizar as flags?
    if (updateFlag)
    {
        set_bit(CPSR, 31, get_bit(resultado, 31));
        set_bit(CPSR, 30, (resultado == 0 && resultadoLow == 0));
    }

    // guarda o resultado em Rd se longFlag o resultado vai ser a parte alta e Rd tem o mesmo valor que RdHigh o RdLow já foi setado corretamente
    set_reg(Rd, resultado);
    logFile << get_indent() << "Resultado escrito em get_reg(" << +Rd << "): 0x" << std::hex << resultado << std::dec << std::endl;

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::arm_branch(uint32_t opcode)
{
    logFile << get_indent() << "[BRANCH] {" << std::endl;
    indent_level++;

    bool linkBranch = (opcode >> 24) & 0x1;
    logFile << get_indent() << "Tipo: " << (linkBranch ? "BL (Branch with Link)" : "B (Branch)") << std::endl;

    // Extrai 24 bits, estende o sinal para 32 bits e multiplica por 4 (Gambiarra)
    int32_t offset = (static_cast<int32_t>(opcode & 0x00FFFFFF) << 8) >> 6;
    logFile << get_indent() << "Offset de salto: " << offset << " bytes" << std::endl;
    logFile << get_indent() << "PC atual (R15): 0x" << std::hex << get_reg(15) << std::dec << std::endl;

    // Se for BL (Branch with Link), salva o endereço de retorno (próxima instrução)
    if (linkBranch)
    {
        set_reg(14, get_reg(15) - 4); // get_reg(15) já é pc_atual + 4
        logFile << get_indent() << "LR (R14) salvo: 0x" << std::hex << get_reg(14) << std::dec << std::endl;
    }

    // O hardware ARM calcula o salto em relação a (PC_atual + 8)
    // Como get_reg(15) vale (PC_atual + 4), somamos mais 4 para alinhar com o pipeline
    set_reg(15, get_reg(15) + (offset));
    logFile << get_indent() << "Novo PC (R15): 0x" << std::hex << get_reg(15) << std::dec << std::endl;

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::arm_bx(uint32_t opcode)
{
    logFile << get_indent() << "[BLX] {" << std::endl;
    indent_level++;

    // pega o endereço do pulo em Rm
    uint8_t Rm = opcode & 0xF;
    uint32_t address = get_reg(Rm);

    logFile << get_indent() << "Registrador Rm: R" << (int)Rm << " = 0x" << std::hex << address << std::dec << std::endl;

    // flag pra se é pra trocar pro modo Thumb
    bool modoThumb = address & 0x1;
    logFile << get_indent() << "Trocar para modo Thumb: " << (modoThumb ? "True" : "False") << std::endl;

    set_bit(CPSR, 5, modoThumb);

    if (modoThumb)
    {
        // alinha pra só mutiplos de 2
        set_reg(15, address & (~(uint32_t)1));
    }
    else
    {
        // alinha pra só mutiplos de 4
        set_reg(15, address & (~(uint32_t)3));
    }

    logFile << get_indent() << "Novo PC (R15) alinhado: 0x" << std::hex << get_reg(15) << std::dec << std::endl;

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::arm_msr(uint32_t opcode)
{
    logFile << get_indent() << "[MSR] {" << std::endl;
    indent_level++;

    bool imediato = (opcode >> 25) & 0x1; // imediato
    bool spsrFlag = (opcode >> 22) & 0x1; // olha se é pra pegar o spsr e colocar no cpsr ou o contrario

    logFile << get_indent() << "Destino: " << (spsrFlag ? "SPSR" : "CPSR") << std::endl;
    logFile << get_indent() << "Tipo de valor: " << (imediato ? "Imediato" : "Registrador") << std::endl;

    uint32_t *endereçoPSR = &CPSR; // o destino do valor
    uint32_t programState = 0;     // o valor a ser salvo

    // define quais campos que pode sobreescrever usando essa mascara
    uint32_t fields = (opcode >> 16) & 0xF;
    uint32_t mask = 0;
    if (get_bit(fields, 0) & 0x1)
    {
        mask |= 0x000000FF;
    }
    if (get_bit(fields, 1) & 0x1)
    {
        mask |= 0x0000FF00;
    }
    if (get_bit(fields, 2) & 0x1)
    {
        mask |= 0x00FF0000;
    }
    if (get_bit(fields, 3) & 0x1)
    {
        mask |= 0xFF000000;
    }

    // separa o modo atual da cpu
    uint8_t modeBits = CPSR & 0x1F;

    // se for usuario ele só pode alterar os bits das flags
    if (modeBits == 0b10000)
    {
        mask &= 0xFF000000;
    }

    logFile << get_indent() << "Máscara de escrita: 0x" << std::hex << mask << std::dec << std::endl;

    // se for pra colocar em sprs ele ajusta qual spsr dependendo do modo atual
    if (spsrFlag)
    {
        switch (modeBits)
        {
        case 0b10000:
        case 0b11111:
        case 0b10001:
        default:
            logFile << get_indent() << "Erro: Acesso inválido ao SPSR no modo 0x" << std::hex << (int)modeBits << std::dec << std::endl;
            indent_level--;
            logFile << get_indent() << "}" << std::endl;
            return;

        case 0b10010:
            endereçoPSR = &SPSR_irq;
            break;

        case 0b10011:
            endereçoPSR = &SPSR_svc;
            break;

        case 0b10111:
            endereçoPSR = &SPSR_abt;
            break;

        case 0b11011:
            endereçoPSR = &SPSR_und;
            break;
        }
    }

    // imediato ele pega o valor do opcode e rotacionar
    // não imediato ele só pega o valor puro do registrador Rm
    if (imediato)
    {
        uint32_t nn = opcode & 0xFF;
        uint8_t rorCount = ((opcode >> 8) & 0xF) * 2;

        programState = std::rotr(nn, rorCount);
        logFile << get_indent() << "Valor imediato obtido: 0x" << std::hex << programState << std::dec << std::endl;
    }
    else
    {
        uint8_t Rm = opcode & 0xF;
        programState = get_reg(Rm);
        logFile << get_indent() << "Valor lido de get_reg(" << (int)Rm << "): 0x" << std::hex << programState << std::dec << std::endl;
    }

    // aplica a mascara
    *endereçoPSR = (*endereçoPSR & ~mask) | (programState & mask);
    logFile << get_indent() << "Novo valor do registrador de status: 0x" << std::hex << *endereçoPSR << std::dec << std::endl;

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::arm_mrs(uint32_t opcode)
{
    logFile << get_indent() << "[MRS] {" << std::endl;
    indent_level++;

    uint8_t Rd = (opcode >> 12) & 0xF;
    bool spsrFlag = (opcode >> 22) & 0x1;

    logFile << get_indent() << "Registrador destino: R" << (int)Rd << std::endl;
    logFile << get_indent() << "Origem: " << (spsrFlag ? "SPSR" : "CPSR") << std::endl;

    uint32_t *endereçoPSR = &CPSR;

    if (spsrFlag)
    {
        if (SPSRatual == nullptr)
        {
            logFile << get_indent() << "Erro: Acesso inválido ao SPSR no modo 0x" << std::hex << (CPSR & 0x1F) << std::dec << std::endl;
            indent_level--;
            logFile << get_indent() << "}" << std::endl;
            return;
        }
        endereçoPSR = SPSRatual;
    }

    set_reg(Rd, *endereçoPSR);
    logFile << get_indent() << "Valor lido (0x" << std::hex << *endereçoPSR << std::dec << ") escrito em get_reg(" << (int)Rd << ")" << std::endl;

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::arm_sdt(uint32_t opcode)
{
    logFile << get_indent() << "[SDT] {" << std::endl;
    indent_level++;

    bool imediato = (opcode >> 25) & 0x1;
    bool preIndex = (opcode >> 24) & 0x1;
    bool upDown = (opcode >> 23) & 0x1;
    bool wordByte = (opcode >> 22) & 0x1;
    bool writeBack = (opcode >> 21) & 0x1;
    bool loadStore = (opcode >> 20) & 0x1;

    uint8_t Rd = (opcode >> 12) & 0xF;
    uint8_t Rn = (opcode >> 16) & 0xF;

    logFile << get_indent() << "Operação: " << (loadStore ? "LDR" : "STR") << (wordByte ? "B" : "") << std::endl;
    logFile << get_indent() << "Indexação: " << (preIndex ? "Pré-indexado" : "Pós-indexado") << std::endl;
    logFile << get_indent() << "Direção offset: " << (upDown ? "+" : "-") << std::endl;
    logFile << get_indent() << "Writeback: " << (writeBack ? "True" : "False") << std::endl;
    logFile << get_indent() << "get_reg(" << (int)Rn << ") base: 0x" << std::hex << get_reg(Rn)
            << " get_reg(" << (int)Rd << ") valor: 0x" << get_reg(Rd) << std::dec << std::endl;

    uint32_t offset = 0;
    if (imediato)
    {
        uint8_t Rm = opcode & 0xF;
        uint8_t shiftType = (opcode >> 5) & 0b11;
        uint8_t shiftCount = (opcode >> 7) & 0x1F;

        OpResult op2 = shift_barrier(shiftType, get_reg(Rm), shiftCount, true, get_bit(CPSR, 29));

        offset = op2.result;
        logFile << get_indent() << "Offset (R" << (int)Rm << " deslocado): 0x" << std::hex << offset << std::dec << std::endl;
    }
    else
    {
        offset = opcode & 0xFFF;
        logFile << get_indent() << "Offset (Imediato): 0x" << std::hex << offset << std::dec << std::endl;
    }

    uint32_t endereço = get_reg(Rn);
    if (preIndex)
    {
        endereço = (upDown) ? get_reg(Rn) + offset : get_reg(Rn) - offset;
    }

    logFile << get_indent() << "Endereço final: 0x" << std::hex << endereço << std::dec << std::endl;

    if (loadStore)
    {
        uint32_t resultado = 0;
        if (wordByte)
        {
            resultado = cpu_read8(endereço);
        }
        else
        {
            resultado = cpu_read32(endereço);
            uint8_t desalinhamentoEmBits = (endereço & 0b11) * 8;
            resultado = std::rotr(resultado, desalinhamentoEmBits);
        }

        set_reg(Rd, resultado);
        logFile << get_indent() << "Lido da memória: 0x" << std::hex << resultado << std::dec << " -> escrito em get_reg(" << (int)Rd << ")" << std::endl;
    }
    else
    {
        if (wordByte)
        {
            memoria->set8(endereço, get_reg(Rd) & 0xFF);
            logFile << get_indent() << "Escrito na memória (Byte): 0x" << std::hex << (get_reg(Rd) & 0xFF) << std::dec << " no endereço 0x" << std::hex << endereço << std::dec << std::endl;
        }
        else
        {
            memoria->set32(endereço, get_reg(Rd));
            logFile << get_indent() << "Escrito na memória (Word): 0x" << std::hex << get_reg(Rd) << std::dec << " no endereço 0x" << std::hex << endereço << std::dec << std::endl;
        }
    }

    if (writeBack || !preIndex)
    {
        if (!loadStore || (Rd != Rn))
        {
            set_reg(Rn, (upDown) ? get_reg(Rn) + offset : get_reg(Rn) - offset);
            logFile << get_indent() << "Writeback atualizou get_reg(" << (int)Rn << ") para 0x" << std::hex << get_reg(Rn) << std::dec << std::endl;
        }
    }

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::arm_hw_s(uint32_t opcode)
{
    logFile << get_indent() << "[HW_S] {" << std::endl;
    indent_level++;

    bool preIndex = (opcode >> 24) & 0x1;
    bool upDown = (opcode >> 23) & 0x1;
    bool imediato = !((opcode >> 22) & 0x1);
    bool writeBack = (opcode >> 21) & 0x1;
    bool loadStore = (opcode >> 20) & 0x1;
    bool signed_ = (opcode >> 6) & 0x1;
    bool halfword = (opcode >> 5) & 0x1;

    uint8_t offsetHigh = (opcode >> 8) & 0xF;
    uint8_t offsetLow = opcode & 0xF;

    uint8_t Rn = (opcode >> 16) & 0xF;
    uint8_t Rm = offsetLow;
    uint8_t Rd = (opcode >> 12) & 0xF;

    logFile << get_indent() << "Operação: " << (loadStore ? "LDR" : "STR")
            << (signed_ ? "S" : "") << (halfword ? "H" : "B") << std::endl;
    logFile << get_indent() << "Indexação: " << (preIndex ? "Pré-indexado" : "Pós-indexado") << std::endl;
    logFile << get_indent() << "Direção offset: " << (upDown ? "+" : "-") << std::endl;
    logFile << get_indent() << "Writeback: " << (writeBack ? "True" : "False") << std::endl;
    logFile << get_indent() << "get_reg(" << (int)Rn << ") base: 0x" << std::hex << get_reg(Rn)
            << " get_reg(" << (int)Rd << ") valor: 0x" << get_reg(Rd) << std::dec << std::endl;

    uint32_t offset = (offsetHigh << 4) | offsetLow;

    if (imediato)
    {
        offset = get_reg(Rm);
        logFile << get_indent() << "Offset (get_reg(" << (int)Rm << ")): 0x" << std::hex << offset << std::dec << std::endl;
    }
    else
    {
        logFile << get_indent() << "Offset (Imediato): 0x" << std::hex << offset << std::dec << std::endl;
    }

    uint32_t endereço = get_reg(Rn);
    if (preIndex)
    {
        endereço = (upDown) ? get_reg(Rn) + offset : get_reg(Rn) - offset;
    }

    logFile << get_indent() << "Endereço final: 0x" << std::hex << endereço << std::dec << std::endl;

    if (loadStore)
    {
        if (halfword)
        {
            uint32_t resultado = cpu_read16(endereço);
            if (signed_)
            {
                resultado |= (((get_bit(resultado, 15)) ? 0xFFFF : 0x00) << 16);
            }
            set_reg(Rd, resultado);
            logFile << get_indent() << "Lido da memória (Halfword" << (signed_ ? " Signed" : "") << "): 0x"
                    << std::hex << resultado << std::dec << " -> escrito em get_reg(" << (int)Rd << ")" << std::endl;
        }
        else
        {
            uint32_t resultado = cpu_read8(endereço);
            if (signed_)
            {
                resultado |= (((get_bit(resultado, 7)) ? 0xFFFFFF : 0x00) << 8);
            }
            set_reg(Rd, resultado);
            logFile << get_indent() << "Lido da memória (Byte Signed): 0x"
                    << std::hex << resultado << std::dec << " -> escrito em get_reg(" << (int)Rd << ")" << std::endl;
        }
    }
    else
    {
        if (halfword)
        {
            memoria->set16(endereço, get_reg(Rd) & 0xFFFF);
            logFile << get_indent() << "Escrito na memória (Halfword): 0x" << std::hex << (get_reg(Rd) & 0xFFFF)
                    << std::dec << " no endereço 0x" << std::hex << endereço << std::dec << std::endl;
        }
        else
        {
            memoria->set8(endereço, get_reg(Rd) & 0xFF);
            logFile << get_indent() << "Escrito na memória (Byte): 0x" << std::hex << (get_reg(Rd) & 0xFF)
                    << std::dec << " no endereço 0x" << std::hex << endereço << std::dec << std::endl;
        }
    }

    if (writeBack || !preIndex)
    {
        if (!loadStore || (Rd != Rn))
        {
            set_reg(Rn, (upDown) ? get_reg(Rn) + offset : get_reg(Rn) - offset);
            logFile << get_indent() << "Writeback atualizou get_reg(" << (int)Rn << ") para 0x" << std::hex << get_reg(Rn) << std::dec << std::endl;
        }
    }

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::arm_bdt(uint32_t opcode)
{
    logFile << get_indent() << "[BDT] {" << std::endl;
    indent_level++;

    bool preIndex = (opcode >> 24) & 0x1;
    bool upDown = (opcode >> 23) & 0x1;
    bool writeBack = (opcode >> 21) & 0x1;
    bool loadStore = (opcode >> 20) & 0x1;

    uint8_t Rn = (opcode >> 16) & 0xF;
    uint16_t regList = opcode & 0xFFFF;

    logFile << get_indent() << "Operação: " << (loadStore ? "LDM" : "STM") << std::endl;
    logFile << get_indent() << "Indexação: " << (preIndex ? "Pré-indexado" : "Pós-indexado") << std::endl;
    logFile << get_indent() << "Direção offset: " << (upDown ? "+" : "-") << std::endl;
    logFile << get_indent() << "Writeback: " << (writeBack ? "True" : "False") << std::endl;
    logFile << get_indent() << "get_reg(" << (int)Rn << ") base: 0x" << std::hex << get_reg(Rn) << std::dec << std::endl;
    logFile << get_indent() << "Lista de registradores: 0x" << std::hex << regList << std::dec << std::endl;

    uint32_t start = 0;
    if (upDown)
    {
        if (preIndex)
        {
            start = get_reg(Rn) + 4;
        }
        else
        {
            start = get_reg(Rn);
        }
    }
    else
    {
        if (preIndex)
        {
            start = get_reg(Rn) - (std::popcount(regList) * 4);
        }
        else
        {
            start = get_reg(Rn) - (std::popcount(regList) * 4) + 4;
        }
    }

    // 1. Loop de Leitura / Escrita
    uint32_t addr = start;
    for (int i = 0; i < 16; i++)
    {
        if ((regList >> i) & 0x1)
        {
            if (loadStore)
            {
                uint32_t valor = cpu_read32(addr);
                set_reg(i, valor);
                logFile << get_indent() << "Lido da memória: 0x" << std::hex << valor << std::dec << " -> R" << i << std::endl;
            }
            else
            {
                memoria->set32(addr, get_reg(i));
                logFile << get_indent() << "Escrito na memória: R" << i << " (0x" << std::hex << get_reg(i) << ") -> 0x" << addr << std::dec << std::endl;
            }
            addr += 4;
        }
    }

    // 2. Aplica o Writeback DEPOIS do loop
    if (writeBack)
    {
        uint32_t totalBytes = std::popcount(regList) * 4;
        uint32_t novoBase = upDown ? (get_reg(Rn) + totalBytes) : (get_reg(Rn) - totalBytes);

        if (!loadStore || !((regList >> Rn) & 1))
        {
            set_reg(Rn, novoBase);
        }
    }

    // 3. Retorno de Exceção (Bit 22 = S, LDM = 1, PC na lista)
    bool bitS = (opcode >> 22) & 0x1;
    bool pcNaLista = (regList >> 15) & 0x1;

    if (bitS && loadStore && pcNaLista)
    {
        logFile << get_indent() << "[EXCEÇÃO] LDM restaurando CPSR via SPSR" << std::endl;
        CPSR = *SPSRatual;
        update_reg_map();
    }

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::arm_swp(uint32_t opcode)
{
    logFile << get_indent() << "[SWP] {" << std::endl;
    indent_level++;

    bool wordByte = (opcode >> 22) & 0x1;
    uint8_t Rn = (opcode >> 16) & 0xF;
    uint8_t Rd = (opcode >> 12) & 0xF;
    uint8_t Rm = opcode & 0xF;

    logFile << get_indent() << "Tipo de Dado: " << ((wordByte) ? "Byte(8 bits)" : "Word(32 bits)") << std::endl;
    logFile << get_indent() << "Rn: 0x" << std::hex << get_reg(Rn) << std::endl;
    logFile << get_indent() << "Rm: 0x" << get_reg(Rm) << std::dec << std::endl;

    if (wordByte)
    {
        uint32_t val = cpu_read8(get_reg(Rn));
        set_reg(Rd, val);
        logFile << get_indent() << "Valor Guardado em Rd: 0x" << std::hex << get_reg(Rd) << std::dec << std::endl;
        memoria->set8(get_reg(Rn), (get_reg(Rm) & 0xFF));
    }
    else
    {
        uint32_t val = cpu_read32(get_reg(Rn));
        set_reg(Rd, val);
        logFile << get_indent() << "Valor Guardado em Rd: 0x" << std::hex << get_reg(Rd) << std::dec << std::endl;
        memoria->set32(get_reg(Rn), get_reg(Rm));
    }

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::step_arm()
{
    // 1. FETCH & ATUALIZAÇÃO DO BIOS LATCH
    uint32_t pc_execucao = get_reg(15);
    OpResult getOpcode = memoria->get32(pc_execucao);
    uint32_t opcode = getOpcode.result;

    // Se a instrução foi buscada da BIOS, salva o valor no Latch de proteção anti-dump
    if (pc_execucao < 0x00004000)
    {
        bios_latch = opcode;
    }

    // Prepara a flag para detectar se a instrução atual modificou o PC
    pc_alterado = false;

    logFile << get_indent() << "[STEP ARM] {" << std::endl;
    indent_level++;
    logFile << get_indent() << "PC atual: 0x" << std::hex << pc_execucao << std::dec << std::endl;
    logFile << get_indent() << "Opcode lido: 0x" << std::hex << opcode << std::dec << std::endl;

    // 2. CHECK CONDITION (Bits 31..28)
    if (!checar_condicao(opcode))
    {
        logFile << get_indent() << "Condição não satisfeita, instrução ignorada." << std::endl;
        set_reg(15, pc_execucao + 4); // Avança PC mesmo se a instrução for ignorada
        indent_level--;
        logFile << get_indent() << "}" << std::endl;
        return;
    }

    // 3. DECODE & DISPATCH (Bits 27..25)
    uint8_t grupo = (opcode >> 25) & 0x7;

    switch (grupo)
    {
    case 0b000:
    {
        // Se bits 7 e 4 forem 1 (Halfword Data Transfer, Multiplicações estendidas)
        if (((opcode >> 7) & 1) && ((opcode >> 4) & 1))
        {
            uint8_t sh = (opcode >> 5) & 0x3; // Bits 6..5

            if (sh != 0)
            {
                arm_hw_s(opcode); // LDRH, STRH, LDRSB, LDRSH
            }
            else
            {
                uint8_t op_mul = (opcode >> 20) & 0xFF;
                if ((op_mul & 0xFC) == 0x00 || (op_mul & 0xF8) == 0x08)
                {
                    arm_mul(opcode); // MUL, MLA, UMULL, SMULL, etc.
                }
                else
                {
                    interrupt(CpuMode::Undefined, 0x00000004);
                }
            }
        }
        else
        {
            // Checa SWP (Bits 27..23 == 00010, Bits 11..4 == 00001001)
            if (((opcode >> 20) & 0xFB) == 0x10 && ((opcode >> 4) & 0xFF) == 0x09)
            {
                arm_swp(opcode);
            }
            else
            {
                uint8_t op_misc = (opcode >> 20) & 0x1F;

                if ((op_misc & 0x19) == 0x10)
                {
                    uint8_t bits_11_4 = (opcode >> 4) & 0xFF;

                    if ((bits_11_4 & 0x0F) == 0x0)
                    {
                        if ((opcode >> 21) & 1)
                            arm_msr(opcode);
                        else
                            arm_mrs(opcode);
                    }
                    else if ((opcode & 0x0FFFFFF0) == 0x012FFF10) // BX ARMv4T
                    {
                        arm_bx(opcode);
                    }
                    else
                    {
                        arm_alu(opcode);
                    }
                }
                else
                {
                    arm_alu(opcode);
                }
            }
        }
        break;
    }

    case 0b001:
    {
        uint8_t op_misc = (opcode >> 20) & 0x1F;

        if (op_misc == 0x12 || op_misc == 0x16)
        {
            arm_msr(opcode); // MSR com imediato
        }
        else
        {
            arm_alu(opcode); // ALU com imediato
        }
        break;
    }

    case 0b010:
    {
        arm_sdt(opcode); // LDR, STR (Offset imediato)
        break;
    }

    case 0b011:
    {
        bool bit4 = (opcode >> 4) & 1;
        if (!bit4)
        {
            arm_sdt(opcode); // LDR, STR (Offset registrador)
        }
        else
        {
            interrupt(CpuMode::Undefined, 0x00000004);
        }
        break;
    }

    case 0b100:
    {
        arm_bdt(opcode); // LDM, STM
        break;
    }

    case 0b101:
    {
        arm_branch(opcode); // B, BL
        break;
    }

    case 0b110:
    {
        interrupt(CpuMode::Undefined, 0x00000004);
        break;
    }

    case 0b111:
    {
        if ((opcode & 0x0F000000) == 0x0F000000)
        {
            interrupt(CpuMode::Supervisor, 0x00000008); // SWI
        }
        else
        {
            interrupt(CpuMode::Undefined, 0x00000004);
        }
        break;
    }

    default:
    {
        logFile << get_indent() << "[ERRO CPU] Opcode ARM invalido: 0x" << std::hex << opcode
                << " no PC: 0x" << pc_execucao << std::dec << std::endl;
        interrupt(CpuMode::Undefined, 0x00000004);
        break;
    }
    }

    // Se NENHUM salto foi executado pela instrução, avança para a próxima instrução
    if (!pc_alterado)
    {
        set_reg(15, get_reg(15) + 4);
    }

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}

void Cpu::thumb_format_1(uint16_t opcode)
{
    logFile << get_indent() << "[THUMB Format 1 - Move Shifted Register] {" << std::endl;
    indent_level++;

    uint8_t operation = (opcode >> 11) & 0b11;
    uint8_t shiftCount = (opcode >> 6) & 0x1F;
    uint8_t Rs = (opcode >> 3) & 0b111;
    uint8_t Rd = opcode & 0b111;

    const char *opNames[] = {"LSL", "LSR", "ASR", "RESERVED"};
    logFile << get_indent() << "Operação: " << opNames[operation] << std::endl;
    logFile << get_indent() << "get_reg(" << (int)Rs << ") valor: 0x" << std::hex << get_reg(Rs)
            << " | Deslocamento: " << std::dec << (int)shiftCount << std::endl;

    OpResult shiftStatus = shift_barrier(operation, get_reg(Rs), shiftCount, false, get_bit(CPSR, 29));

    uint32_t resultado = shiftStatus.result;

    set_reg(Rd, resultado);
    set_bit(CPSR, 31, get_bit(resultado, 31));
    set_bit(CPSR, 30, resultado == 0);
    set_bit(CPSR, 29, shiftStatus.extra & 0x1);

    logFile << get_indent() << "Resultado: 0x" << std::hex << resultado
            << " -> escrito em get_reg(" << std::dec << (int)Rd << ")" << std::endl;
    logFile << get_indent() << "Flags (NZC): N=" << get_bit(CPSR, 31)
            << " Z=" << get_bit(CPSR, 30)
            << " C=" << get_bit(CPSR, 29) << std::endl;

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::thumb_format_2(uint16_t opcode)
{
    logFile << get_indent() << "[THUMB Format 2 - Add/Subtract] {" << std::endl;
    indent_level++;

    bool imediato = (opcode >> 10) & 0x1;
    bool sub = (opcode >> 9) & 0x1;

    uint8_t Rs = (opcode >> 3) & 0b111;
    uint8_t Rd = opcode & 0b111;
    uint8_t Rn = (opcode >> 6) & 0b111;

    uint32_t op2 = (imediato) ? Rn : get_reg(Rn);

    logFile << get_indent() << "Operação: " << (sub ? "SUB" : "ADD") << std::endl;
    logFile << get_indent() << "get_reg(" << (int)Rs << ") valor: 0x" << std::hex << get_reg(Rs) << std::dec << std::endl;
    if (imediato)
    {
        logFile << get_indent() << "Op2 (Imediato 3-bit): 0x" << std::hex << (int)Rn << std::dec << std::endl;
    }
    else
    {
        logFile << get_indent() << "Op2 (get_reg(" << (int)Rn << ")): 0x" << std::hex << op2 << std::dec << std::endl;
    }

    bool flagC = false;
    bool overflow = false;

    uint32_t resultado;
    if (sub)
    {
        resultado = get_reg(Rs) - op2;
        flagC = get_reg(Rs) >= op2;
        overflow = ((get_reg(Rs) ^ op2) & (get_reg(Rs) ^ resultado)) & 0x80000000;
    }
    else
    {
        resultado = get_reg(Rs) + op2;
        flagC = resultado < get_reg(Rs);
        overflow = (~(get_reg(Rs) ^ op2) & (get_reg(Rs) ^ resultado)) & 0x80000000;
    }

    set_reg(Rd, resultado);

    set_bit(CPSR, 31, get_bit(resultado, 31));
    set_bit(CPSR, 30, resultado == 0);
    set_bit(CPSR, 29, flagC);
    set_bit(CPSR, 28, overflow);

    logFile << get_indent() << "Resultado: 0x" << std::hex << resultado
            << " -> escrito em get_reg(" << std::dec << (int)Rd << ")" << std::endl;
    logFile << get_indent() << "Flags (NZCV): N=" << get_bit(CPSR, 31)
            << " Z=" << get_bit(CPSR, 30)
            << " C=" << get_bit(CPSR, 29)
            << " V=" << get_bit(CPSR, 28) << std::endl;

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::thumb_format_3(uint16_t opcode)
{
    logFile << get_indent() << "[THUMB Format 3 - Move/Compare/Add/Subtract Immediate] {" << std::endl;
    indent_level++;

    uint8_t operation = (opcode >> 11) & 0b11;
    uint8_t Rd = (opcode >> 8) & 0b111;
    uint8_t imm8 = opcode & 0xFF;

    const char *opNames[] = {"MOV", "CMP", "ADD", "SUB"};
    logFile << get_indent() << "Operação: " << opNames[operation] << std::endl;
    logFile << get_indent() << "get_reg(" << (int)Rd << ") base: 0x" << std::hex << get_reg(Rd)
            << " | Imediato (imm8): 0x" << (int)imm8 << std::dec << std::endl;

    bool flagN = get_bit(CPSR, 31);
    bool flagZ = get_bit(CPSR, 30);
    bool flagC = get_bit(CPSR, 29);
    bool flagV = get_bit(CPSR, 28);

    uint32_t resultado;

    switch (operation)
    {
    case 0b00:
        resultado = imm8;
        flagN = false;
        break;

    case 0b01:
    case 0b11:
        resultado = get_reg(Rd) - imm8;

        flagN = get_bit(resultado, 31);
        flagC = get_reg(Rd) >= imm8;
        flagV = ((get_reg(Rd) ^ imm8) & (get_reg(Rd) ^ resultado)) >> 31;
        break;

    case 0b10:
        resultado = get_reg(Rd) + imm8;

        flagN = get_bit(resultado, 31);
        flagC = resultado < get_reg(Rd);
        flagV = (~(get_reg(Rd) ^ imm8) & (get_reg(Rd) ^ resultado)) >> 31;
        break;

    default:
        break;
    }

    flagZ = resultado == 0;

    if (operation != 0b01)
    {
        set_reg(Rd, resultado);
        logFile << get_indent() << "Resultado: 0x" << std::hex << resultado
                << " -> escrito em get_reg(" << std::dec << (int)Rd << ")" << std::endl;
    }
    else
    {
        logFile << get_indent() << "Resultado (CMP - sem escrita em get_reg(" << (int)Rd << ")): 0x"
                << std::hex << resultado << std::dec << std::endl;
    }

    set_bit(CPSR, 31, flagN);
    set_bit(CPSR, 30, flagZ);
    set_bit(CPSR, 29, flagC);
    set_bit(CPSR, 28, flagV);

    logFile << get_indent() << "Flags (NZCV): N=" << get_bit(CPSR, 31)
            << " Z=" << get_bit(CPSR, 30)
            << " C=" << get_bit(CPSR, 29)
            << " V=" << get_bit(CPSR, 28) << std::endl;

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::thumb_format_4(uint16_t opcode)
{
    logFile << get_indent() << "[THUMB Format 4 - ALU Operations] {" << std::endl;
    indent_level++;

    uint8_t op = (opcode >> 6) & 0xF;
    uint8_t Rs = (opcode >> 3) & 0b111;
    uint8_t Rd = opcode & 0b111;

    uint32_t op1 = get_reg(Rd);
    uint32_t op2 = get_reg(Rs);
    uint32_t resultado = 0;

    const char *opNames[] = {
        "AND", "EOR", "LSL", "LSR", "ASR", "ADC", "SBC", "ROR",
        "TST", "NEG", "CMP", "CMN", "ORR", "MUL", "BIC", "MVN"};

    logFile << get_indent() << "Operação: " << opNames[op] << std::endl;
    logFile << get_indent() << "get_reg(" << (int)Rd << ") (op1): 0x" << std::hex << op1
            << " | get_reg(" << std::dec << (int)Rs << ") (op2): 0x" << std::hex << op2 << std::dec << std::endl;

    bool flagN = get_bit(CPSR, 31);
    bool flagZ = get_bit(CPSR, 30);
    bool flagC = get_bit(CPSR, 29);
    bool flagV = get_bit(CPSR, 28);

    bool writeResult = true; // TST, CMP e CMN não escrevem em Rd

    switch (op)
    {
    // --- LÓGICAS (N, Z) ---
    case 0x0: // AND
        resultado = op1 & op2;
        break;

    case 0x1: // EOR
        resultado = op1 ^ op2;
        break;

    case 0x8: // TST
        resultado = op1 & op2;
        writeResult = false;
        break;

    case 0xC: // ORR
        resultado = op1 | op2;
        break;

    case 0xE: // BIC
        resultado = op1 & (~op2);
        break;

    case 0xF: // MVN
        resultado = ~op2;
        break;

    // --- SHIFTS POR REGISTRADOR (N, Z, C) ---
    case 0x2: // LSL
    case 0x3: // LSR
    case 0x4: // ASR
    case 0x7: // ROR
    {
        uint8_t shiftType = (op == 0x7) ? 0b11 : (op - 0x2);
        uint8_t shiftAmount = op2 & 0xFF;

        OpResult shiftRes = shift_barrier(shiftType, op1, shiftAmount, true, flagC);
        resultado = shiftRes.result;
        flagC = shiftRes.extra & 0x1;
        break;
    }

    // --- ARITMÉTICAS (N, Z, C, V) ---
    case 0x5: // ADC
    {
        uint64_t sum = (uint64_t)op1 + op2 + (flagC ? 1 : 0);
        resultado = (uint32_t)sum;
        flagC = sum > 0xFFFFFFFF;
        flagV = (~(op1 ^ op2) & (op1 ^ resultado)) >> 31;
        break;
    }

    case 0x6: // SBC
    {
        uint64_t sub = (uint64_t)op1 + (uint32_t)(~op2) + (flagC ? 1 : 0);
        resultado = (uint32_t)sub;
        flagC = sub >> 32;
        flagV = ((op1 ^ op2) & (op1 ^ resultado)) >> 31;
        break;
    }

    case 0x9: // NEG (0 - op2)
        resultado = 0 - op2;
        flagC = (op2 == 0);
        flagV = (op2 & resultado) >> 31;
        break;

    case 0xA: // CMP
        resultado = op1 - op2;
        flagC = op1 >= op2;
        flagV = ((op1 ^ op2) & (op1 ^ resultado)) >> 31;
        writeResult = false;
        break;

    case 0xB: // CMN
        resultado = op1 + op2;
        flagC = resultado < op1;
        flagV = (~(op1 ^ op2) & (op1 ^ resultado)) >> 31;
        writeResult = false;
        break;

    // --- MULTIPLICAÇÃO (N, Z) ---
    case 0xD: // MUL
        resultado = op1 * op2;
        break;
    }

    flagN = get_bit(resultado, 31);
    flagZ = (resultado == 0);

    if (writeResult)
    {
        set_reg(Rd, resultado);
        logFile << get_indent() << "Resultado: 0x" << std::hex << resultado
                << " -> escrito em get_reg(" << std::dec << (int)Rd << ")" << std::endl;
    }
    else
    {
        logFile << get_indent() << "Resultado (" << opNames[op] << " - sem escrita): 0x"
                << std::hex << resultado << std::dec << std::endl;
    }

    set_bit(CPSR, 31, flagN);
    set_bit(CPSR, 30, flagZ);
    set_bit(CPSR, 29, flagC);
    set_bit(CPSR, 28, flagV);

    logFile << get_indent() << "Flags (NZCV): N=" << get_bit(CPSR, 31)
            << " Z=" << get_bit(CPSR, 30)
            << " C=" << get_bit(CPSR, 29)
            << " V=" << get_bit(CPSR, 28) << std::endl;

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::thumb_format_5(uint16_t opcode)
{
    logFile << get_indent() << "[THUMB Format 5 - Hi Register Ops / Branch Exchange] {" << std::endl;
    indent_level++;

    uint8_t operation = (opcode >> 8) & 0b11;
    uint8_t Rs = (opcode >> 3) & 0xF;
    uint8_t Rd = ((opcode >> 4) & 0x8) | (opcode & 0b111);

    const char *opNames[] = {"ADD", "CMP", "MOV", "BX"};
    logFile << get_indent() << "Operação: " << opNames[operation] << std::endl;
    logFile << get_indent() << "get_reg(" << (int)Rd << ") destino: 0x" << std::hex << get_reg(Rd)
            << " | get_reg(" << (int)Rs << ") origem: 0x" << get_reg(Rs) << std::dec << std::endl;

    switch (operation)
    {
    case 0b00: // ADD
        set_reg(Rd, get_reg(Rd) + get_reg(Rs));
        if (Rd == 15)
        {
            set_reg(15, get_reg(15) & (~1U));
        }
        logFile << get_indent() << "Novo valor de get_reg(" << (int)Rd << "): 0x" << std::hex << get_reg(Rd) << std::dec << std::endl;
        break;

    case 0b01: // CMP
    {
        uint32_t resultado = get_reg(Rd) - get_reg(Rs);

        bool flagN = get_bit(resultado, 31);
        bool flagZ = (resultado == 0);
        bool flagC = (get_reg(Rd) >= get_reg(Rs));
        bool flagV = ((get_reg(Rd) ^ get_reg(Rs)) & (get_reg(Rd) ^ resultado)) >> 31;

        set_bit(CPSR, 31, flagN);
        set_bit(CPSR, 30, flagZ);
        set_bit(CPSR, 29, flagC);
        set_bit(CPSR, 28, flagV);

        logFile << get_indent() << "Resultado CMP: 0x" << std::hex << resultado << std::dec << std::endl;
        logFile << get_indent() << "Flags (NZCV): N=" << flagN << " Z=" << flagZ
                << " C=" << flagC << " V=" << flagV << std::endl;
        break;
    }

    case 0b10: // MOV
        set_reg(Rd, get_reg(Rs));
        if (Rd == 15)
        {
            set_reg(15, get_reg(15) & (~1U));
        }
        logFile << get_indent() << "Novo valor de get_reg(" << (int)Rd << "): 0x" << std::hex << get_reg(Rd) << std::dec << std::endl;
        break;

    case 0b11: // BX
    {
        uint32_t endereço = get_reg(Rs);
        bool armMode = !get_bit(get_reg(Rs), 0);

        if (get_bit(get_reg(Rs), 0))
        {
            set_reg(15, endereço & (~1U));
            set_bit(CPSR, 5, 1);
        }
        else
        {
            set_reg(15, endereço & (~3U));
            set_bit(CPSR, 5, 0);
        }

        logFile << get_indent() << "Desvio BX para 0x" << std::hex << get_reg(15) << std::dec
                << " | Modo: " << (armMode ? "ARM" : "THUMB") << std::endl;
        break;
    }
    default:
        break;
    }

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::thumb_format_6(uint16_t opcode)
{
    logFile << get_indent() << "[THUMB Format 6 - PC-Relative Load] {" << std::endl;
    indent_level++;

    uint8_t Rd = (opcode >> 8) & 0b111;
    uint32_t imm8 = opcode & 0xFF;

    uint32_t pc_alinhado = get_reg(15) & ~2U;
    uint32_t endereco = pc_alinhado + (imm8 << 2);

    logFile << get_indent() << "Operação: LDR (PC-relative)" << std::endl;
    logFile << get_indent() << "PC alinhado: 0x" << std::hex << pc_alinhado
            << " | Offset: 0x" << (imm8 << 2) << " (imm8: 0x" << imm8 << ")" << std::dec << std::endl;
    logFile << get_indent() << "Endereço final: 0x" << std::hex << endereco << std::dec << std::endl;

    uint32_t resultado = cpu_read32(endereco);
    set_reg(Rd, resultado);
    logFile << get_indent() << "Lido da memória: 0x" << std::hex << resultado
            << std::dec << " -> escrito em get_reg(" << (int)Rd << ")" << std::endl;

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::thumb_format_7(uint16_t opcode)
{
    logFile << get_indent() << "[THUMB Format 7 - Load/Store Register Offset] {" << std::endl;
    indent_level++;

    bool L = (opcode >> 11) & 1; // 1 = Load, 0 = Store
    bool B = (opcode >> 10) & 1; // 1 = Byte, 0 = Word

    uint8_t Ro = (opcode >> 6) & 0b111;
    uint8_t Rb = (opcode >> 3) & 0b111;
    uint8_t Rd = opcode & 0b111;

    uint32_t endereco = get_reg(Rb) + get_reg(Ro);

    logFile << get_indent() << "Operação: " << (L ? "LDR" : "STR") << (B ? "B" : "") << std::endl;
    logFile << get_indent() << "get_reg(" << (int)Rb << ") base: 0x" << std::hex << get_reg(Rb)
            << " | get_reg(" << (int)Ro << ") offset: 0x" << get_reg(Ro) << std::dec << std::endl;
    logFile << get_indent() << "Endereço final: 0x" << std::hex << endereco << std::dec << std::endl;

    if (L) // Operations: LDR / LDRB
    {
        if (B)
        {
            uint32_t resultado = cpu_read8(endereco); // LDRB (Zero-extended)
            set_reg(Rd, resultado);
            logFile << get_indent() << "Lido da memória (Byte): 0x" << std::hex << resultado
                    << std::dec << " -> escrito em get_reg(" << (int)Rd << ")" << std::endl;
        }
        else
        {
            uint32_t resultado = cpu_read32(endereco); // LDR
            set_reg(Rd, resultado);
            logFile << get_indent() << "Lido da memória (Word): 0x" << std::hex << resultado
                    << std::dec << " -> escrito em get_reg(" << (int)Rd << ")" << std::endl;
        }
    }
    else // Operations: STR / STRB
    {
        if (B)
        {
            memoria->set8(endereco, get_reg(Rd) & 0xFF); // STRB
            logFile << get_indent() << "Escrito na memória (Byte): 0x" << std::hex << (get_reg(Rd) & 0xFF)
                    << std::dec << " no endereço 0x" << std::hex << endereco << std::dec << std::endl;
        }
        else
        {
            memoria->set32(endereco, get_reg(Rd)); // STR
            logFile << get_indent() << "Escrito na memória (Word): 0x" << std::hex << get_reg(Rd)
                    << std::dec << " no endereço 0x" << std::hex << endereco << std::dec << std::endl;
        }
    }

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::thumb_format_8(uint16_t opcode)
{
    logFile << get_indent() << "[THUMB Format 8 - Load/Store Sign-Extended Byte/Halfword] {" << std::endl;
    indent_level++;

    bool H = (opcode >> 11) & 1;
    bool S = (opcode >> 10) & 1;
    uint8_t Ro = (opcode >> 6) & 0b111;
    uint8_t Rb = (opcode >> 3) & 0b111;
    uint8_t Rd = opcode & 0b111;

    uint32_t addr = get_reg(Rb) + get_reg(Ro);

    const char *opName = (!S && !H) ? "STRH" : (!S && H) ? "LDRH"
                                           : (S && !H)   ? "LDSB"
                                                         : "LDSH";

    logFile << get_indent() << "Operação: " << opName << std::endl;
    logFile << get_indent() << "get_reg(" << (int)Rb << ") base: 0x" << std::hex << get_reg(Rb)
            << " | get_reg(" << (int)Ro << ") offset: 0x" << get_reg(Ro) << std::dec << std::endl;
    logFile << get_indent() << "Endereço final: 0x" << std::hex << addr << std::dec << std::endl;

    if (!S && !H)
    {
        memoria->set16(addr, get_reg(Rd) & 0xFFFF);
        logFile << get_indent() << "Escrito na memória (Halfword): 0x" << std::hex << (get_reg(Rd) & 0xFFFF)
                << std::dec << " no endereço 0x" << std::hex << addr << std::dec << std::endl;
    }
    else if (!S && H)
    {
        uint32_t resultado = cpu_read16(addr);
        set_reg(Rd, resultado);
        logFile << get_indent() << "Lido da memória (LDRH): 0x" << std::hex << resultado
                << std::dec << " -> escrito em get_reg(" << (int)Rd << ")" << std::endl;
    }
    else if (S && !H)
    {
        uint32_t val = cpu_read8(addr);
        set_reg(Rd, static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(val))));
        logFile << get_indent() << "Lido da memória com extensão de sinal (LDSB): 0x" << std::hex << get_reg(Rd)
                << std::dec << " -> escrito em get_reg(" << (int)Rd << ")" << std::endl;
    }
    else if (S && H)
    {
        uint32_t val = cpu_read16(addr);
        set_reg(Rd, static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(val))));
        logFile << get_indent() << "Lido da memória com extensão de sinal (LDSH): 0x" << std::hex << get_reg(Rd)
                << std::dec << " -> escrito em get_reg(" << (int)Rd << ")" << std::endl;
    }

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::thumb_format_9(uint16_t opcode)
{
    logFile << get_indent() << "[THUMB Format 9 - Load/Store Immediate Offset] {" << std::endl;
    indent_level++;

    bool B = (opcode >> 12) & 1; // 0 = Word, 1 = Byte
    bool L = (opcode >> 11) & 1; // 0 = Store, 1 = Load
    uint32_t imm5 = (opcode >> 6) & 0x1F;
    uint8_t Rb = (opcode >> 3) & 0b111;
    uint8_t Rd = opcode & 0b111;

    uint32_t offset = B ? imm5 : (imm5 << 2);
    uint32_t addr = get_reg(Rb) + offset;

    logFile << get_indent() << "Operação: " << (L ? "LDR" : "STR") << (B ? "B" : "") << std::endl;
    logFile << get_indent() << "get_reg(" << (int)Rb << ") base: 0x" << std::hex << get_reg(Rb)
            << " | Offset (Imediato): 0x" << offset << std::dec << std::endl;
    logFile << get_indent() << "Endereço final: 0x" << std::hex << addr << std::dec << std::endl;

    if (L)
    {
        if (B)
        {
            uint32_t resultado = cpu_read8(addr);
            set_reg(Rd, resultado);
            logFile << get_indent() << "Lido da memória (Byte): 0x" << std::hex << resultado
                    << std::dec << " -> escrito em get_reg(" << (int)Rd << ")" << std::endl;
        }
        else
        {
            uint32_t resultado = cpu_read32(addr);
            set_reg(Rd, resultado);
            logFile << get_indent() << "Lido da memória (Word): 0x" << std::hex << resultado
                    << std::dec << " -> escrito em get_reg(" << (int)Rd << ")" << std::endl;
        }
    }
    else
    {
        if (B)
        {
            memoria->set8(addr, get_reg(Rd) & 0xFF);
            logFile << get_indent() << "Escrito na memória (Byte): 0x" << std::hex << (get_reg(Rd) & 0xFF)
                    << std::dec << " no endereço 0x" << std::hex << addr << std::dec << std::endl;
        }
        else
        {
            memoria->set32(addr, get_reg(Rd));
            logFile << get_indent() << "Escrito na memória (Word): 0x" << std::hex << get_reg(Rd)
                    << std::dec << " no endereço 0x" << std::hex << addr << std::dec << std::endl;
        }
    }

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::thumb_format_10(uint16_t opcode)
{
    logFile << get_indent() << "[THUMB Format 10 - Load/Store Halfword Immediate Offset] {" << std::endl;
    indent_level++;

    bool L = (opcode >> 11) & 1;
    uint32_t imm5 = (opcode >> 6) & 0x1F;
    uint8_t Rb = (opcode >> 3) & 0b111;
    uint8_t Rd = opcode & 0b111;

    uint32_t offset = imm5 << 1;
    uint32_t addr = get_reg(Rb) + offset;

    logFile << get_indent() << "Operação: " << (L ? "LDRH" : "STRH") << std::endl;
    logFile << get_indent() << "get_reg(" << (int)Rb << ") base: 0x" << std::hex << get_reg(Rb)
            << " | Offset (Imediato): 0x" << offset << std::dec << std::endl;
    logFile << get_indent() << "Endereço final: 0x" << std::hex << addr << std::dec << std::endl;

    if (L)
    {
        uint32_t resultado = cpu_read16(addr);
        set_reg(Rd, resultado);
        logFile << get_indent() << "Lido da memória (Halfword): 0x" << std::hex << resultado
                << std::dec << " -> escrito em get_reg(" << (int)Rd << ")" << std::endl;
    }
    else
    {
        memoria->set16(addr, get_reg(Rd) & 0xFFFF);
        logFile << get_indent() << "Escrito na memória (Halfword): 0x" << std::hex << (get_reg(Rd) & 0xFFFF)
                << std::dec << " no endereço 0x" << std::hex << addr << std::dec << std::endl;
    }

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::thumb_format_11(uint16_t opcode)
{
    logFile << get_indent() << "[THUMB Format 11 - SP-Relative Load/Store] {" << std::endl;
    indent_level++;

    bool L = (opcode >> 11) & 1;
    uint8_t Rd = (opcode >> 8) & 0b111;
    uint32_t imm8 = opcode & 0xFF;

    uint32_t offset = imm8 << 2;
    uint32_t addr = get_reg(13) + offset;

    logFile << get_indent() << "Operação: " << (L ? "LDR" : "STR") << " (SP-relative)" << std::endl;
    logFile << get_indent() << "SP (R13) base: 0x" << std::hex << get_reg(13)
            << " | Offset (Imediato): 0x" << offset << std::dec << std::endl;
    logFile << get_indent() << "Endereço final: 0x" << std::hex << addr << std::dec << std::endl;

    if (L)
    {
        uint32_t resultado = cpu_read32(addr);
        set_reg(Rd, resultado);
        logFile << get_indent() << "Lido da memória: 0x" << std::hex << resultado
                << std::dec << " -> escrito em get_reg(" << (int)Rd << ")" << std::endl;
    }
    else
    {
        memoria->set32(addr, get_reg(Rd));
        logFile << get_indent() << "Escrito na memória: 0x" << std::hex << get_reg(Rd)
                << std::dec << " no endereço 0x" << std::hex << addr << std::dec << std::endl;
    }

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::thumb_format_12(uint16_t opcode)
{
    logFile << get_indent() << "[THUMB Format 12 - Get Relative Address] {" << std::endl;
    indent_level++;

    bool isSP = (opcode >> 11) & 1;
    uint8_t Rd = (opcode >> 8) & 0b111;
    uint32_t imm8 = opcode & 0xFF;
    uint32_t offset = imm8 << 2;

    uint32_t base = 0;
    if (isSP)
    {
        base = get_reg(13);
    }
    else
    {
        base = get_reg(15) & ~2U;
    }

    uint32_t resultado = base + offset;
    set_reg(Rd, resultado);

    logFile << get_indent() << "Operação: ADD R" << (int)Rd << ", " << (isSP ? "SP" : "PC") << ", #0x" << std::hex << offset << std::dec << std::endl;
    logFile << get_indent() << "Base (" << (isSP ? "SP" : "PC alinhado") << "): 0x" << std::hex << base << std::dec << std::endl;
    logFile << get_indent() << "Resultado: 0x" << std::hex << resultado << std::dec << " -> escrito em get_reg(" << (int)Rd << ")" << std::endl;

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::thumb_format_13(uint16_t opcode)
{
    logFile << get_indent() << "[THUMB Format 13 - Add/Subtract Offset to Stack Pointer] {" << std::endl;
    indent_level++;

    bool isSub = (opcode >> 7) & 1;
    uint32_t imm7 = opcode & 0x7F;
    uint32_t offset = imm7 << 2;

    logFile << get_indent() << "Operação: " << (isSub ? "SUB" : "ADD") << " SP, #0x" << std::hex << offset << std::dec << std::endl;
    logFile << get_indent() << "SP (R13) anterior: 0x" << std::hex << get_reg(13) << std::dec << std::endl;

    if (isSub)
    {
        set_reg(13, get_reg(13) - (offset));
    }
    else
    {
        set_reg(13, get_reg(13) + (offset));
    }

    logFile << get_indent() << "SP (R13) atualizado: 0x" << std::hex << get_reg(13) << std::dec << std::endl;

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::thumb_format_14(uint16_t opcode)
{
    logFile << get_indent() << "[THUMB Format 14 - Push/Pop Registers] {" << std::endl;
    indent_level++;

    bool isPop = (opcode >> 11) & 1;
    bool storeLR_loadPC = (opcode >> 8) & 1;
    uint8_t regList = opcode & 0xFF;

    logFile << get_indent() << "Operação: " << (isPop ? "POP" : "PUSH") << std::endl;
    logFile << get_indent() << "SP (R13) inicial: 0x" << std::hex << get_reg(13) << std::dec << std::endl;

    if (!isPop) // PUSH
    {
        if (storeLR_loadPC)
        {
            set_reg(13, get_reg(13) - 4);
            memoria->set32(get_reg(13), get_reg(14));
            logFile << get_indent() << "PUSH R14 (LR): 0x" << std::hex << get_reg(14) << " em 0x" << get_reg(13) << std::dec << std::endl;
        }
        for (int i = 7; i >= 0; i--)
        {
            if ((regList >> i) & 1)
            {
                set_reg(13, get_reg(13) - 4);
                memoria->set32(get_reg(13), get_reg(i));
                logFile << get_indent() << "PUSH get_reg(" << i << "): 0x" << std::hex << get_reg(i) << " em 0x" << get_reg(13) << std::dec << std::endl;
            }
        }
    }
    else // POP
    {
        for (int i = 0; i <= 7; i++)
        {
            if ((regList >> i) & 1)
            {
                uint32_t val = cpu_read32(get_reg(13));
                set_reg(i, val);
                logFile << get_indent() << "POP get_reg(" << i << "): 0x" << std::hex << val << " de 0x" << get_reg(13) << std::dec << std::endl;
                set_reg(13, get_reg(13) + 4);
            }
        }
        if (storeLR_loadPC)
        {
            uint32_t val = cpu_read32(get_reg(13));
            set_reg(15, val & ~1U); // Garante alinhamento de instrução no PC
            logFile << get_indent() << "POP PC (R15): 0x" << std::hex << get_reg(15) << " de 0x" << get_reg(13) << std::dec << std::endl;
            set_reg(13, get_reg(13) + 4);
        }
    }

    logFile << get_indent() << "SP (R13) final: 0x" << std::hex << get_reg(13) << std::dec << std::endl;

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::thumb_format_15(uint16_t opcode)
{
    logFile << get_indent() << "[THUMB Format 15 - Multiple Load/Store] {" << std::endl;
    indent_level++;

    bool isLoad = (opcode >> 11) & 1;
    uint8_t Rb = (opcode >> 8) & 0b111;
    uint8_t regList = opcode & 0xFF;

    uint32_t addr = get_reg(Rb);

    logFile << get_indent() << "Operação: " << (isLoad ? "LDMIA" : "STMIA") << std::endl;
    logFile << get_indent() << "get_reg(" << (int)Rb << ") base inicial: 0x" << std::hex << addr << std::dec << std::endl;

    for (int i = 0; i <= 7; i++)
    {
        if ((regList >> i) & 1)
        {
            if (isLoad)
            {
                uint32_t val = cpu_read32(addr);
                set_reg(i, val);
                logFile << get_indent() << "Lido da memória 0x" << std::hex << addr << ": 0x" << val << " -> escrito em get_reg(" << std::dec << i << ")" << std::endl;
            }
            else
            {
                memoria->set32(addr, get_reg(i));
                logFile << get_indent() << "Escrito na memória 0x" << std::hex << addr << ": get_reg(" << std::dec << i << ") (0x" << std::hex << get_reg(i) << ")" << std::endl;
            }
            addr += 4;
        }
    }

    // Writeback só se aplica se for STMIA ou se Rb NÃO foi um dos registradores carregados
    if (!isLoad || !((regList >> Rb) & 1))
    {
        set_reg(Rb, addr);
        logFile << get_indent() << "Writeback get_reg(" << (int)Rb << ") atualizado para 0x" << std::hex << get_reg(Rb) << std::dec << std::endl;
    }

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::thumb_format_16(uint16_t opcode)
{
    logFile << get_indent() << "[THUMB Format 16 - Conditional Branch] {" << std::endl;
    indent_level++;

    uint8_t cond = (opcode >> 8) & 0xF;
    int32_t offset = (int8_t)(opcode & 0xFF);

    if (cond == 0xF)
    {
        indent_level--;
        logFile << get_indent() << "}" << std::endl;
        return; // SWI (Formato 17)
    }

    const char *condNames[] = {
        "EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC",
        "HI", "LS", "GE", "LT", "GT", "LE", "AL", "NV"};

    bool satisfeita = checar_condicao((cond << 28));
    logFile << get_indent() << "Condição: " << condNames[cond] << " (" << (satisfeita ? "Verdadeira" : "Falsa") << ")" << std::endl;
    logFile << get_indent() << "Offset: " << (offset << 1) << " bytes" << std::endl;

    if (satisfeita)
    {
        set_reg(15, get_reg(15) + ((offset << 1)));
        logFile << get_indent() << "Desvio realizado -> Novo PC: 0x" << std::hex << R[15] << std::dec << std::endl;
    }
    else
    {
        logFile << get_indent() << "Desvio não realizado" << std::endl;
    }

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::thumb_format_17(uint16_t opcode)
{
    logFile << get_indent() << "[THUMB Format 17 - Software Interrupt (SWI)] {" << std::endl;
    indent_level++;

    uint8_t comment = opcode & 0xFF;
    logFile << get_indent() << "[A FAZER] SWI THUMB executado (Comentário/Vetor: 0x" << std::hex << (int)comment << std::dec << ")" << std::endl;

    opcode += 1;

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::thumb_format_18(uint16_t opcode)
{
    logFile << get_indent() << "[THUMB Format 18 - Unconditional Branch] {" << std::endl;
    indent_level++;

    int16_t imm11 = opcode & 0x7FF;
    if (imm11 & 0x400)
    {
        imm11 |= 0xF800;
    }

    int32_t offset = (int32_t)imm11 << 1;

    logFile << get_indent() << "Offset do desvio: " << offset << " bytes" << std::endl;
    set_reg(15, get_reg(15) + (offset));
    logFile << get_indent() << "Novo PC: 0x" << std::hex << get_reg(15) << std::dec << std::endl;

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::thumb_format_19(uint16_t opcode)
{
    logFile << get_indent() << "[THUMB Format 19 - Long Branch with Link (BL)] {" << std::endl;
    indent_level++;

    bool H = (opcode >> 11) & 1;
    uint32_t offset11 = opcode & 0x7FF;

    logFile << get_indent() << "Etapa: " << (H ? "Parte 2 (Segunda instrução)" : "Parte 1 (Primeira instrução)") << std::endl;

    if (!H)
    {
        int32_t off = offset11;
        if (off & 0x400)
        {
            off |= 0xFFFFF800;
        }
        set_reg(14, get_reg(15) + (off << 12));
        logFile << get_indent() << "Offset alto (sign-extended): " << (off << 12) << std::endl;
        logFile << get_indent() << "LR (R14) pré-calculado: 0x" << std::hex << get_reg(14) << std::dec << std::endl;
    }
    else
    {
        uint32_t next_pc = get_reg(14) + (offset11 << 1);
        set_reg(14, (get_reg(15) - 2) | 1);
        set_reg(15, next_pc & ~1U);

        logFile << get_indent() << "Endereço de retorno salvo em LR (R14): 0x" << std::hex << get_reg(14) << std::dec << std::endl;
        logFile << get_indent() << "Novo PC (R15): 0x" << std::hex << get_reg(15) << std::dec << std::endl;
    }

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}
void Cpu::step_thumb()
{
    // 1. FETCH & ATUALIZAÇÃO DO BIOS LATCH
    uint32_t pc_atual = get_reg(15);
    uint16_t opcode = memoria->get16(pc_atual).result;

    // Em Thumb (16 bits), o barramento de 32 bits da BIOS duplica a halfword lida
    if (pc_atual < 0x00004000)
    {
        bios_latch = (static_cast<uint32_t>(opcode) << 16) | opcode;
    }

    // Prepara a flag para detectar se a instrução atual modificou o PC
    pc_alterado = false;

    logFile << get_indent() << "[STEP THUMB] {" << std::endl;
    indent_level++;
    logFile << get_indent() << "PC atual: 0x" << std::hex << pc_atual << std::dec << std::endl;
    logFile << get_indent() << "Opcode lido: 0x" << std::hex << opcode << std::dec << std::endl;

    // 2. DECODE & DISPATCH (Bits 15..11)
    uint8_t top5 = opcode >> 11;

    switch (top5)
    {
    case 0x00:
    case 0x01:
    case 0x02:
        thumb_format_1(opcode);
        break;

    case 0x03:
        thumb_format_2(opcode);
        break;

    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
        thumb_format_3(opcode);
        break;

    case 0x08:
        if ((opcode & 0x0400) == 0)
            thumb_format_4(opcode);
        else
            thumb_format_5(opcode);
        break;

    case 0x09:
        thumb_format_6(opcode);
        break;

    case 0x0A:
    case 0x0B:
        if ((opcode & 0x0200) == 0)
            thumb_format_7(opcode);
        else
            thumb_format_8(opcode);
        break;

    case 0x0C:
    case 0x0D:
    case 0x0E:
    case 0x0F:
        thumb_format_9(opcode);
        break;

    case 0x10:
    case 0x11:
        thumb_format_10(opcode);
        break;

    case 0x12:
    case 0x13:
        thumb_format_11(opcode);
        break;

    case 0x14:
    case 0x15:
        thumb_format_12(opcode);
        break;

    case 0x16:
    case 0x17:
        if ((opcode & 0xFF00) == 0xB000)
        {
            thumb_format_13(opcode);
        }
        else if ((opcode & 0xF600) == 0xB400)
        {
            thumb_format_14(opcode);
        }
        else
        {
            logFile << get_indent() << "[ERRO CPU] Opcode THUMB invalido (0xB...): 0x" << std::hex << opcode
                    << " no PC: 0x" << pc_atual << std::dec << std::endl;
            interrupt(CpuMode::Undefined, 0x00000004);
        }
        break;

    case 0x18:
    case 0x19:
        thumb_format_15(opcode);
        break;

    case 0x1A:
    case 0x1B:
        if ((opcode & 0xFF00) == 0xDF00)
        {
            interrupt(CpuMode::Supervisor, 0x00000008); // SWI em Thumb
        }
        else
        {
            thumb_format_16(opcode);
        }
        break;

    case 0x1C:
        thumb_format_18(opcode);
        break;

    case 0x1E:
    case 0x1F:
        thumb_format_19(opcode);
        break;

    default:
        logFile << get_indent() << "[ERRO CPU] Opcode THUMB nao mapeado: 0x" << std::hex << opcode
                << " no PC: 0x" << pc_atual << std::dec << std::endl;
        interrupt(CpuMode::Undefined, 0x00000004);
        break;
    }

    // Se NENHUM salto foi executado pela instrução, avança o PC em +2 bytes
    if (!pc_alterado)
    {
        set_reg(15, get_reg(15) + 2);
    }

    indent_level--;
    logFile << get_indent() << "}" << std::endl;
}

void Cpu::interrupt(CpuMode mode, uint32_t jumpAddr)
{
    uint32_t oldCPSR = CPSR;
    bool estavaEmThumb = (CPSR & (1 << 5)) != 0;

    // 1. Cálculo do LR de acordo com o tipo de exceção
    uint32_t PCretorno;

    switch (mode)
    {
    case CpuMode::Supervisor: // SWI
    case CpuMode::Undefined:  // Instrução Inválida
        // SWI e Undefined completam ou abortam a instrução apontando para a próxima
        PCretorno = R[15] + (estavaEmThumb ? 2 : 4);
        break;

    case CpuMode::IRQ: // Interrupção de Hardware
    case CpuMode::FIQ: // Fast Interrupt
        // IRQ e FIQ abortam no início do fetch (precisam compensar o -4 do SUBS)
        PCretorno = R[15] + 4;
        break;

    default:
        PCretorno = R[15] + 4;
        break;
    }

    // 2. Transição de CPSR
    CPSR &= ~(0x1F);                     // Limpa bits de modo
    CPSR |= static_cast<uint32_t>(mode); // Seta novo modo (Supervisor, FIQ, etc)
    set_bit(CPSR, 5, false);             // Força modo ARM (T = 0)
    set_bit(CPSR, 7, true);              // Desativa IRQs normais (Bit I = 1)

    // Se for FIQ, a arquitetura ARM também desativa novos FIQs (Bit F = 1)
    if (mode == CpuMode::FIQ)
    {
        set_bit(CPSR, 6, true); // Bit F = 1
    }

    // 3. Atualiza ponteiros de registradores bancados
    update_reg_map();

    // 4. Salva o contexto antigo no SPSR do NOVO modo
    *SPSRatual = oldCPSR;

    // 5. Configura o retorno e o salto do vetor
    set_reg(14, PCretorno);
    set_reg(15, jumpAddr);
}

void Cpu::step()
{
    // Leitura crua dos registradores de 16 bits
    uint16_t IE = memoria->get16(static_cast<uint32_t>(IorAddr::IE)).result;
    uint16_t IF = memoria->get16(static_cast<uint32_t>(IorAddr::IF)).result;
    uint16_t IME = memoria->get16(static_cast<uint32_t>(IorAddr::IME)).result;

    bool IRQhabilitada = get_bit(CPSR, 7) == 0; // Bit I = 0 no CPSR

    // Checa se o bit 0 do IME está ativo E se há bit comum entre IE e IF
    bool irqPendente = (IME & 1) && ((IE & IF) != 0) && IRQhabilitada;

    if (irqPendente)
    {
        interrupt(CpuMode::IRQ, 0x00000018);
        return;
    }

    pc_alterado = false;
    if (get_bit(CPSR, 5))
    {
        step_thumb();
    }
    else
    {
        step_arm();
    }
}

void Cpu::set_r(uint8_t index, uint32_t value)
{
    if (index < 16)
    {
        set_reg(index, value);
    }
    else if (index == 16)
    {
        CPSR = value;
    }
}
uint32_t Cpu::get_r(uint8_t index)
{
    if (index < 16)
    {
        return get_reg(index);
    }
    else if (index == 16)
    {
        return CPSR;
    }
    return 0;
}
