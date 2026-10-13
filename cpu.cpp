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
    uint32_t SPSR_swi = 0;

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
    }
    void instrução_branch(uint32_t opcode)
    {
        bool linkBranch = (opcode >> 24) & 0x1;

        int32_t offset = (static_cast<int32_t>(opcode & 0x00FFFFFF) << 8) >> 6;

        if (linkBranch)
        {
            R[14] = R[15] - 4;
        }
        R[15] = static_cast<uint32_t>(static_cast<int32_t>(R[15]) + offset);
    }
    void instrução_bx(uint32_t opcode)
    {
        uint8_t Rm = opcode & 0xF;
        uint32_t address = R[Rm];

        bool modoThumb = address & 0x1;
        set_bit(CPSR, 5, modoThumb);

        if (modoThumb)
        {
            R[15] = address & (~(uint32_t)1);
        }
        else
        {
            R[15] = address & (~(uint32_t)3);
        }
    }
    void instrução_msr(uint32_t opcode)
    {
        bool imediato = (opcode >> 25) & 0x1;

        bool spsrFlag = (opcode >> 22) & 0x1;
        uint32_t *endereçoPSR = &CPSR;
        uint32_t programState = 0;

        uint32_t fields = (opcode >> 16) & 0xF;
        uint32_t mask = 0;
        if (get_bit(fields, 0) & 0x1)
        {
            // Campo 'c'
            mask |= 0x000000FF;
        }
        if (get_bit(fields, 1) & 0x1)
        {
            // Campo 'x'
            mask |= 0x0000FF00;
        }
        if (get_bit(fields, 2) & 0x1)
        {
            // Campo 's'
            mask |= 0x00FF0000;
        }
        if (get_bit(fields, 3) & 0x1)
        {
            // Campo 'f'
            mask |= 0xFF000000;
        }
        uint8_t modeBits = CPSR & 0x1F;

        if (modeBits == 0b10000)
        {
            mask &= 0xFF000000;
        }

        if (spsrFlag)
        {
            switch (modeBits)
            {
            case 0b10000: // Modo User
            case 0b11111: // Modo System
            case 0b10001: // Modo FIQ
                return;   // Não possuem SPSR acessível / ignorado no GBA

            case 0b10010: // Modo IRQ
                endereçoPSR = &SPSR_irq;
                break;

            case 0b10011: // Modo SWI / Supervisor
                endereçoPSR = &SPSR_swi;
                break;

            case 0b10111: // Modo Abort
                endereçoPSR = &SPSR_abt;
                break;

            case 0b11011: // Modo Undefined
                endereçoPSR = &SPSR_und;
                break;

            default:
                std::cout << "[DEBUG] Caiu no default! modeBits era: 0x" << std::hex << (int)modeBits << "\n";
                return;
            }
        }

        if (imediato)
        {
            uint32_t nn = opcode & 0xFF;
            uint8_t rorCount = ((opcode >> 8) & 0xF) * 2;

            programState = ror32(nn, rorCount);
        }
        else
        {
            uint8_t Rm = opcode & 0xF;
            programState = R[Rm];
        }

        std::cout << "[DEBUG] programState: 0x" << std::hex << programState << " | mask: 0x" << mask << "\n";

        *endereçoPSR = (*endereçoPSR & ~mask) | (programState & mask);
    }
    void instrução_mrs(uint32_t opcode)
    {
        uint8_t Rd = (opcode >> 12) & 0xF;

        bool spsrFlag = (opcode >> 22) & 0x1;
        uint32_t *endereçoPSR = &CPSR;

        if (spsrFlag)
        {
            uint8_t modeBits = CPSR & 0x1F;
            switch (modeBits)
            {
            case 0b10000: // Modo User
            case 0b11111: // Modo System
            case 0b10001: // Modo FIQ
                return;   // Não possuem SPSR acessível / ignorado no GBA

            case 0b10010: // Modo IRQ
                endereçoPSR = &SPSR_irq;
                break;

            case 0b10011: // Modo SWI / Supervisor
                endereçoPSR = &SPSR_swi;
                break;

            case 0b10111: // Modo Abort
                endereçoPSR = &SPSR_abt;
                break;

            case 0b11011: // Modo Undefined
                endereçoPSR = &SPSR_und;
                break;

            default:
                return;
            }
        }

        R[Rd] = *endereçoPSR;
    }

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
    uint32_t getCPSR()
    {
        return CPSR;
    }
    void setCPSR(uint32_t value)
    {
        CPSR = value;
    }
    uint32_t getSPSR_irq()
    {
        return SPSR_irq;
    };

    void debugar_alu(uint32_t opcode)
    {
        instrução_alu(opcode);
    }
    void debugar_mul(uint32_t opcode)
    {
        instrução_mul(opcode);
    }
    void debugar_branch(uint32_t opcode)
    {
        instrução_branch(opcode);
    }
    void debugar_bx(uint32_t opcode)
    {
        instrução_bx(opcode);
    }
    void debugar_mrs(uint32_t opcode)
    {
        instrução_mrs(opcode);
    }
    void debugar_msr(uint32_t opcode)
    {
        instrução_msr(opcode);
    }
};

