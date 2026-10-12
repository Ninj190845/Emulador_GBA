#include <cstring>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// utilidades de bit
inline void set_bit(uint32_t &reg, uint8_t bit, bool value)
{
    if (value)
    {
        reg |= (1U << bit); // Seta o bit especificado para 1
    }
    else
    {
        reg &= ~(1U << bit); // Limpa o bit especificado para 0
    }
}
inline bool get_bit(uint32_t &reg, uint8_t bit)
{
    return (reg >> bit) & 0x1;
}

class Cpu
{
private:
    // registradores
    uint32_t R[16] = {0};
    uint32_t CPSR = 0;

    uint32_t R8_fiq[7] = {0};
    uint32_t SPSR_fiq = 0;

    uint32_t R13_svc[2] = {0};
    uint32_t SPSR_svc = 0;

    uint32_t R13_abt[2] = {0};
    uint32_t SPSR_abt = 0;

    uint32_t R13_irq[2] = {0};
    uint32_t SPSR_irq = 0;

    uint32_t R13_und[2] = {0};
    uint32_t SPSR_und = 0;

    // Funções de apoio
    uint32_t ror32(uint32_t value, uint8_t count)
    {
        count = count % 32;
        if (count == 0)
        {
            return value;
        }
        return (value >> count) | (value << (32 - count));
    };
    uint32_t processar_op2(uint32_t opcode, uint8_t Rm, uint8_t Rs, bool &flagC)
    {
        uint32_t val = R[Rm]; // Boa prática: inicializar com 0

        uint8_t shiftType = (opcode >> 5) & 0b11;
        uint8_t shiftCount;
        bool shiftImediato = !((opcode >> 4) & 0x1);

        if (shiftImediato)
        {
            std::cout << "Shift Imediato\n";
            shiftCount = (opcode >> 7) & 0x1F;
        }
        else
        {
            std::cout << "Shift por Registrador\n";
            shiftCount = R[Rs] & 0xFF;
            std::cout << (R[Rs] & 0xFF) << "\n";
        }

        std::cout << "Shift Type: ";
        switch (shiftType)
        {
        case 0:
            std::cout << "LSL\n";
            if (shiftCount >= 32)
            {
                flagC = (shiftCount == 32) ? get_bit(R[Rm], 0) : 0;
                val = 0;
            }
            else
            {
                flagC = get_bit(R[Rm], (32 - shiftCount));
                val = R[Rm] << shiftCount;
            }
            break;

        case 1:
            std::cout << "LSR\n";
            if (shiftCount == 0)
            {
                std::cout << "Shiftcount = 0\n";
                if (shiftImediato)
                {
                    val = 0;
                    flagC = get_bit(R[Rm], 31);
                }
            }
            else if (shiftCount >= 32)
            {
                std::cout << "shiftcount >= 32\n";
                val = 0;
                flagC = (shiftCount == 32) ? get_bit(R[Rm], 31) : 0;
            }
            else
            {
                std::cout << static_cast<uint>(shiftCount) << "\n";
                val = R[Rm] >> shiftCount;
                flagC = get_bit(R[Rm], (shiftCount - 1));
            }
            break;

        case 2:
            std::cout << "ASR\n";
            if (shiftCount == 0)
            {
                if (shiftImediato)
                {
                    val = get_bit(R[Rm], 31) ? 0xFFFFFFFF : 0;
                    flagC = get_bit(R[Rm], 31);
                }
            }
            else if (shiftCount >= 32)
            {
                val = get_bit(R[Rm], 31) ? 0xFFFFFFFF : 0;
                flagC = get_bit(R[Rm], 31);
            }
            else
            {
                val = static_cast<uint32_t>(static_cast<int32_t>(R[Rm]) >> shiftCount);
                flagC = get_bit(R[Rm], (shiftCount - 1));
            }
            break;

        case 3:
            std::cout << "ROR / RRX\n";
            if (shiftCount == 0)
            {
                if (shiftImediato)
                {
                    val = (get_bit(CPSR, 29) << 31) | (R[Rm] >> 1);
                    flagC = get_bit(R[Rm], 0);
                }
            }
            else if ((shiftCount % 32) == 0) // Otimizado: Só testa se é múltiplo de 32 (32, 64, 96...)
            {
                flagC = get_bit(R[Rm], 31);
            }
            else // <--- CORRIGIDO AQUI! Cobre qualquer shift de 1 a 31 e o resto das divisões.
            {
                val = ror32(R[Rm], shiftCount);
                flagC = get_bit(R[Rm], ((shiftCount % 32) - 1));
            }
            break;

        default:
            break;
        }

        // NUNCA ESQUECER DO RETURN!
        return val;
    }

