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
    void debugar_branch(uint32_t opcode)
    {
        instrução_branch(opcode);
    };
    void debugar_bx(uint32_t opcode)
    {
        instrução_bx(opcode);
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
    Cpu cpu;

    std::cout << "========================================\n";
    std::cout << "      TESTANDO INSTRUÇÕES B E BL        \n";
    std::cout << "========================================\n\n";

    // ----------------------------------------------------
    // TESTE 1: Branch Simples (B +8 bytes)
    // Opcode: 0xEA000002 (B para offset de +2 instruções/+8 bytes)
    // ----------------------------------------------------
    std::cout << "--- [1] Teste Branch Simples (B) ---\n";
    cpu.set_r(15, 0x08000008); // Simula R15 apontando para PC + 8

    print_hex("R15 antes", cpu.get_r(15));

    // Executa instrução B +8 bytes
    cpu.debugar_branch(0xEA000002);

    print_hex("R15 apos (esperado 0x08000010)", cpu.get_r(15));
    std::cout << "\n";

    // ----------------------------------------------------
    // TESTE 2: Branch with Link (BL +20 bytes)
    // Opcode: 0xEB000005 (BL com bit de Link ativo)
    // ----------------------------------------------------
    std::cout << "--- [2] Teste Branch with Link (BL) ---\n";
    cpu.set_r(15, 0x08000008); // Reset R15
    cpu.set_r(14, 0x00000000); // Reset R14 (LR)

    print_hex("R15 antes", cpu.get_r(15));
    print_hex("R14 (LR) antes", cpu.get_r(14));

    // Executa BL (+20 bytes)
    cpu.debugar_branch(0xEB000005);

    print_hex("R15 apos (esperado 0x0800001C)", cpu.get_r(15));
    print_hex("R14 (LR) salvo (esperado 0x08000004)", cpu.get_r(14));
    std::cout << "\n";

    std::cout << "========================================\n";
    std::cout << "           TESTANDO BX                  \n";
    std::cout << "========================================\n\n";

    // ----------------------------------------------------
    // TESTE 3: BX com mudanca para modo THUMB (Bit 0 de R0 = 1)
    // Opcode BX R0: 0xE12FFF10
    // ----------------------------------------------------
    std::cout << "--- [3] Teste BX R0 (Mudanca para THUMB) ---\n";
    cpu.set_r(0, 0x08000101); // Endereco impar (bit 0 = 1 -> Modo THUMB)

    print_hex("R0 (Alvo)", cpu.get_r(0));
    print_hex("CPSR antes", cpu.lerCPSR());

    cpu.debugar_bx(0xE12FFF10);

    print_hex("R15 apos (esperado 0x08000100 - alinhado 2B)", cpu.get_r(15));
    print_hex("CPSR apos (esperado Bit 5 = 1 / Thumb ativo)", cpu.lerCPSR());
    std::cout << "Modo THUMB Ativo? " << ((cpu.lerCPSR() & (1 << 5)) ? "SIM" : "NAO") << "\n\n";

    // ----------------------------------------------------
    // TESTE 4: BX com retorno para modo ARM (Bit 0 de R1 = 0)
    // Opcode BX R1: 0xE12FFF11
    // ----------------------------------------------------
    std::cout << "--- [4] Teste BX R1 (Retorno para ARM) ---\n";
    cpu.set_r(1, 0x08000200); // Endereco par (bit 0 = 0 -> Modo ARM)

    print_hex("R1 (Alvo)", cpu.get_r(1));

    cpu.debugar_bx(0xE12FFF11);

    print_hex("R15 apos (esperado 0x08000200 - alinhado 4B)", cpu.get_r(15));
    print_hex("CPSR apos (esperado Bit 5 = 0 / ARM ativo)", cpu.lerCPSR());
    std::cout << "Modo THUMB Ativo? " << ((cpu.lerCPSR() & (1 << 5)) ? "SIM" : "NAO") << "\n\n";

    return 0;
}