// codigo feito pelo gemini para debug
#include <iostream>
#include <iomanip>
#include <cstdint>

// Função auxiliar para exibir valores em hexadecimal no terminal
void print_hex(const std::string &label, uint32_t val)
{
    std::cout << label << ": 0x"
              << std::uppercase << std::setfill('0') << std::setw(8)
              << std::hex << val << std::dec << "\n";
}

int main()
{
#include <iostream>
#include <iomanip>

    // Assumindo a instância da sua CPU
    // cpu.setCPSR(...);
    // cpu.instrução_mrs(...); etc.

    Cpu cpu;

    std::cout << "========================================\n";
    std::cout << "         TESTANDO MRS E MSR             \n";
    std::cout << "========================================\n\n";

    // ----------------------------------------------------
    // [1] Teste MRS: Ler CPSR para R0
    // ----------------------------------------------------
    cpu.setCPSR(0x60000010);          // Define CPSR com flags e modo User (0x10)
    uint32_t opcode_mrs = 0xE10F0000; // MRS R0, CPSR

    cpu.debugar_mrs(opcode_mrs);

    std::cout << "--- [1] Teste MRS (CPSR -> R0) ---\n";
    std::cout << "CPSR configurado : 0x" << std::hex << 0x60000010 << "\n";
    std::cout << "R0 apos MRS      : 0x" << std::hex << cpu.get_r(0) << "\n\n";

    // ----------------------------------------------------
    // [2] Teste MSR: Alterar flags do CPSR via Registrador (R1 -> CPSR_f)
    // ----------------------------------------------------
    cpu.setCPSR(0x00000013);              // Modo Supervisor (SVC / 0x13)
    cpu.set_r(1, 0xF0000000);             // R1 com flags N, Z, C, V ativas
    uint32_t opcode_msr_reg = 0xE128F001; // MSR CPSR_f, R1

    cpu.debugar_msr(opcode_msr_reg);

    std::cout << "--- [2] Teste MSR Reg (R1 -> CPSR_f) ---\n";
    std::cout << "CPSR esperado    : 0xF0000013\n";
    std::cout << "CPSR apos MSR    : 0x" << std::hex << cpu.getCPSR() << "\n\n";

    // ----------------------------------------------------
    // [3] Teste MSR: Alterar SPSR_irq via Imediato
    // ----------------------------------------------------
    cpu.setCPSR(0x00000012);              // Entra no modo IRQ (0x12)
    uint32_t opcode_msr_imm = 0xE368F480; // MSR SPSR_f, #0x80000000 (imm 0x80, rot 0)

    cpu.debugar_msr(opcode_msr_imm);

    std::cout << "--- [3] Teste MSR Imm (Imediato -> SPSR_irq) ---\n";
    std::cout << "SPSR_irq esperado: 0x80000000\n";
    std::cout << "SPSR_irq apos MSR: 0x" << std::hex << cpu.getSPSR_irq() << "\n";
}