    // funções de instruções
    void instrução_alu(uint32_t opcode)
    {
        std::cout << "\n[ALU] === INICIANDO INSTRUCAO DA ULA ===\n";
        std::cout << "[ALU] Opcode: 0x" << std::hex << opcode << std::dec << "\n";

        // index Registradores
        uint8_t Rn = (opcode >> 16) & 0xF;
        uint8_t Rd = (opcode >> 12) & 0xF;

        // bits de flag
        bool updateFlag = (opcode >> 20) & 0x1;
        bool imediatoFlag = (opcode >> 25) & 0x1;

        // flags atuais (para log e cálculos)
        bool flagN;
        bool flagZ;
        bool flagC = get_bit(CPSR, 29);
        bool flagV = get_bit(CPSR, 28);
        bool Carry = flagC; // Guarda o Carry inicial para o ADC/SBC

        std::cout << "[ALU] R[" << +Rn << "] (Operando 1): 0x" << std::hex << R[Rn] << std::dec << "\n";
        std::cout << "[ALU] Update Flags (Bit S): " << (updateFlag ? "SIM" : "NAO") << "\n";

        // op2
        uint32_t val;
        uint8_t alu_op = (opcode >> 21) & 0xF;

        if (imediatoFlag)
        {
            // calculo com valor imediato rotacionando
            std::cout << "[ALU] Operando 2: Imediato\n";
            uint8_t rorCount = ((opcode >> 8) & 0xF) * 2;
            uint32_t nn = opcode & 0xFF;
            val = ror32(nn, rorCount);
            if (rorCount != 0)
            {
                flagC = get_bit(val, 31);
            }
        }
        else
        {
            std::cout << "[ALU] Operando 2: Registrador (Shift)\n";
            uint8_t Rs = (opcode >> 8) & 0xF;
            uint8_t Rm = opcode & 0xF;
            val = processar_op2(opcode, Rm, Rs, flagC);
        }

        std::cout << "[ALU] Valor do Operando 2 (val): 0x" << std::hex << val << std::dec << "\n";

        uint32_t resultado;
        bool escrever_rd = true;

        std::cout << "[ALU] Executando operacao: 0x" << std::hex << +alu_op << std::dec << "...\n";

        switch (alu_op)
        {
        case 0x0: // AND
            resultado = R[Rn] & val;
            break;

        case 0x1: // EOR
            resultado = R[Rn] ^ val;
            break;

        case 0x2: // SUB
            resultado = R[Rn] - val;
            flagC = (R[Rn] >= val);
            flagV = ((R[Rn] ^ val) & (R[Rn] ^ resultado)) >> 31;
            break;

        case 0x3: // RSB
            resultado = val - R[Rn];
            flagC = (val >= R[Rn]);                            // CORRIGIDO: Invertido em relacao ao SUB
            flagV = ((R[Rn] ^ val) & (val ^ resultado)) >> 31; // CORRIGIDO
            break;

        case 0x4: // ADD
            resultado = R[Rn] + val;
            flagC = ((uint64_t)R[Rn] + (uint64_t)val) > UINT32_MAX;
            flagV = ((R[Rn] ^ resultado) & (val ^ resultado)) >> 31;
            break;

        case 0x5: // ADC
            resultado = R[Rn] + val + Carry;
            flagC = ((uint64_t)R[Rn] + (uint64_t)val + Carry) > UINT32_MAX; // CORRIGIDO: adc soma o Carry
            flagV = ((R[Rn] ^ resultado) & (val ^ resultado)) >> 31;
            break;

        case 0x6: // SBC
            resultado = R[Rn] - val - !Carry;
            flagC = ((uint64_t)R[Rn] - (uint64_t)val - !Carry) <= UINT32_MAX; // Forma segura sem underflow
            flagV = ((R[Rn] ^ val) & (R[Rn] ^ resultado)) >> 31;
            break;

        case 0x7: // RSC
            resultado = val - R[Rn] - !Carry;
            flagC = ((uint64_t)val - (uint64_t)R[Rn] - !Carry) <= UINT32_MAX;
            flagV = ((R[Rn] ^ val) & (val ^ resultado)) >> 31;
            break;

        case 0x8: // TST
            resultado = R[Rn] & val;
            escrever_rd = false;
            break;

        case 0x9: // TEQ
            resultado = R[Rn] ^ val;
            escrever_rd = false;
            break;

        case 0xA: // CMP
            resultado = R[Rn] - val;
            escrever_rd = false;
            flagC = (R[Rn] >= val);
            flagV = ((R[Rn] ^ val) & (R[Rn] ^ resultado)) >> 31;
            break;

        case 0xB: // CMN
            resultado = R[Rn] + val;
            flagC = ((uint64_t)R[Rn] + (uint64_t)val) > UINT32_MAX;
            flagV = ((R[Rn] ^ resultado) & (val ^ resultado)) >> 31;
            escrever_rd = false;
            break;

        case 0xC: // ORR
            resultado = R[Rn] | val;
            break;

        case 0xD: // MOV
            resultado = val;
            break;

        case 0xE: // BIC
            resultado = R[Rn] & (~val);
            break;

        case 0xF: // MVN
            resultado = ~val;
            break;

        default:
            std::cout << "[ALU] AVISO: Operacao desconhecida!\n";
            break;
        }

        std::cout << "[ALU] Resultado bruto: 0x" << std::hex << resultado << std::dec << "\n";

        if (updateFlag)
        {
            flagN = resultado >> 31;
            flagZ = resultado == 0;
            set_bit(CPSR, 31, flagN);
            set_bit(CPSR, 30, flagZ);
            set_bit(CPSR, 29, flagC);
            set_bit(CPSR, 28, flagV);

            std::cout << "[ALU] Atualizando Flags (NZCV): ";
            std::cout << "N=" << flagN << " Z=" << flagZ << " C=" << flagC << " V=" << flagV << "\n";
        }
        else
        {
            std::cout << "[ALU] Flags INTACTAS (S=0).\n";
        }

        if (escrever_rd)
        {
            R[Rd] = resultado;
            std::cout << "[ALU] Gravou resultado em R[" << +Rd << "]\n";
        }
        else
        {
            std::cout << "[ALU] Resultado DESCARTADO (Instrucao de Teste)\n";
        }
        std::cout << "[ALU] =================================\n\n";
    }
    void instrução_mul(uint32_t opcode)
    {
        bool updateFlag = (opcode >> 20) & 0x1;

        // Registradores
        uint8_t Rd = (opcode >> 16) & 0xF;
        uint8_t Rn = (opcode >> 12) & 0xF;
        uint8_t Rs = (opcode >> 8) & 0xF;
        uint8_t Rm = opcode & 0xF;
        uint8_t RdHigh = Rd;
        uint8_t RdLow = Rn;

        // dados da multiplicação
        uint8_t mulType = (opcode >> 4) & 0xF;
        bool longFlag = (opcode >> 23) & 0x1;
        bool signedFlag = ((opcode >> 22) & 0x1);
        bool aculmulativo = (opcode >> 21) & 0x1;

        // flags
        bool flag_N = 0;
        bool flag_Z = 0;

        uint32_t resultado = 0;
        uint32_t resultadoLow = 0;
        if (longFlag)
        {
            if (RdHigh == RdLow || RdHigh == Rm || RdLow == Rm || RdHigh == 15 || RdLow == 15 || Rm == 15 || Rs == 15)
            {
                std::cout << "[MUL] [ERRO] Uso invalido dos registradores para mutiplicação!\n";
                return;
            }
            // multiplicaçlão de 64 bits
            uint64_t buffer_result = 0;
            if (aculmulativo)
            {
                uint64_t Rd_64 = ((uint64_t)R[RdHigh] << 32) | R[RdLow];

                if (signedFlag)
                {
                    buffer_result = ((int64_t)(int32_t)R[Rm] * (int64_t)(int32_t)R[Rs]) + (int64_t)Rd_64;
                }
                else
                {
                    buffer_result = ((uint64_t)R[Rm] * (uint64_t)R[Rs]) + Rd_64;
                }
            }
            else
            {
                if (signedFlag)
                {
                    buffer_result = (int64_t)((int32_t)R[Rm] * (int64_t)(int32_t)R[Rs]);
                }
                else
                {
                    buffer_result = ((uint64_t)R[Rm] * (uint64_t)R[Rs]);
                }
            }
            resultado = (buffer_result >> 32) & UINT32_MAX;
            resultadoLow = (buffer_result)&UINT32_MAX;
            R[RdLow] = resultadoLow;
        }
        else
        {
            if (Rd == Rm || Rd == 15 || Rm == 15 || Rs == 15 || Rn == 15)
            {
                std::cout << "[MUL] [ERRO] Uso invalido dos registradores para mutiplicação!\n";
                return;
            }
            if (aculmulativo)
            {
                resultado = R[Rm] * R[Rs] + R[Rn];
            }
            else
            {
                resultado = R[Rm] * R[Rs];
            }
        }

        if (updateFlag)
        {
            set_bit(CPSR, 31, get_bit(resultado, 31));
            set_bit(CPSR, 30, (resultado == 0 && resultadoLow == 0));
        }

        R[Rd] = resultado;
    };
    void instrução_branch(uint32_t opcode) {};

public:
    Cpu()
    {
        return;
    };
    ~Cpu() = default;
    void set_r(uint8_t index, uint32_t value)
    {
        if (index < 16)
        {
            R[index] = value;
        }
        else
        {
            std::cerr << "Erro: Indice de registrador invalido (" << static_cast<int>(index) << ")\n";
        }
    }
    uint32_t get_r(uint8_t index)
    {
        if (index < 16)
        {
            return R[index];
        }
        else
        {
            std::cout << "Erro d eindex no lerR\n";
            return 0;
        }
    }
    uint32_t lerCPSR()
    {
        return CPSR;
    };

