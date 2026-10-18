#include "cpu.hpp"
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
    OpResult returning(resultado, 0, true);
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

void Cpu::alu(uint32_t opcode)
{
    logFile << get_indent() << "[ALU] {\n";
    indent_level++;
    // Registradores
    uint8_t Rn = (opcode >> 16) & 0xF;
    uint8_t Rd = (opcode >> 12) & 0xF;
    logFile << get_indent() << "Rn: 0x" << std::hex << R[Rn] << " Rd: 0x" << R[Rd] << std::dec << "\n";

    // flags da alu
    bool updateFlag = (opcode >> 20) & 0x1;
    bool imediatoFlag = (opcode >> 25) & 0x1;
    logFile << get_indent() << "Atualizar flags: " << ((updateFlag) ? "True" : "False") << "\n";
    logFile << get_indent() << "Operação imediata: " << ((imediatoFlag) ? "True" : "False") << "\n";

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
        logFile << get_indent() << "Valor imediato: " << std::hex << nn << std::dec << "\n";
        logFile << get_indent() << "Contagem de ROR: " << +rorCount << "\n";

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
            count = R[Rs] & 0xFF;
        }

        uint8_t shiftType = (opcode >> 5) & 0b11;

        OpResult op2 = shift_barrier(shiftType, R[Rm], count, origemImediato, flagC);

        val = op2.result;
        flagC = op2.ok;
    }

    // resultado e se pode escrever o resultado em rd
    uint32_t resultado;
    bool escrever_rd = true;

    // identificador de tipo de operação
    // atualiza flags se necessario
    // pode setar a escrita de rd como falso
    // atualiza o resultado
    logFile << get_indent() << "Segundo Operando: " << std::hex << val << std::dec << "\n";
    logFile << get_indent() << "Tipo de operação: ";
    uint8_t alu_op = (opcode >> 21) & 0xF;
    switch (alu_op)
    {
    case 0x0:
        logFile << "AND\n";
        resultado = R[Rn] & val;
        break;

    case 0x1:
        logFile << "EOR\n";
        resultado = R[Rn] ^ val;
        break;

    case 0x2:
        logFile << "SUB\n";
        resultado = R[Rn] - val;
        flagC = (R[Rn] >= val);
        flagV = ((R[Rn] ^ val) & (R[Rn] ^ resultado)) >> 31;
        break;

    case 0x3:
        logFile << "RSB\n";
        resultado = val - R[Rn];
        flagC = (val >= R[Rn]);
        flagV = ((R[Rn] ^ val) & (val ^ resultado)) >> 31;
        break;

    case 0x4:
        logFile << "ADD\n";
        resultado = R[Rn] + val;
        flagC = ((uint64_t)R[Rn] + (uint64_t)val) > UINT32_MAX;
        flagV = ((R[Rn] ^ resultado) & (val ^ resultado)) >> 31;
        break;

    case 0x5:
        logFile << "ADC\n";
        resultado = R[Rn] + val + Carry;
        flagC = ((uint64_t)R[Rn] + (uint64_t)val + Carry) > UINT32_MAX;
        flagV = ((R[Rn] ^ resultado) & (val ^ resultado)) >> 31;
        break;

    case 0x6:
        logFile << "SBC\n";
        resultado = R[Rn] - val - !Carry;
        flagC = ((uint64_t)R[Rn] - (uint64_t)val - !Carry) <= UINT32_MAX;
        flagV = ((R[Rn] ^ val) & (R[Rn] ^ resultado)) >> 31;
        break;

    case 0x7:
        logFile << "RSC\n";
        resultado = val - R[Rn] - !Carry;
        flagC = ((uint64_t)val - (uint64_t)R[Rn] - !Carry) <= UINT32_MAX;
        flagV = ((R[Rn] ^ val) & (val ^ resultado)) >> 31;
        break;

    case 0x8:
        logFile << "TST\n";
        resultado = R[Rn] & val;
        escrever_rd = false;
        break;

    case 0x9:
        logFile << "TEQ\n";
        resultado = R[Rn] ^ val;
        escrever_rd = false;
        break;

    case 0xA:
        logFile << "CMP\n";
        resultado = R[Rn] - val;
        escrever_rd = false;
        flagC = (R[Rn] >= val);
        flagV = ((R[Rn] ^ val) & (R[Rn] ^ resultado)) >> 31;
        break;

    case 0xB:
        logFile << "CMN\n";
        resultado = R[Rn] + val;
        flagC = ((uint64_t)R[Rn] + (uint64_t)val) > UINT32_MAX;
        flagV = ((R[Rn] ^ resultado) & (val ^ resultado)) >> 31;
        escrever_rd = false;
        break;

    case 0xC:
        logFile << "ORR\n";
        resultado = R[Rn] | val;
        break;

    case 0xD:
        logFile << "MOV\n";
        resultado = val;
        break;

    case 0xE:
        logFile << "BIC\n";
        resultado = R[Rn] & (~val);
        break;

    case 0xF:
        logFile << "MVN\n";
        resultado = ~val;
        break;

    default:
        break;
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
        logFile << get_indent() << "Flags atualizadas: N=" << flagN << " Z=" << flagZ << " C=" << flagC << " V=" << flagV << "\n";
    }

    // pode escrever o resultador em Rd ?
    if (escrever_rd)
    {
        logFile << get_indent() << "Resultado escrito em R[" << +Rd << "]: 0x" << std::hex << resultado << std::dec << "\n";
        R[Rd] = resultado;
    }
    indent_level--;
    logFile << get_indent() << "}\n";
}
void Cpu::mul(uint32_t opcode)
{
    logFile << get_indent() << "[MUL] {\n";
    indent_level++;
    // é pra atualizar flags?

    bool updateFlag = (opcode >> 20) & 0x1;
    logFile << get_indent() << "Atualizar flags: " << ((updateFlag) ? "True" : "False") << "\n";

    // registradores
    uint8_t Rd = (opcode >> 16) & 0xF;
    uint8_t Rn = (opcode >> 12) & 0xF;
    uint8_t Rs = (opcode >> 8) & 0xF;
    uint8_t Rm = opcode & 0xF;
    uint8_t RdHigh = Rd;
    uint8_t RdLow = Rn;
    logFile << get_indent() << "Rd: " << std::hex << R[Rd] << " Rn: " << R[Rn] << " Rs: " << R[Rs] << " Rm: " << R[Rm] << std::dec << "\n";

    // identificador e modificadores da multiplicação
    uint8_t mulType = (opcode >> 4) & 0xF;
    bool longFlag = (opcode >> 23) & 0x1;
    bool signedFlag = ((opcode >> 22) & 0x1);
    bool aculmulativo = (opcode >> 21) & 0x1;
    logFile << get_indent() << "Tipo de multiplicação: " << std::hex << (int)mulType << std::dec << "\n";
    logFile << get_indent() << "Multiplicação longa: " << ((longFlag) ? "True" : "False") << "\n";
    logFile << get_indent() << "Multiplicação com sinal: " << ((signedFlag) ? "True" : "False") << "\n";
    logFile << get_indent() << "Multiplicação acumulativa: " << ((aculmulativo) ? "True" : "False") << "\n";

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
            uint64_t Rd_64 = ((uint64_t)R[RdHigh] << 32) | R[RdLow];

            if (signedFlag)
            {
                /*
                com o signed é pra a mutiplicação respeitar o sinal usei o cast do proprio c++
                para passar pra int32 preservando o sinal e dpeois pra int64 se eu passasse
                direto pra int64 ele n traduziria o sinal e daria um positivo
                */
                buffer_result = ((int64_t)(int32_t)R[Rm] * (int64_t)(int32_t)R[Rs]) + (int64_t)Rd_64;
            }
            else
            {
                buffer_result = ((uint64_t)R[Rm] * (uint64_t)R[Rs]) + Rd_64;
            }
        }
        else
        {
            // mesma logica mas sem somar com Rd_64
            if (signedFlag)
            {
                buffer_result = (int64_t)((int32_t)R[Rm] * (int64_t)(int32_t)R[Rs]);
            }
            else
            {
                buffer_result = ((uint64_t)R[Rm] * (uint64_t)R[Rs]);
            }
        }
        // pega o resultado normal e deixa com os bits altos e já seta os Rd baixo com o resultado baixo
        resultado = (buffer_result >> 32) & UINT32_MAX;
        resultadoLow = buffer_result & UINT32_MAX;
        R[RdLow] = resultadoLow;
        logFile << get_indent() << "Resultado baixo escrito em R[" << +RdLow << "]: 0x" << std::hex << resultadoLow << std::dec << "\n";
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
            resultado = R[Rm] * R[Rs] + R[Rn];
        }
        else
        {
            resultado = R[Rm] * R[Rs];
        }
    }

    // é pra atualizar as flags?
    if (updateFlag)
    {
        set_bit(CPSR, 31, get_bit(resultado, 31));
        set_bit(CPSR, 30, (resultado == 0 && resultadoLow == 0));
    }

    // guarda o resultado em Rd se longFlag o resultado vai ser a parte alta e Rd tem o mesmo valor que RdHigh o RdLow já foi setado corretamente
    R[Rd] = resultado;
    logFile << get_indent() << "Resultado escrito em R[" << +Rd << "]: 0x" << std::hex << resultado << std::dec << "\n";

    indent_level--;
    logFile << get_indent() << "}\n";
}
void Cpu::branch(uint32_t opcode)
{
    logFile << get_indent() << "[BRANCH] {\n";
    indent_level++;

    bool linkBranch = (opcode >> 24) & 0x1;
    logFile << get_indent() << "Tipo: " << (linkBranch ? "BL (Branch with Link)" : "B (Branch)") << "\n";

    // Extrai 24 bits, estende o sinal para 32 bits e multiplica por 4 (Gambiarra)
    int32_t offset = (static_cast<int32_t>(opcode & 0x00FFFFFF) << 8) >> 6;
    logFile << get_indent() << "Offset de salto: " << offset << " bytes\n";
    logFile << get_indent() << "PC atual (R15): 0x" << std::hex << R[15] << std::dec << "\n";

    // Se for BL (Branch with Link), salva o endereço de retorno (próxima instrução)
    if (linkBranch)
    {
        R[14] = R[15] - 4; // R[15] já é pc_atual + 4
        logFile << get_indent() << "LR (R14) salvo: 0x" << std::hex << R[14] << std::dec << "\n";
    }

    // O hardware ARM calcula o salto em relação a (PC_atual + 8)
    // Como R[15] vale (PC_atual + 4), somamos mais 4 para alinhar com o pipeline
    R[15] += offset;
    logFile << get_indent() << "Novo PC (R15): 0x" << std::hex << R[15] << std::dec << "\n";

    indent_level--;
    logFile << get_indent() << "}\n";
}
void Cpu::blx(uint32_t opcode)
{
    logFile << get_indent() << "[BLX] {\n";
    indent_level++;

    // pega o endereço do pulo em Rm
    uint8_t Rm = opcode & 0xF;
    uint32_t address = R[Rm];

    logFile << get_indent() << "Registrador Rm: R" << (int)Rm << " = 0x" << std::hex << address << std::dec << "\n";

    // flag pra se é pra trocar pro modo Thumb
    bool modoThumb = address & 0x1;
    logFile << get_indent() << "Trocar para modo Thumb: " << (modoThumb ? "True" : "False") << "\n";

    set_bit(CPSR, 5, modoThumb);

    if (modoThumb)
    {
        // alinha pra só mutiplos de 2
        R[15] = address & (~(uint32_t)1);
    }
    else
    {
        // alinha pra só mutiplos de 4
        R[15] = address & (~(uint32_t)3);
    }

    logFile << get_indent() << "Novo PC (R15) alinhado: 0x" << std::hex << R[15] << std::dec << "\n";

    indent_level--;
    logFile << get_indent() << "}\n";
}
void Cpu::msr(uint32_t opcode)
{
    logFile << get_indent() << "[MSR] {\n";
    indent_level++;

    bool imediato = (opcode >> 25) & 0x1; // imediato
    bool spsrFlag = (opcode >> 22) & 0x1; // olha se é pra pegar o spsr e colocar no cpsr ou o contrario

    logFile << get_indent() << "Destino: " << (spsrFlag ? "SPSR" : "CPSR") << "\n";
    logFile << get_indent() << "Tipo de valor: " << (imediato ? "Imediato" : "Registrador") << "\n";

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

    logFile << get_indent() << "Máscara de escrita: 0x" << std::hex << mask << std::dec << "\n";

    // se for pra colocar em sprs ele ajusta qual spsr dependendo do modo atual
    if (spsrFlag)
    {
        switch (modeBits)
        {
        case 0b10000:
        case 0b11111:
        case 0b10001:
        default:
            logFile << get_indent() << "Erro: Acesso inválido ao SPSR no modo 0x" << std::hex << (int)modeBits << std::dec << "\n";
            indent_level--;
            logFile << get_indent() << "}\n";
            return;

        case 0b10010:
            endereçoPSR = &SPSR_irq;
            break;

        case 0b10011:
            endereçoPSR = &SPSR_swi;
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
        logFile << get_indent() << "Valor imediato obtido: 0x" << std::hex << programState << std::dec << "\n";
    }
    else
    {
        uint8_t Rm = opcode & 0xF;
        programState = R[Rm];
        logFile << get_indent() << "Valor lido de R[" << (int)Rm << "]: 0x" << std::hex << programState << std::dec << "\n";
    }

    // aplica a mascara
    *endereçoPSR = (*endereçoPSR & ~mask) | (programState & mask);
    logFile << get_indent() << "Novo valor do registrador de status: 0x" << std::hex << *endereçoPSR << std::dec << "\n";

    indent_level--;
    logFile << get_indent() << "}\n";
}
void Cpu::mrs(uint32_t opcode)
{
    logFile << get_indent() << "[MRS] {\n";
    indent_level++;

    uint8_t Rd = (opcode >> 12) & 0xF;
    bool spsrFlag = (opcode >> 22) & 0x1;

    logFile << get_indent() << "Registrador destino: R" << (int)Rd << "\n";
    logFile << get_indent() << "Origem: " << (spsrFlag ? "SPSR" : "CPSR") << "\n";

    uint32_t *endereçoPSR = &CPSR;

    if (spsrFlag)
    {
        uint8_t modeBits = CPSR & 0x1F;
        switch (modeBits)
        {
        case 0b10000:
        case 0b11111:
        case 0b10001:
        default:
            logFile << get_indent() << "Erro: Acesso inválido ao SPSR no modo 0x" << std::hex << (int)modeBits << std::dec << "\n";
            indent_level--;
            logFile << get_indent() << "}\n";
            return;

        case 0b10010:
            endereçoPSR = &SPSR_irq;
            break;

        case 0b10011:
            endereçoPSR = &SPSR_swi;
            break;

        case 0b10111:
            endereçoPSR = &SPSR_abt;
            break;

        case 0b11011:
            endereçoPSR = &SPSR_und;
            break;
        }
    }

    R[Rd] = *endereçoPSR;
    logFile << get_indent() << "Valor lido (0x" << std::hex << *endereçoPSR << std::dec << ") escrito em R[" << (int)Rd << "]\n";

    indent_level--;
    logFile << get_indent() << "}\n";
}
void Cpu::sdt(uint32_t opcode)
{
    logFile << get_indent() << "[SDT] {\n";
    indent_level++;

    bool imediato = (opcode >> 25) & 0x1;
    bool preIndex = (opcode >> 24) & 0x1;
    bool upDown = (opcode >> 23) & 0x1;
    bool wordByte = (opcode >> 22) & 0x1;
    bool writeBack = (opcode >> 21) & 0x1;
    bool loadStore = (opcode >> 20) & 0x1;

    uint8_t Rd = (opcode >> 12) & 0xF;
    uint8_t Rn = (opcode >> 16) & 0xF;

    logFile << get_indent() << "Operação: " << (loadStore ? "LDR" : "STR") << (wordByte ? "B" : "") << "\n";
    logFile << get_indent() << "Indexação: " << (preIndex ? "Pré-indexado" : "Pós-indexado") << "\n";
    logFile << get_indent() << "Direção offset: " << (upDown ? "+" : "-") << "\n";
    logFile << get_indent() << "Writeback: " << (writeBack ? "True" : "False") << "\n";
    logFile << get_indent() << "R[" << (int)Rn << "] base: 0x" << std::hex << R[Rn]
            << " R[" << (int)Rd << "] valor: 0x" << R[Rd] << std::dec << "\n";

    uint32_t offset = 0;
    if (imediato)
    {
        uint8_t Rm = opcode & 0xF;
        uint8_t shiftType = (opcode >> 5) & 0b11;
        uint8_t shiftCount = (opcode >> 7) & 0x1F;

        OpResult op2 = shift_barrier(shiftType, R[Rm], shiftCount, true, get_bit(CPSR, 29));

        offset = op2.result;
        logFile << get_indent() << "Offset (R" << (int)Rm << " deslocado): 0x" << std::hex << offset << std::dec << "\n";
    }
    else
    {
        offset = opcode & 0xFFF;
        logFile << get_indent() << "Offset (Imediato): 0x" << std::hex << offset << std::dec << "\n";
    }

    uint32_t endereço = R[Rn];
    if (preIndex)
    {
        endereço = (upDown) ? R[Rn] + offset : R[Rn] - offset;
    }

    logFile << get_indent() << "Endereço final: 0x" << std::hex << endereço << std::dec << "\n";

    if (loadStore)
    {
        OpResult status;
        if (wordByte)
        {
            status = memoria->get8(endereço);
        }
        else
        {
            status = memoria->get32(endereço);
            uint8_t desalinhamentoEmBits = (endereço & 0b11) * 8;
            status.result = std::rotr(status.result, desalinhamentoEmBits);
        }
        if (status.ok)
        {
            R[Rd] = status.result;
            logFile << get_indent() << "Lido da memória: 0x" << std::hex << status.result << std::dec << " -> escrito em R[" << (int)Rd << "]\n";
        }
        else
        {
            logFile << get_indent() << "Falha ao ler o endereço: 0x" << std::hex << endereço << std::dec << "\n";
        }
    }
    else
    {
        if (wordByte)
        {
            memoria->set8(endereço, R[Rd] & 0xFF);
            logFile << get_indent() << "Escrito na memória (Byte): 0x" << std::hex << (R[Rd] & 0xFF) << std::dec << " no endereço 0x" << std::hex << endereço << std::dec << "\n";
        }
        else
        {
            memoria->set32(endereço, R[Rd]);
            logFile << get_indent() << "Escrito na memória (Word): 0x" << std::hex << R[Rd] << std::dec << " no endereço 0x" << std::hex << endereço << std::dec << "\n";
        }
    }

    if (writeBack || !preIndex)
    {
        if (!loadStore || (Rd != Rn))
        {
            R[Rn] = (upDown) ? R[Rn] + offset : R[Rn] - offset;
            logFile << get_indent() << "Writeback atualizou R[" << (int)Rn << "] para 0x" << std::hex << R[Rn] << std::dec << "\n";
        }
    }

    indent_level--;
    logFile << get_indent() << "}\n";
}
void Cpu::hw_s(uint32_t opcode)
{
    logFile << get_indent() << "[HW_S] {\n";
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
            << (signed_ ? "S" : "") << (halfword ? "H" : "B") << "\n";
    logFile << get_indent() << "Indexação: " << (preIndex ? "Pré-indexado" : "Pós-indexado") << "\n";
    logFile << get_indent() << "Direção offset: " << (upDown ? "+" : "-") << "\n";
    logFile << get_indent() << "Writeback: " << (writeBack ? "True" : "False") << "\n";
    logFile << get_indent() << "R[" << (int)Rn << "] base: 0x" << std::hex << R[Rn]
            << " R[" << (int)Rd << "] valor: 0x" << R[Rd] << std::dec << "\n";

    uint32_t offset = (offsetHigh << 4) | offsetLow;

    if (imediato)
    {
        offset = R[Rm];
        logFile << get_indent() << "Offset (R[" << (int)Rm << "]): 0x" << std::hex << offset << std::dec << "\n";
    }
    else
    {
        logFile << get_indent() << "Offset (Imediato): 0x" << std::hex << offset << std::dec << "\n";
    }

    uint32_t endereço = R[Rn];
    if (preIndex)
    {
        endereço = (upDown) ? R[Rn] + offset : R[Rn] - offset;
    }

    logFile << get_indent() << "Endereço final: 0x" << std::hex << endereço << std::dec << "\n";

    if (loadStore)
    {
        if (halfword)
        {
            OpResult status = memoria->get16(endereço);
            if (status.ok)
            {
                uint32_t resultado = status.result;
                if (signed_)
                {
                    resultado |= (((get_bit(status.result, 15)) ? 0xFFFF : 0x00) << 16);
                }
                R[Rd] = resultado;
                logFile << get_indent() << "Lido da memória (Halfword" << (signed_ ? " Signed" : "") << "): 0x"
                        << std::hex << resultado << std::dec << " -> escrito em R[" << (int)Rd << "]\n";
            }
            else
            {
                logFile << get_indent() << "Falha ao ler o endereço: 0x" << std::hex << endereço << std::dec << "\n";
            }
        }
        else
        {
            OpResult status = memoria->get8(endereço);
            if (status.ok)
            {
                uint32_t resultado = status.result;
                if (signed_)
                {
                    resultado |= (((get_bit(status.result, 7)) ? 0xFFFFFF : 0x00) << 8);
                }
                R[Rd] = resultado;
                logFile << get_indent() << "Lido da memória (Byte Signed): 0x"
                        << std::hex << resultado << std::dec << " -> escrito em R[" << (int)Rd << "]\n";
            }
            else
            {
                logFile << get_indent() << "Falha ao ler o endereço: 0x" << std::hex << endereço << std::dec << "\n";
            }
        }
    }
    else
    {
        if (halfword)
        {
            memoria->set16(endereço, R[Rd] & 0xFFFF);
            logFile << get_indent() << "Escrito na memória (Halfword): 0x" << std::hex << (R[Rd] & 0xFFFF)
                    << std::dec << " no endereço 0x" << std::hex << endereço << std::dec << "\n";
        }
        else
        {
            memoria->set8(endereço, R[Rd] & 0xFF);
            logFile << get_indent() << "Escrito na memória (Byte): 0x" << std::hex << (R[Rd] & 0xFF)
                    << std::dec << " no endereço 0x" << std::hex << endereço << std::dec << "\n";
        }
    }

    if (writeBack || !preIndex)
    {
        if (!loadStore || (Rd != Rn))
        {
            R[Rn] = (upDown) ? R[Rn] + offset : R[Rn] - offset;
            logFile << get_indent() << "Writeback atualizou R[" << (int)Rn << "] para 0x" << std::hex << R[Rn] << std::dec << "\n";
        }
    }

    indent_level--;
    logFile << get_indent() << "}\n";
}
void Cpu::bdt(uint32_t opcode)
{
    logFile << get_indent() << "[BDT] {\n";
    indent_level++;

    bool preIndex = (opcode >> 24) & 0x1;
    bool upDown = (opcode >> 23) & 0x1;
    bool writeBack = (opcode >> 21) & 0x1;
    bool loadStore = (opcode >> 20) & 0x1;

    uint8_t Rn = (opcode >> 16) & 0xF;
    uint16_t regList = opcode & 0xFFFF;

    logFile << get_indent() << "Operação: " << (loadStore ? "LDM" : "STM") << "\n";
    logFile << get_indent() << "Indexação: " << (preIndex ? "Pré-indexado" : "Pós-indexado") << "\n";
    logFile << get_indent() << "Direção offset: " << (upDown ? "+" : "-") << "\n";
    logFile << get_indent() << "Writeback: " << (writeBack ? "True" : "False") << "\n";
    logFile << get_indent() << "R[" << (int)Rn << "] base: 0x" << std::hex << R[Rn] << std::dec << "\n";
    logFile << get_indent() << "Lista de registradores: 0x" << std::hex << regList << std::dec << "\n";

    // Aqui você pode implementar a lógica para LDM/STM conforme necessário

    uint32_t start = 0;
    if (upDown)
    {
        if (preIndex)
        {
            start = R[Rn] + 4;
        }
        else
        {
            start = R[Rn];
        }
    }
    else
    {
        if (preIndex)
        {
            start = R[Rn] - (std::popcount(regList) * 4);
        }
        else
        {
            start = R[Rn] - (std::popcount(regList) * 4) + 4;
        }
    }

    if (writeBack)
    {
        uint32_t totalBytes = std::popcount(regList) * 4;
        if (upDown)
        {
            R[Rn] += totalBytes;
        }
        else
        {
            R[Rn] -= totalBytes;
        }
        logFile << get_indent() << "Writeback: R[" << (int)Rn << "] atualizado para 0x" << std::hex << R[Rn] << std::dec << "\n";
    }

    uint32_t addr = start;
    for (int i = 0; i < 16; i++)
    {
        if ((regList >> i) & 0x1)
        {
            if (loadStore)
            {
                OpResult status = memoria->get32(addr);
                if (status.ok)
                {
                    R[i] = status.result;
                    logFile << get_indent() << "Lido da memória: 0x" << std::hex << status.result << std::dec << " -> escrito em R[" << i << "]\n";
                }
                else
                {
                    logFile << get_indent() << "Falha ao ler o endereço: 0x" << std::hex << addr << std::dec << "\n";
                }
            }
            else
            {
                memoria->set32(addr, R[i]);
                logFile << get_indent() << "Escrito na memória: 0x" << std::hex << R[i] << std::dec << " no endereço 0x" << std::hex << addr << std::dec << "\n";
            }
            addr += 4;
        }
    }

    indent_level--;
    logFile << get_indent() << "}\n";
}
void Cpu::swp(uint32_t opcode)
{
    logFile << get_indent() << "[SWP]{\n";
    indent_level++;

    bool wordByte = (opcode >> 22) & 0x1;
    uint8_t Rn = (opcode >> 16) & 0xF;
    uint8_t Rd = (opcode >> 12) & 0xF;
    uint8_t Rm = opcode & 0xF;

    logFile << get_indent() << "Tipo de Dado: " << ((wordByte) ? "Byte(8 bits)" : "Word(32 bits)") << "\n";
    logFile << get_indent() << "Rn: " << std::hex << R[Rn] << "\n";
    logFile << get_indent() << "Rm: " << R[Rm] << std::dec << "\n";

    OpResult status;
    if (wordByte)
    {
        status = memoria->get8(R[Rn]);
        if (status.ok)
        {
            R[Rd] = status.result;
            logFile << get_indent() << "Valor Guardado em Rd: " << std::hex << R[Rd] << std::dec << "\n";
            memoria->set8(R[Rn], (R[Rm] & 0xFF));
        }
        else
        {
            logFile << get_indent() << "[ERRO] Mem::get8 Retornou {ok} false\n";
            return;
        }
    }
    else
    {
        status = memoria->get32(R[Rn]);
        if (status.ok)
        {
            R[Rd] = status.result;
            logFile << get_indent() << "Valor Guardado em Rd: " << std::hex << R[Rd] << std::dec << "\n";
            memoria->set32(R[Rn], R[Rm]);
        }
        else
        {
            logFile << get_indent() << "[ERRO] Mem::get32 Retornou {ok} false\n";
            return;
        }
    }
    indent_level--;
    logFile << get_indent() << "}";
}

void Cpu::set_r(uint8_t index, uint32_t value)
{
    if (index < 16)
    {
        R[index] = value;
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
        return R[index];
    }
    else if (index == 16)
    {
        return CPSR;
    }
    return 0;
}
void Cpu::step()
{
    // 1. FETCH
    OpResult getOpcode = memoria->get32(R[15]);
    uint32_t opcode = getOpcode.result;
    uint32_t pc_atual = R[15];
    R[15] += pc_atual + 8; // Avança o PC em 4 bytes (modo ARM 32-bit)

    logFile << get_indent() << "[STEP] {\n";
    indent_level++;
    logFile << get_indent() << "PC atual: 0x" << std::hex << pc_atual << std::dec << "\n";
    logFile << get_indent() << "Opcode lido: 0x" << std::hex << opcode << std::dec << "\n";

    // 2. CHECK CONDITION (Bits 31..28)
    if (!checar_condicao(opcode))
    {
        logFile << get_indent() << "Condição não satisfeita, instrução ignorada.\n";
        indent_level--;
        return; // Instrução ignorada se a condição der falsa
    }

    // 3. DECODE & DISPATCH (Bits 27..25)
    uint8_t grupo = (opcode >> 25) & 0x7;

    switch (grupo)
    {
    case 0b000:
    {
        // Checa se os bits 7 e 4 são ambos 1 (Assinatura das instruções extras)
        if (((opcode >> 7) & 1) && ((opcode >> 4) & 1))
        {
            uint8_t sh = (opcode >> 5) & 0x3; // Bits 6..5 (S e H)

            if (sh != 0)
            {
                hw_s(opcode); // LDRH, STRH, LDRSB, LDRSH
            }
            else
            {
                uint8_t op_mul = (opcode >> 20) & 0xFF;

                // Multiplicações de 32 bits (MUL, MLA) e 64 bits (UMULL, SMULL, etc.)
                if ((op_mul & 0xFC) == 0x00 || (op_mul & 0xF8) == 0x08)
                {
                    mul(opcode);
                }
                else if ((op_mul & 0xFB) == 0x10 && ((opcode >> 4) & 0xFF) == 0x09)
                {
                    swp(opcode);
                }
            }
        }
        else
        {
            // Checa se é instrução de status (MRS/MSR) ou salto relativo (BX/BLX)
            uint8_t op_misc = (opcode >> 20) & 0x1F;

            if ((op_misc & 0x19) == 0x10)
            {
                uint8_t bits_11_4 = (opcode >> 4) & 0xFF;

                if ((bits_11_4 & 0x0F) == 0x0)
                {
                    // Bit 21 = 0 é MRS (ler status), Bit 21 = 1 é MSR (escrever status)
                    if ((opcode >> 21) & 1)
                        msr(opcode);
                    else
                        mrs(opcode);
                }
                else if ((opcode & 0x0FFFFFF0) == 0x012FFF30)
                {
                    blx(opcode); // BX, BLX por registrador
                }
                else
                {
                    alu(opcode); // Operação de ALU via registrador com Shift
                }
            }
            else
            {
                alu(opcode); // Operação de ALU padrão
            }
        }
        break;
    }

    case 0b001:
    {
        uint8_t op_misc = (opcode >> 20) & 0x1F;

        if (op_misc == 0x12 || op_misc == 0x16)
        {
            msr(opcode); // MSR com imediato
        }
        else
        {
            alu(opcode); // ALU com Imediato rotacionado
        }
        break;
    }

    case 0b010:
    {
        sdt(opcode); // LDR, STR, LDRB, STRB (Offset Imediato)
        break;
    }

    case 0b011:
    {
        bool bit4 = (opcode >> 4) & 1;
        if (!bit4)
        {
            sdt(opcode); // LDR, STR, LDRB, STRB (Offset de Registrador)
        }
        break;
    }

    case 0b100:
    {
        bdt(opcode); // LDM / STM (Block Transfer)
        break;
    }

    case 0b101:
    {
        branch(opcode); // B e BL (Desvios com offset de 24 bits)
        break;
    }

    default:
    {
        std::cout << "[ERRO CPU] Opcode nao mapeado: 0x" << std::hex << opcode
                  << " no PC: 0x" << pc_atual << std::dec << "\n";
        break;
    }
    }

    if (R[15] == pc_atual + 8)
    {
        R[15] = pc_atual + 4;
    }

    indent_level--;
    logFile << get_indent() << "}\n";
}