    void debugar_alu(uint32_t opcode)
    {
        instrução_alu(opcode);
    };
    void debugar_mul(uint32_t opcode)
    {
        instrução_mul(opcode);
    };
};

// codigo feito pelo gemini para debug

#include <iostream>
#include <iomanip>
#include <cstdint>

// Funções auxiliares para gerar os opcodes de teste dinamicamente
uint32_t make_MUL(uint8_t Rd, uint8_t Rm, uint8_t Rs, bool S)
{
    return 0xE0000090 | (S << 20) | (Rd << 16) | (Rs << 8) | Rm;
}

uint32_t make_MLA(uint8_t Rd, uint8_t Rm, uint8_t Rs, uint8_t Rn, bool S)
{
    return 0xE0200090 | (S << 20) | (Rd << 16) | (Rn << 12) | (Rs << 8) | Rm;
}

uint32_t make_MULL(bool U, bool A, bool S, uint8_t RdHi, uint8_t RdLo, uint8_t Rm, uint8_t Rs)
{
    // U=0 (Unsigned) | U=1 (Signed)
    // A=0 (Multiply) | A=1 (Accumulate)
    return 0xE0800090 | (U << 22) | (A << 21) | (S << 20) | (RdHi << 16) | (RdLo << 12) | (Rs << 8) | Rm;
}

void print_resultado(uint32_t cpsr, uint32_t rd, uint32_t rd2 = 0, bool is_long = false)
{
    bool n = (cpsr >> 31) & 1;
    bool z = (cpsr >> 30) & 1;

    if (is_long)
    {
        std::cout << "  Resultado Hi: 0x" << std::hex << std::uppercase << rd
                  << " | Lo: 0x" << rd2 << std::dec << "\n";
    }
    else
    {
        std::cout << "  Resultado Rd: " << (int32_t)rd << " (0x" << std::hex << std::uppercase << rd << std::dec << ")\n";
    }
    std::cout << "  Flags -> N: " << n << " | Z: " << z << "\n\n";
}

int main()
{
    Cpu cpu;

    std::cout << "========================================\n";
    std::cout << "      BATERIA DE TESTES DA MUL ARM      \n";
    std::cout << "========================================\n\n";

    // ---------------------------------------------------------
    std::cout << "[TESTE 1] MUL Normal (Positivo x Positivo) | Sem Flags\n";
    // 5 * 4 = 20
    cpu.set_r(2, 5);
    cpu.set_r(3, 4);
    cpu.debugar_mul(make_MUL(1, 2, 3, false)); // R1 = R2 * R3
    print_resultado(cpu.lerCPSR(), cpu.get_r(1));

    // ---------------------------------------------------------
    std::cout << "[TESTE 2] MULS Normal (Positivo x Negativo) | Com Flags\n";
    // 5 * -4 = -20. (Z = 0, N = 1)
    cpu.set_r(2, 5);
    cpu.set_r(3, (uint32_t)-4);
    cpu.debugar_mul(make_MUL(1, 2, 3, true)); // R1 = R2 * R3
    print_resultado(cpu.lerCPSR(), cpu.get_r(1));

    // ---------------------------------------------------------
    std::cout << "[TESTE 3] MLAS (Acumula Negativo para dar Zero) | Com Flags\n";
    // (-5 * 4) + 20 = 0. (Z = 1, N = 0)
    cpu.set_r(2, (uint32_t)-5);
    cpu.set_r(3, 4);
    cpu.set_r(4, 20);
    cpu.debugar_mul(make_MLA(1, 2, 3, 4, true)); // R1 = (R2 * R3) + R4
    print_resultado(cpu.lerCPSR(), cpu.get_r(1));

    // ---------------------------------------------------------
    std::cout << "[TESTE 4] UMULL (Longo Sem Sinal) | Sem Flags\n";
    // 0xFFFFFFFF (4.294.967.295) * 2 = 8.589.934.590 (0x00000001FFFFFFFE)
    cpu.set_r(2, 0xFFFFFFFF);
    cpu.set_r(3, 2);
    // U=0 (unsigned), A=0 (no accum), S=0 (no flags). RdHi=1, RdLo=0
    cpu.debugar_mul(make_MULL(false, false, false, 1, 0, 2, 3));
    print_resultado(cpu.lerCPSR(), cpu.get_r(1), cpu.get_r(0), true);

    // ---------------------------------------------------------
    std::cout << "[TESTE 5] SMULLS (Longo Com Sinal) | Com Flags\n";
    // -1 (0xFFFFFFFF) * 2 = -2 (0xFFFFFFFFFFFFFFFE)
    cpu.set_r(2, 0xFFFFFFFF);
    cpu.set_r(3, 2);
    // U=1 (signed), A=0, S=1. RdHi=1, RdLo=0
    cpu.debugar_mul(make_MULL(true, false, true, 1, 0, 2, 3));
    print_resultado(cpu.lerCPSR(), cpu.get_r(1), cpu.get_r(0), true);

    // ---------------------------------------------------------
    std::cout << "[TESTE 6] SMLALS (Longo Com Sinal + Acumulativo resultando em Zero) | Com Flags\n";
    // Acumulador (RdHi:RdLo) = 2.
    // Multiplicação: -1 * 2 = -2.
    // Total: -2 + 2 = 0. (Z = 1, N = 0)
    cpu.set_r(1, 0);          // RdHi
    cpu.set_r(0, 2);          // RdLo
    cpu.set_r(2, 0xFFFFFFFF); // -1
    cpu.set_r(3, 2);
    // U=1, A=1, S=1.
    cpu.debugar_mul(make_MULL(true, true, true, 1, 0, 2, 3));
    print_resultado(cpu.lerCPSR(), cpu.get_r(1), cpu.get_r(0), true);

    return 0;
}