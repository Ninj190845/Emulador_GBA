#include "cpu.hpp"

#include <bit>
#include <sstream>
#include <iomanip>

Cpu::Cpu(Mem *mem) : memoria(mem)
{
    CPSR.val = 0xD3; // Inicia em modo Supervisor (SVC), IRQ/FIQ desabilitados e modo ARM
    update_reg_map();
}
void Cpu::reset()
{
    // 1. Configura o CPSR para o estado de Power-On da ARM:
    // Modo Supervisor (0x13), Bits I=1 (IRQ off), F=1 (FIQ off), T=0 (Modo ARM)
    CPSR.val = 0xD3; // 0b11010011 em binario

    // 2. Atualiza a tabela de ponteiros de registradores para o modo Supervisor
    update_reg_map();

    // 3. Aponta o PC para o inicio da BIOS
    set_reg(15, 0x00000000);

    // Opcional: Zerar registradores gerais se necessário
    for (int i = 0; i < 13; ++i)
    {
        set_reg(i, 0);
    }
    set_reg(14, 0); // LR inicia zerado
}

// auxiliares de registradores
void Cpu::set_reg(uint8_t i, uint32_t v)
{
    if (i < 16)
    {
        *regMap[i] = v;
        if (i == 15)
        {
            pc_alterado = true;
        }
    }
    else
    {
        LOG_MSG("[CPU][set_reg()]: Tentativa de escrever um index não existente");
    }
}
uint32_t Cpu::get_reg(uint8_t i)
{
    if (i == 15)
    {
        return *regMap[i] + ((CPSR.thumb) ? 4 : 8);
    }
    else if (i < 16)
    {
        return *regMap[i];
    }
    else
    {
        LOG_MSG("[CPU][get_reg()]: Tentativa de acessar um index não existente");
    }
    return 0;
}

// auxiliares de sistema
void Cpu::update_reg_map()
{
    // Registradores R0 a R7 são comuns a todos os modos
    for (int i = 0; i <= 7; i++)
    {
        regMap[i] = &R[i];
    }

    uint8_t mode = CPSR.mode;

    // Trata o modo FIQ (possui R8 a R14 próprios)
    if (mode == static_cast<uint8_t>(CpuMode::FIQ))
    {
        for (int i = 8; i <= 14; i++)
        {
            regMap[i] = &R8_fiq[i - 8];
        }
        SPSRatual = &SPSR_fiq;
    }
    else
    {
        // Registradores R8 a R12 comuns aos modos não-FIQ
        for (int i = 8; i <= 12; i++)
        {
            regMap[i] = &R[i];
        }

        // Mapeia R13 (SP) e R14 (LR) conforme o modo atual
        switch (static_cast<CpuMode>(mode))
        {
        case CpuMode::IRQ:
            regMap[13] = &R13_irq[0];
            regMap[14] = &R13_irq[1];
            SPSRatual = &SPSR_irq;
            break;
        case CpuMode::Supervisor:
            regMap[13] = &R13_svc[0];
            regMap[14] = &R13_svc[1];
            SPSRatual = &SPSR_svc;
            break;
        case CpuMode::Abort:
            regMap[13] = &R13_abt[0];
            regMap[14] = &R13_abt[1];
            SPSRatual = &SPSR_abt;
            break;
        case CpuMode::Undefined:
            regMap[13] = &R13_und[0];
            regMap[14] = &R13_und[1];
            SPSRatual = &SPSR_und;
            break;
        case CpuMode::User:
        case CpuMode::System:
        default:
            regMap[13] = &R[13];
            regMap[14] = &R[14];
            SPSRatual = nullptr; // Modos User e System não possuem SPSR
            break;
        }
    }

    // R15 (PC) é sempre o mesmo
    regMap[15] = &R[15];
}
OpResult Cpu::shift_barrier(uint32_t shiftType, uint32_t value, uint8_t count, bool origemImediato, bool flagC)
{
    // Verifica se o shiftType recebido excede 2 bits (indicativo de decode incorreto)
    if (shiftType > 0b11)
    {
        LOG_MSG("[CPU][shift_barrier]: Tipo de shift invalido (" + std::to_string(shiftType) + ") para o valor 0x" + std::to_string(value));
    }

    uint32_t result = 0;
    shiftType &= 0b11; // mascarando shifttype para apenas 2 bits

    switch (shiftType)
    {
    case 0b00:
    {
        if (count == 0)
        {
            result = value;
        }
        else if (count == 32)
        {
            flagC = get_bit(value, 0);
        }
        else if (count > 32)
        {
            flagC = 0;
            break;
        }
        else
        {
            result = value << count;
            flagC = get_bit(value, 32 - count);
        }
        break;
    }
    case 0b01:
    {
        if (count == 0)
        {
            if (origemImediato)
            {
                flagC = get_bit(value, 31);
            }
            else
            {
                result = value;
            }
        }
        else if (count == 32)
        {
            flagC = get_bit(value, 31);
        }
        else if (count > 32)
        {
            flagC = 0;
        }
        else
        {
            result = value >> count;
            flagC = get_bit(value, count - 1);
        }

        break;
    }
    case 0b10:
    {
        if (count == 0)
        {
            if (origemImediato)
            {
                if (get_bit(value, 31))
                {
                    result = UINT32_MAX;
                }
                else
                {
                    result = 0x00000000;
                }
                flagC = get_bit(value, 31);
            }
            else
            {
                result = value;
            }
        }
        else if (count >= 32)
        {
            flagC = get_bit(value, 31);
            if (get_bit(value, 31))
            {
                result = UINT32_MAX;
            }
            else
            {
                result = 0x00000000;
            }
        }
        else
        {
            result = static_cast<int32_t>(value) >> count;
            flagC = get_bit(value, count - 1);
        }

        break;
    }
    case 0b11:
    {
        if (count == 0)
        {
            if (origemImediato)
            {
                bool oldC = flagC;
                flagC = get_bit(value, 0);

                result = value >> 1;
                set_bit(result, 31, oldC); // RRX usa o Carry antigo para o bit 31
            }
            else
            {
                result = value;
            }
        }
        else
        {
            result = std::rotr(value, count);
            flagC = get_bit(result, 31);
        }

        break;
    }
    default:
        LOG_MSG("[CPU][shift_barrier]: Caso irreconhecido no switch de shiftType");
        break;
    }

    OpResult returning(result, flagC, StCode::SUCESS);
    return returning;
}
void Cpu::interrupt(CpuMode mode, uint32_t jumpAddr)
{
    uint32_t oldCPSR = CPSR.val;
    bool estavaEmThumb = CPSR.thumb;

    // 1. Cálculo do LR de acordo com o tipo de exceção
    uint32_t PCretorno = 0;

    switch (mode)
    {
    case CpuMode::Supervisor: // SWI
    case CpuMode::Undefined:  // Instrução Inválida
        PCretorno = get_reg(15) + (estavaEmThumb ? 2 : 4);
        break;

    case CpuMode::IRQ: // Interrupção de Hardware
    case CpuMode::FIQ: // Fast Interrupt
        PCretorno = get_reg(15) + 4;
        break;

    default:
        PCretorno = get_reg(15) + 4;
        break;
    }

    // 2. Transição do CPSR
    CPSR.mode = static_cast<uint8_t>(mode);
    CPSR.thumb = 0;       // Força modo ARM (T = 0)
    CPSR.irq_disable = 1; // Desativa IRQs normais (Bit I = 1)

    if (mode == CpuMode::FIQ)
    {
        CPSR.fiq_disable = 1; // Desativa FIQs (Bit F = 1)
    }

    // 3. Atualiza ponteiros de registradores bancados
    update_reg_map();

    // 4. Salva o contexto antigo no SPSR do NOVO modo
    if (SPSRatual != nullptr)
    {
        SPSRatual->val = oldCPSR;
    }
    else
    {
        std::ostringstream ss;
        ss << "[CPU][interrupt]: ERRO - Tentativa de salvar SPSR com SPSRatual NULO ao mudar para o modo "
           << static_cast<int>(mode);
        LOG_MSG(ss.str());
    }

    // 5. Configura o retorno e o salto do vetor
    set_reg(14, PCretorno);
    set_reg(15, jumpAddr);

    pc_alterado = true; // Notifica a CPU sobre o salto da interrupção
}
bool Cpu::checar_condicao(uint32_t opcode)
{
    uint8_t cond = opcode >> 28;

    // AL (0xE - Always): executa sempre sem perder tempo checando flags
    if (cond == 0xE)
        return true;

    switch (cond)
    {
    case 0x0:
        return CPSR.z; // EQ: Equal (Z == 1)
    case 0x1:
        return !CPSR.z; // NE: Not Equal (Z == 0)
    case 0x2:
        return CPSR.c; // CS / HS: Carry Set (C == 1)
    case 0x3:
        return !CPSR.c; // CC / LO: Carry Clear (C == 0)
    case 0x4:
        return CPSR.n; // MI: Minus (N == 1)
    case 0x5:
        return !CPSR.n; // PL: Plus / Positive (N == 0)
    case 0x6:
        return CPSR.v; // VS: Overflow (V == 1)
    case 0x7:
        return !CPSR.v; // VC: No Overflow (V == 0)
    case 0x8:
        return CPSR.c && !CPSR.z; // HI: Unsigned Higher (C == 1 && Z == 0)
    case 0x9:
        return !CPSR.c || CPSR.z; // LS: Unsigned Lower or Same (C == 0 || Z == 1)
    case 0xA:
        return CPSR.n == CPSR.v; // GE: Signed Greater or Equal (N == V)
    case 0xB:
        return CPSR.n != CPSR.v; // LT: Signed Less Than (N != V)
    case 0xC:
        return !CPSR.z && (CPSR.n == CPSR.v); // GT: Signed Greater Than (Z == 0 && N == V)
    case 0xD:
        return CPSR.z || (CPSR.n != CPSR.v); // LE: Signed Less or Equal (Z == 1 || N != V)
    case 0xF:
        LOG_MSG("[CPU][checar_condicao]: Condicao reservada/invalida NV (0xF) no opcode: " + std::to_string(opcode));
        return false; // NV: Never (No ARMv4T / GBA é reservado/nunca executa)
    default:
        LOG_MSG("[CPU][checar_condicao]: Codigo de condicao desconhecido: " + std::to_string(cond));
        return false;
    }
}

uint32_t Cpu::cpu_read32(uint32_t addr)
{ // Alerta de desalinhamento (32-bit precisa estar alinhado por 4 bytes)
    if (addr & 0x3)
    {
        std::ostringstream ss;
        ss << "[CPU][cpu_read32]: WARN - Leitura desalinhada de 32-bit no endereco 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << addr;
        LOG_MSG(ss.str());
    }

    uint8_t region = (addr >> 24) & 0xFF;

    // Regra de Proteção Anti-Dump da BIOS (0x00)
    if (region == 0x00 && addr < 0x00004000)
    {
        // Se a CPU estiver executando FORA da BIOS, devolve a trava
        if (*regMap[15] >= 0x00004000)
        {
            std::cout << "retornando bios latch no cpu_read32 \n";
            std::cout << "PC: " << *regMap[15] << "Endereço: 0x" << std::hex << addr << std::endl;
            return biosLatch;
        }
    }

    if (memoria != nullptr)
    {
        OpResult result = memoria->get32(addr);
        if (result.status_code == StCode::SUCESS)
        {

            return result.result;
        }

        std::ostringstream ss;
        ss << "[CPU][cpu_read32]: ERRO - Falha na leitura do barramento no endereco 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << addr
           << " (StatusCode: " << std::dec << static_cast<int>(result.status_code) << ")";
        LOG_MSG(ss.str());
        return 0x0;
    }

    std::ostringstream ss;
    ss << "[CPU][cpu_read32]: FATAL - Ponteiro de memoria nulo ao tentar ler 0x"
       << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << addr;
    LOG_MSG(ss.str());
    return 0x0;
}
uint16_t Cpu::cpu_read16(uint32_t addr)
{
    if (addr & 0x1)
    {
        std::ostringstream ss;
        ss << "[CPU][cpu_read16]: WARN - Leitura desalinhada de 16-bit no endereco 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << addr;
        LOG_MSG(ss.str());
    }

    uint8_t region = (addr >> 24) & 0xFF;

    if (region == 0x00 && addr < 0x00004000)
    {
        if (*regMap[15] >= 0x00004000)
        {
            return static_cast<uint16_t>(biosLatch);
        }
    }

    if (memoria != nullptr)
    {
        OpResult result = memoria->get16(addr);
        if (result.status_code == StCode::SUCESS)
        {
            return result.result;
        }

        std::ostringstream ss;
        ss << "[CPU][cpu_read16]: ERRO - Falha na leitura do barramento no endereco 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << addr
           << " (StatusCode: " << std::dec << static_cast<int>(result.status_code) << ")";
        LOG_MSG(ss.str());
        return 0x0;
    }

    std::ostringstream ss;
    ss << "[CPU][cpu_read16]: FATAL - Ponteiro de memoria nulo ao tentar ler 0x"
       << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << addr;
    LOG_MSG(ss.str());
    return 0x0;
}
uint8_t Cpu::cpu_read8(uint32_t addr)
{
    uint8_t region = (addr >> 24) & 0xFF;

    if (region == 0x00 && addr < 0x00004000)
    {
        if (*regMap[15] >= 0x00004000)
        {
            return static_cast<uint8_t>(biosLatch);
        }
    }

    if (memoria != nullptr)
    {
        OpResult result = memoria->get8(addr);
        if (result.status_code == StCode::SUCESS)
        {
            return result.result;
        }

        std::ostringstream ss;
        ss << "[CPU][cpu_read8]: ERRO - Falha na leitura do barramento no endereco 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << addr
           << " (StatusCode: " << std::dec << static_cast<int>(result.status_code) << ")";
        LOG_MSG(ss.str());
        return 0x0;
    }

    std::ostringstream ss;
    ss << "[CPU][cpu_read8]: FATAL - Ponteiro de memoria nulo ao tentar ler 0x"
       << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << addr;
    LOG_MSG(ss.str());
    return 0x0;
}

void Cpu::arm_alu(uint32_t opcode)
{
    uint8_t Rn = (opcode >> 16) & 0xF;
    uint8_t Rd = (opcode >> 12) & 0xF;

    bool updateFlag = (opcode >> 20) & 0x1;
    bool imediatoFlag = (opcode >> 25) & 0x1;

    bool flagN;
    bool flagZ;
    bool flagC = CPSR.c;
    bool flagV = CPSR.v;
    bool Carry = flagC;

    uint32_t op2;

    if (imediatoFlag)
    {
        uint8_t rorCount = ((opcode >> 8) & 0xF) * 2;
        uint32_t nn = opcode & 0xFF;

        op2 = std::rotr(nn, rorCount);
        if (rorCount != 0)
        {
            flagC = get_bit(op2, 31);
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

        OpResult shift_result = shift_barrier(shiftType, get_reg(Rm), count, origemImediato, flagC);

        if (shift_result.status_code != StCode::SUCESS)
        {
            std::ostringstream ss;
            ss << "[CPU][arm_alu]: ERRO - Falha no calculo de shift no opcode 0x"
               << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << opcode;
            LOG_MSG(ss.str());
        }

        op2 = shift_result.result;
        flagC = shift_result.extra;
    }

    uint32_t resultado;
    bool escrever_rd = true;

    uint8_t alu_op = (opcode >> 21) & 0xF;

    // Alerta de instrucao inefetiva ou decode incorreto (TST, TEQ, CMP, CMN sem bit S=1)
    if (!updateFlag && (alu_op >= 0x8 && alu_op <= 0xB))
    {
        std::ostringstream ss;
        ss << "[CPU][arm_alu]: WARN - Operacao de comparacao (0x"
           << std::hex << static_cast<int>(alu_op) << ") executada sem o bit S (updateFlag=0) no opcode 0x"
           << std::uppercase << std::setfill('0') << std::setw(8) << opcode;
        LOG_MSG(ss.str());
    }

    switch (alu_op)
    {
    case 0x0:
        resultado = get_reg(Rn) & op2;
        break;

    case 0x1:
        resultado = get_reg(Rn) ^ op2;
        break;

    case 0x2:
        resultado = get_reg(Rn) - op2;
        flagC = (get_reg(Rn) >= op2);
        flagV = ((get_reg(Rn) ^ op2) & (get_reg(Rn) ^ resultado)) >> 31;
        break;

    case 0x3:
        resultado = op2 - get_reg(Rn);
        flagC = (op2 >= get_reg(Rn));
        flagV = ((get_reg(Rn) ^ op2) & (op2 ^ resultado)) >> 31;
        break;

    case 0x4:
        resultado = get_reg(Rn) + op2;
        flagC = ((uint64_t)get_reg(Rn) + (uint64_t)op2) > UINT32_MAX;
        flagV = ((get_reg(Rn) ^ resultado) & (op2 ^ resultado)) >> 31;
        break;

    case 0x5:
        resultado = get_reg(Rn) + op2 + Carry;
        flagC = ((uint64_t)get_reg(Rn) + (uint64_t)op2 + Carry) > UINT32_MAX;
        flagV = ((get_reg(Rn) ^ resultado) & (op2 ^ resultado)) >> 31;
        break;

    case 0x6:
        resultado = get_reg(Rn) - op2 - !Carry;
        flagC = ((uint64_t)get_reg(Rn) - (uint64_t)op2 - !Carry) <= UINT32_MAX;
        flagV = ((get_reg(Rn) ^ op2) & (get_reg(Rn) ^ resultado)) >> 31;
        break;

    case 0x7:
        resultado = op2 - get_reg(Rn) - !Carry;
        flagC = ((uint64_t)op2 - (uint64_t)get_reg(Rn) - !Carry) <= UINT32_MAX;
        flagV = ((get_reg(Rn) ^ op2) & (op2 ^ resultado)) >> 31;
        break;

    case 0x8:
        resultado = get_reg(Rn) & op2;
        escrever_rd = false;
        break;

    case 0x9:
        resultado = get_reg(Rn) ^ op2;
        escrever_rd = false;
        break;

    case 0xA:
        resultado = get_reg(Rn) - op2;
        escrever_rd = false;
        flagC = (get_reg(Rn) >= op2);
        flagV = ((get_reg(Rn) ^ op2) & (get_reg(Rn) ^ resultado)) >> 31;
        break;

    case 0xB:
        resultado = get_reg(Rn) + op2;
        flagC = ((uint64_t)get_reg(Rn) + (uint64_t)op2) > UINT32_MAX;
        flagV = ((get_reg(Rn) ^ resultado) & (op2 ^ resultado)) >> 31;
        escrever_rd = false;
        break;

    case 0xC:
        resultado = get_reg(Rn) | op2;
        break;

    case 0xD:
        resultado = op2;
        break;

    case 0xE:
        resultado = get_reg(Rn) & (~op2);
        break;

    case 0xF:
        resultado = ~op2;
        break;

    default:
        std::ostringstream ss;
        ss << "[CPU][arm_alu]: ERRO - alu_op desconhecido (0x"
           << std::hex << static_cast<int>(alu_op) << ") no opcode 0x"
           << std::uppercase << std::setfill('0') << std::setw(8) << opcode;
        LOG_MSG(ss.str());
        return;
    }

    bool bitS = (opcode >> 20) & 0x1;

    if (Rd == 15 && bitS)
    {
        if (SPSRatual == nullptr)
        {
            std::ostringstream ss;
            ss << "[CPU][arm_alu]: ERRO - Tentativa de restaurar CPSR a partir de SPSRatual NULO (modo User/System?) no opcode 0x"
               << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << opcode;
            LOG_MSG(ss.str());
        }
        else
        {
            CPSR = *SPSRatual;
            update_reg_map();
        }

        set_reg(15, resultado);
        return; // Sai sem alterar as flags do novo CPSR
    }

    // e pra atualizar as flags?
    if (updateFlag)
    {
        flagN = resultado >> 31;
        flagZ = resultado == 0;
        CPSR.n = flagN;
        CPSR.z = flagZ;
        CPSR.c = flagC;
        CPSR.v = flagV;
    }

    // pode escrever o resultado em Rd ?
    if (escrever_rd)
    {
        set_reg(Rd, resultado);
    }
}
void Cpu::arm_mul(uint32_t opcode)
{
    bool updateFlag = (opcode >> 20) & 0x1;

    // registradores
    uint8_t Rd = (opcode >> 16) & 0xF;
    uint8_t Rn = (opcode >> 12) & 0xF;
    uint8_t Rs = (opcode >> 8) & 0xF;
    uint8_t Rm = opcode & 0xF;
    uint8_t RdHigh = Rd;
    uint8_t RdLow = Rn;

    // identificador e modificadores da multiplicação
    bool longFlag = (opcode >> 23) & 0x1;
    bool signedFlag = ((opcode >> 22) & 0x1);
    bool aculmulativo = (opcode >> 21) & 0x1;

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
        CPSR.z = resultado == 0 && resultadoLow == 0;
        CPSR.n = get_bit(resultado, 31);
    }

    // guarda o resultado em Rd se longFlag o resultado vai ser a parte alta e Rd tem o mesmo valor que RdHigh o RdLow já foi setado corretamente
    set_reg(Rd, resultado);
}
void Cpu::arm_branch(uint32_t opcode)
{
    bool linkBranch = (opcode >> 24) & 0x1;

    // Extrai o offset de 24 bits com extensão de sinal para 32 bits e multiplica por 4 (shift left de 2 bits)
    int32_t offset = (static_cast<int32_t>(opcode & 0x00FFFFFF) << 8) >> 6;

    uint32_t pc_atual = get_reg(15);
    uint32_t target_pc = pc_atual + offset;

    std::cout << "[DEBUG BRANCH] PC Antes: 0x" << std::hex << pc_atual
              << " | Target: 0x" << target_pc << std::dec << std::endl;

    // Alerta de desalinhamento de memória (Em modo ARM, os saltos precisam ser múltiplos de 4 bytes)
    if (target_pc & 0x3)
    {
        std::ostringstream ss;
        ss << "[CPU][arm_branch]: WARN - Salto para endereco desalinhado (nao multiplo de 4): 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << target_pc
           << " (Offset: " << std::dec << offset << " bytes) no opcode 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << opcode;
        LOG_MSG(ss.str());
    }

    // Se for BL (Branch with Link), salva o endereço de retorno em LR (R14)
    if (linkBranch)
    {
        set_reg(14, pc_atual - 4); // get_reg(15) vale (PC_execucao + 4)
    }

    // Atualiza o PC (R15) com o endereço de destino
    set_reg(15, target_pc);

    std::cout << "[DEBUG BRANCH] PC Depois de set_reg: 0x" << std::hex << get_reg(15)
              << std::dec << std::endl;
}
void Cpu::arm_bx(uint32_t opcode)
{
    uint8_t Rm = opcode & 0xF;
    uint32_t address = get_reg(Rm);

    bool modoThumb = address & 0x1;

    // Atualiza a flag thumb na união CPSR
    CPSR.thumb = modoThumb;

    if (modoThumb)
    {
        // Alinha para múltiplos de 2 bytes em modo Thumb
        set_reg(15, address & (~static_cast<uint32_t>(1)));
    }
    else
    {
        if (address & 0x3)
        {
            std::ostringstream ss;
            ss << "[CPU][arm_bx]: WARN - Salto para endereco desalinhado em modo ARM: 0x"
               << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << address
               << " no opcode 0x" << std::setw(8) << opcode;
            LOG_MSG(ss.str());
        }

        // Alinha para múltiplos de 4 bytes em modo ARM
        set_reg(15, address & (~static_cast<uint32_t>(3)));
    }
}
void Cpu::arm_msr(uint32_t opcode)
{
    bool imediato = (opcode >> 25) & 0x1;
    bool spsrFlag = (opcode >> 22) & 0x1;

    uint32_t fields = (opcode >> 16) & 0xF;
    if (fields == 0)
    {
        std::ostringstream ss;
        ss << "[CPU][arm_msr]: WARN - Operacao MSR sem campos especificados (mascara 0) no opcode 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << opcode;
        LOG_MSG(ss.str());
    }

    uint32_t mask = 0;
    if (fields & 0x1)
        mask |= 0x000000FF;
    if (fields & 0x2)
        mask |= 0x0000FF00;
    if (fields & 0x4)
        mask |= 0x00FF0000;
    if (fields & 0x8)
        mask |= 0xFF000000;

    CpuMode modeAtual = static_cast<CpuMode>(CPSR.mode);

    // Em modo Usuario, apenas o byte de flags (superior) pode ser alterado
    if (modeAtual == CpuMode::User)
    {
        mask &= 0xFF000000;
    }

    Psr *targetPSR = &CPSR;

    if (spsrFlag)
    {
        if (modeAtual == CpuMode::User || modeAtual == CpuMode::System || SPSRatual == nullptr)
        {
            std::ostringstream ss;
            ss << "[CPU][arm_msr]: ERRO - Acesso invalido ao SPSR no modo "
               << static_cast<int>(modeAtual) << " no opcode 0x"
               << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << opcode;
            LOG_MSG(ss.str());
            return;
        }
        targetPSR = SPSRatual;
    }

    uint32_t val = 0;
    if (imediato)
    {
        uint32_t nn = opcode & 0xFF;
        uint8_t rorCount = ((opcode >> 8) & 0xF) * 2;
        val = std::rotr(nn, rorCount);
    }
    else
    {
        uint8_t Rm = opcode & 0xF;
        val = get_reg(Rm);
    }

    // Aplica a máscara diretamente usando a propriedade .val da união Psr
    targetPSR->val = (targetPSR->val & ~mask) | (val & mask);

    // Se o CPSR foi modificado, atualizamos o mapeamento de registradores
    if (!spsrFlag)
    {
        update_reg_map();
    }
}
void Cpu::arm_mrs(uint32_t opcode)
{
    uint8_t Rd = (opcode >> 12) & 0xF;
    bool spsrFlag = (opcode >> 22) & 0x1;

    if (Rd == 15)
    {
        std::ostringstream ss;
        ss << "[CPU][arm_mrs]: WARN - Instrucao MRS tentando escrever diretamente em R15 (PC) no opcode 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << opcode;
        LOG_MSG(ss.str());
    }

    uint32_t val = 0;
    if (spsrFlag)
    {
        if (SPSRatual == nullptr)
        {
            std::ostringstream ss;
            ss << "[CPU][arm_mrs]: ERRO - Acesso invalido ao SPSR no modo "
               << static_cast<int>(CPSR.mode) << " (SPSRatual == nullptr) no opcode 0x"
               << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << opcode;
            LOG_MSG(ss.str());
            return;
        }
        val = SPSRatual->val;
    }
    else
    {
        val = CPSR.val;
    }

    set_reg(Rd, val);
}
void Cpu::arm_sdt(uint32_t opcode)
{
    bool imediato = (opcode >> 25) & 0x1;
    bool preIndex = (opcode >> 24) & 0x1;
    bool upDown = (opcode >> 23) & 0x1;
    bool wordByte = (opcode >> 22) & 0x1;
    bool writeBack = (opcode >> 21) & 0x1;
    bool loadStore = (opcode >> 20) & 0x1;

    uint8_t Rd = (opcode >> 12) & 0xF;
    uint8_t Rn = (opcode >> 16) & 0xF;

    uint32_t offset = 0;
    if (imediato)
    {
        uint8_t Rm = opcode & 0xF;
        uint8_t shiftType = (opcode >> 5) & 0b11;
        uint8_t shiftCount = (opcode >> 7) & 0x1F;

        OpResult op2 = shift_barrier(shiftType, get_reg(Rm), shiftCount, true, CPSR.c);
        if (op2.status_code != StCode::SUCESS)
        {
            std::ostringstream ss;
            ss << "[CPU][arm_sdt]: ERRO - Falha no calculo do offset deslocado no opcode 0x"
               << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << opcode;
            LOG_MSG(ss.str());
        }
        offset = op2.result;
    }
    else
    {
        offset = opcode & 0xFFF;
    }

    uint32_t endereço = get_reg(Rn);
    if (preIndex)
    {
        endereço = (upDown) ? get_reg(Rn) + offset : get_reg(Rn) - offset;
    }

    if (loadStore)
    {
        std::cout << "leitura \n";
        uint32_t resultado = 0;
        if (wordByte)
        {
            resultado = cpu_read8(endereço);
        }
        else
        {
            std::cout << "leitura 32bits \n";
            resultado = cpu_read32(endereço);
            uint8_t desalinhamentoEmBits = (endereço & 0b11) * 8;
            resultado = std::rotr(resultado, desalinhamentoEmBits);
        }

        set_reg(Rd, resultado);
    }
    else
    {
        if (memoria == nullptr)
        {
            std::ostringstream ss;
            ss << "[CPU][arm_sdt]: FATAL - Ponteiro de memoria nulo ao tentar escrever no endereco 0x"
               << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << endereço;
            LOG_MSG(ss.str());
            return;
        }

        if (wordByte)
        {
            memoria->set8(endereço, get_reg(Rd) & 0xFF);
        }
        else
        {
            if (endereço & 0x3)
            {
                std::ostringstream ss;
                ss << "[CPU][arm_sdt]: WARN - Escrita de Word (32-bit) desalinhada no endereco 0x"
                   << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << endereço;
                LOG_MSG(ss.str());
            }
            memoria->set32(endereço, get_reg(Rd));
        }
    }

    if (writeBack || !preIndex)
    {
        if (!loadStore || (Rd != Rn))
        {
            set_reg(Rn, (upDown) ? get_reg(Rn) + offset : get_reg(Rn) - offset);
        }
    }
}
void Cpu::arm_hw_s(uint32_t opcode)
{
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

    uint32_t offset = (offsetHigh << 4) | offsetLow;
    if (imediato)
    {
        offset = get_reg(Rm);
    }

    uint32_t endereço = get_reg(Rn);
    if (preIndex)
    {
        endereço = (upDown) ? get_reg(Rn) + offset : get_reg(Rn) - offset;
    }

    if (loadStore) // LDRH / LDRSB / LDRSH
    {
        if (halfword)
        {
            uint32_t resultado = cpu_read16(endereço);
            if (signed_)
            {
                resultado |= (((resultado & 0x8000) ? 0xFFFF : 0x00) << 16);
            }
            set_reg(Rd, resultado);
        }
        else
        {
            if (!signed_)
            {
                std::ostringstream ss;
                ss << "[CPU][arm_hw_s]: WARN - Combinacao reservada/invalida (S=0, H=0) no opcode 0x"
                   << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << opcode;
                LOG_MSG(ss.str());
            }

            uint32_t resultado = cpu_read8(endereço);
            if (signed_)
            {
                resultado |= (((resultado & 0x80) ? 0xFFFFFF : 0x00) << 8);
            }
            set_reg(Rd, resultado);
        }
    }
    else // STRH
    {
        if (memoria == nullptr)
        {
            std::ostringstream ss;
            ss << "[CPU][arm_hw_s]: FATAL - Ponteiro de memoria nulo ao tentar escrever no endereco 0x"
               << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << endereço;
            LOG_MSG(ss.str());
            return;
        }

        if (halfword)
        {
            if (endereço & 0x1)
            {
                std::ostringstream ss;
                ss << "[CPU][arm_hw_s]: WARN - Escrita de Halfword (16-bit) desalinhada no endereco 0x"
                   << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << endereço;
                LOG_MSG(ss.str());
            }
            memoria->set16(endereço, get_reg(Rd) & 0xFFFF);
        }
        else
        {
            memoria->set8(endereço, get_reg(Rd) & 0xFF);
        }
    }

    if (writeBack || !preIndex)
    {
        if (!loadStore || (Rd != Rn))
        {
            set_reg(Rn, (upDown) ? get_reg(Rn) + offset : get_reg(Rn) - offset);
        }
    }
}
void Cpu::arm_bdt(uint32_t opcode)
{
    bool preIndex = (opcode >> 24) & 0x1;
    bool upDown = (opcode >> 23) & 0x1;
    bool bitS = (opcode >> 22) & 0x1;
    bool writeBack = (opcode >> 21) & 0x1;
    bool loadStore = (opcode >> 20) & 0x1;

    uint8_t Rn = (opcode >> 16) & 0xF;
    uint16_t regList = opcode & 0xFFFF;
    bool pcNaLista = (regList >> 15) & 0x1;

    // Detecta se é uma transferência forçada para o banco de registrador do Modo User
    bool userBankTransfer = bitS && !pcNaLista;

    uint32_t regCount = std::popcount(regList);
    uint32_t start = 0;

    if (upDown)
    {
        start = preIndex ? (get_reg(Rn) + 4) : get_reg(Rn);
    }
    else
    {
        start = preIndex ? (get_reg(Rn) - (regCount * 4)) : (get_reg(Rn) - (regCount * 4) + 4);
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

                // Se for User Bank Transfer, grava no banco User (R0-R14)
                if (userBankTransfer)
                    R[i] = valor;
                else
                    set_reg(i, valor);
            }
            else
            {
                // Se for User Bank Transfer, lê do banco User (R0-R14)
                uint32_t valor = userBankTransfer ? R[i] : get_reg(i);
                memoria->set32(addr, valor);
            }
            addr += 4;
        }
    }

    // 2. Aplica o Writeback DEPOIS do loop
    if (writeBack)
    {
        uint32_t totalBytes = regCount * 4;
        uint32_t novoBase = upDown ? (get_reg(Rn) + totalBytes) : (get_reg(Rn) - totalBytes);

        if (!loadStore || !((regList >> Rn) & 1))
        {
            set_reg(Rn, novoBase);
        }
    }

    // 3. Retorno de Exceção (APENAS quando LDM e PC estiver na lista)
    if (bitS && loadStore && pcNaLista)
    {
        if (SPSRatual != nullptr)
        {
            CPSR = *SPSRatual;
            update_reg_map(); // Atualiza os ponteiros de registrador para o novo modo
        }
        else
        {
            LOG_MSG("[CPU][arm_bdt]: ERRO - LDM restaurando CPSR com SPSRatual NULO");
        }
    }
}
void Cpu::arm_swp(uint32_t opcode)
{
    bool wordByte = (opcode >> 22) & 0x1;
    uint8_t Rn = (opcode >> 16) & 0xF;
    uint8_t Rd = (opcode >> 12) & 0xF;
    uint8_t Rm = opcode & 0xF;

    uint32_t addr = get_reg(Rn);

    if (memoria == nullptr)
    {
        std::ostringstream ss;
        ss << "[CPU][arm_swp]: FATAL - Ponteiro de memoria nulo ao executar SWP no endereco 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << addr;
        LOG_MSG(ss.str());
        return;
    }

    if (wordByte)
    {
        uint32_t val = cpu_read8(addr);
        set_reg(Rd, val);
        memoria->set8(addr, get_reg(Rm) & 0xFF);
    }
    else
    {
        if (addr & 0x3)
        {
            std::ostringstream ss;
            ss << "[CPU][arm_swp]: WARN - Operacao SWP (Word 32-bit) em endereco desalinhado: 0x"
               << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << addr;
            LOG_MSG(ss.str());
        }

        uint32_t val = cpu_read32(addr);
        set_reg(Rd, val);
        memoria->set32(addr, get_reg(Rm));
    }
}
void Cpu::step_arm()
{
    // 1. FETCH & ATUALIZAÇÃO DO BIOS LATCH
    uint32_t pc_execucao = *regMap[15];

    if (memoria == nullptr)
    {
        std::ostringstream ss;
        ss << "[CPU][step_arm]: FATAL - Ponteiro de memoria nulo ao tentar buscar instrucao no PC: 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << pc_execucao;
        LOG_MSG(ss.str());
        return;
    }

    OpResult getOpcode = memoria->get32(pc_execucao);
    if (getOpcode.status_code != StCode::SUCESS)
    {
        std::ostringstream ss;
        ss << "[CPU][step_arm]: ERRO - Falha na busca (fetch) do opcode no PC: 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << pc_execucao;
        LOG_MSG(ss.str());
        return;
    }

    uint32_t opcode = getOpcode.result;

    // Se a instrução foi buscada da BIOS, salva o valor no Latch de proteção anti-dump
    if (pc_execucao < 0x00004000)
    {
        biosLatch = opcode;
    }

    // Prepara a flag para detectar se a instrução atual modificou o PC
    pc_alterado = false;

    // 2. CHECK CONDITION (Bits 31..28)
    if (!checar_condicao(opcode))
    {
        set_reg(15, pc_execucao + 4); // Avança PC mesmo se a instrução for ignorada
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
        std::cout << "arm_sdt chamado\n";
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
        std::ostringstream ss;
        ss << "[CPU][step_arm]: ERRO - Opcode ARM desconhecido/invalido: 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << opcode
           << " no PC: 0x" << pc_execucao;
        LOG_MSG(ss.str());

        interrupt(CpuMode::Undefined, 0x00000004);
        break;
    }
    }

    // Se NENHUM salto foi executado pela instrução, avança para a próxima instrução
    if (!pc_alterado)
    {
        *regMap[15] += 4;
    }
}

void Cpu::thumb_format_1(uint16_t opcode)
{
    uint8_t operation = (opcode >> 11) & 0b11;
    uint8_t shiftCount = (opcode >> 6) & 0x1F;
    uint8_t Rs = (opcode >> 3) & 0b111;
    uint8_t Rd = opcode & 0b111;

    if (operation == 0b11)
    {
        std::ostringstream ss;
        ss << "[CPU][thumb_format_1]: WARN - Operacao reservada (0b11) no opcode 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << opcode;
        LOG_MSG(ss.str());
    }

    // origemImediato deve ser true para tratar LSR/ASR #0 como #32
    OpResult shiftStatus = shift_barrier(operation, get_reg(Rs), shiftCount, true, CPSR.c);

    uint32_t resultado = shiftStatus.result;
    set_reg(Rd, resultado);

    CPSR.n = (resultado >> 31) & 1;
    CPSR.z = (resultado == 0);
    CPSR.c = shiftStatus.extra & 0x1;
}
void Cpu::thumb_format_2(uint16_t opcode)
{
    bool imediato = (opcode >> 10) & 0x1;
    bool sub = (opcode >> 9) & 0x1;

    uint8_t Rs = (opcode >> 3) & 0b111;
    uint8_t Rd = opcode & 0b111;
    uint8_t Rn = (opcode >> 6) & 0b111;

    uint32_t op2 = (imediato) ? Rn : get_reg(Rn);
    uint32_t valRs = get_reg(Rs);

    bool flagC = false;
    bool overflow = false;
    uint32_t resultado = 0;

    if (sub)
    {
        resultado = valRs - op2;
        flagC = valRs >= op2;
        overflow = ((valRs ^ op2) & (valRs ^ resultado)) >> 31;
    }
    else
    {
        resultado = valRs + op2;
        flagC = (static_cast<uint64_t>(valRs) + op2) > UINT32_MAX;
        overflow = (~(valRs ^ op2) & (valRs ^ resultado)) >> 31;
    }

    set_reg(Rd, resultado);

    CPSR.n = (resultado >> 31) & 1;
    CPSR.z = (resultado == 0);
    CPSR.c = flagC;
    CPSR.v = overflow;
}
void Cpu::thumb_format_3(uint16_t opcode)
{
    uint8_t operation = (opcode >> 11) & 0b11;
    uint8_t Rd = (opcode >> 8) & 0b111;
    uint8_t imm8 = opcode & 0xFF;

    bool flagN = CPSR.n;
    bool flagZ = CPSR.z;
    bool flagC = CPSR.c;
    bool flagV = CPSR.v;

    uint32_t valRd = get_reg(Rd);
    uint32_t resultado = 0;

    switch (operation)
    {
    case 0b00: // MOV
        resultado = imm8;
        flagN = false;
        break;

    case 0b01: // CMP
    case 0b11: // SUB
        resultado = valRd - imm8;
        flagN = (resultado >> 31) & 1;
        flagC = valRd >= imm8;
        flagV = ((valRd ^ imm8) & (valRd ^ resultado)) >> 31;
        break;

    case 0b10: // ADD
        resultado = valRd + imm8;
        flagN = (resultado >> 31) & 1;
        flagC = (static_cast<uint64_t>(valRd) + imm8) > UINT32_MAX;
        flagV = (~(valRd ^ imm8) & (valRd ^ resultado)) >> 31;
        break;

    default:
        break;
    }

    flagZ = (resultado == 0);

    if (operation != 0b01) // Nao escreve em Rd se for CMP
    {
        set_reg(Rd, resultado);
    }

    CPSR.n = flagN;
    CPSR.z = flagZ;
    CPSR.c = flagC;
    CPSR.v = flagV;
}
void Cpu::thumb_format_4(uint16_t opcode)
{
    uint8_t op = (opcode >> 6) & 0xF;
    uint8_t Rs = (opcode >> 3) & 0b111;
    uint8_t Rd = opcode & 0b111;

    uint32_t op1 = get_reg(Rd);
    uint32_t op2 = get_reg(Rs);
    uint32_t resultado = 0;

    bool flagN = CPSR.n;
    bool flagZ = CPSR.z;
    bool flagC = CPSR.c;
    bool flagV = CPSR.v;

    bool writeResult = true; // TST, CMP e CMN nao escrevem em Rd

    switch (op)
    {
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

    // Shifts por registrador (origemImediato = false)
    case 0x2: // LSL
    case 0x3: // LSR
    case 0x4: // ASR
    case 0x7: // ROR
    {
        uint8_t shiftType = (op == 0x7) ? 0b11 : (op - 0x2);
        uint8_t shiftAmount = op2 & 0xFF;

        OpResult shiftRes = shift_barrier(shiftType, op1, shiftAmount, false, flagC);
        resultado = shiftRes.result;
        flagC = shiftRes.extra & 0x1;
        break;
    }

    case 0x5: // ADC
    {
        uint64_t sum = static_cast<uint64_t>(op1) + op2 + (flagC ? 1 : 0);
        resultado = static_cast<uint32_t>(sum);
        flagC = sum > UINT32_MAX;
        flagV = (~(op1 ^ op2) & (op1 ^ resultado)) >> 31;
        break;
    }

    case 0x6: // SBC
    {
        uint64_t sub = static_cast<uint64_t>(op1) - op2 - (!flagC ? 1 : 0);
        resultado = static_cast<uint32_t>(sub);
        flagC = sub <= UINT32_MAX;
        flagV = ((op1 ^ op2) & (op1 ^ resultado)) >> 31;
        break;
    }

    case 0x9: // NEG
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
        flagC = (static_cast<uint64_t>(op1) + op2) > UINT32_MAX;
        flagV = (~(op1 ^ op2) & (op1 ^ resultado)) >> 31;
        writeResult = false;
        break;

    case 0xD: // MUL
        resultado = op1 * op2;
        break;

    default:
        break;
    }

    flagN = (resultado >> 31) & 1;
    flagZ = (resultado == 0);

    if (writeResult)
    {
        set_reg(Rd, resultado);
    }

    CPSR.n = flagN;
    CPSR.z = flagZ;
    CPSR.c = flagC;
    CPSR.v = flagV;
}
void Cpu::thumb_format_5(uint16_t opcode)
{
    uint8_t operation = (opcode >> 8) & 0b11;
    uint8_t Rs = (opcode >> 3) & 0xF;
    uint8_t Rd = ((opcode >> 4) & 0x8) | (opcode & 0b111);

    uint32_t valRd = get_reg(Rd);
    uint32_t valRs = get_reg(Rs);

    switch (operation)
    {
    case 0b00: // ADD
    {
        uint32_t res = valRd + valRs;
        set_reg(Rd, res);
        if (Rd == 15)
        {
            set_reg(15, res & (~1U));
            pc_alterado = true; // Notifica a CPU sobre o salto
        }
        break;
    }

    case 0b01: // CMP
    {
        uint32_t resultado = valRd - valRs;

        CPSR.n = (resultado >> 31) & 1;
        CPSR.z = (resultado == 0);
        CPSR.c = valRd >= valRs;
        CPSR.v = ((valRd ^ valRs) & (valRd ^ resultado)) >> 31;
        break;
    }

    case 0b10: // MOV
    {
        set_reg(Rd, valRs);
        if (Rd == 15)
        {
            set_reg(15, valRs & (~1U));
            pc_alterado = true; // Notifica a CPU sobre o salto
        }
        break;
    }

    case 0b11: // BX
    {
        bool thumbMode = valRs & 0x1;
        CPSR.thumb = thumbMode;

        if (thumbMode)
        {
            set_reg(15, valRs & (~1U));
        }
        else
        {
            if (valRs & 0x3)
            {
                std::ostringstream ss;
                ss << "[CPU][thumb_format_5]: WARN - Salto BX para modo ARM desalinhado: 0x"
                   << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << valRs;
                LOG_MSG(ss.str());
            }
            set_reg(15, valRs & (~3U));
        }

        pc_alterado = true; // Notifica a CPU sobre o salto
        break;
    }
    }
}
void Cpu::thumb_format_6(uint16_t opcode)
{
    uint8_t Rd = (opcode >> 8) & 0b111;
    uint32_t imm8 = opcode & 0xFF;

    uint32_t pc_alinhado = get_reg(15) & ~2U;
    uint32_t endereco = pc_alinhado + (imm8 << 2);

    uint32_t resultado = cpu_read32(endereco);
    set_reg(Rd, resultado);
}
void Cpu::thumb_format_7(uint16_t opcode)
{
    bool L = (opcode >> 11) & 1; // 1 = Load, 0 = Store
    bool B = (opcode >> 10) & 1; // 1 = Byte, 0 = Word

    uint8_t Ro = (opcode >> 6) & 0b111;
    uint8_t Rb = (opcode >> 3) & 0b111;
    uint8_t Rd = opcode & 0b111;

    uint32_t endereco = get_reg(Rb) + get_reg(Ro);

    if (L)
    {
        if (B)
        {
            set_reg(Rd, cpu_read8(endereco));
        }
        else
        {
            set_reg(Rd, cpu_read32(endereco));
        }
    }
    else
    {
        if (memoria == nullptr)
        {
            std::ostringstream ss;
            ss << "[CPU][thumb_format_7]: FATAL - Memoria nula ao escrever no endereco 0x"
               << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << endereco;
            LOG_MSG(ss.str());
            return;
        }

        if (B)
        {
            memoria->set8(endereco, get_reg(Rd) & 0xFF);
        }
        else
        {
            if (endereco & 0x3)
            {
                std::ostringstream ss;
                ss << "[CPU][thumb_format_7]: WARN - Escrita STR desalinhada em 0x"
                   << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << endereco;
                LOG_MSG(ss.str());
            }
            memoria->set32(endereco, get_reg(Rd));
        }
    }
}
void Cpu::thumb_format_8(uint16_t opcode)
{
    bool H = (opcode >> 11) & 1;
    bool S = (opcode >> 10) & 1;
    uint8_t Ro = (opcode >> 6) & 0b111;
    uint8_t Rb = (opcode >> 3) & 0b111;
    uint8_t Rd = opcode & 0b111;

    uint32_t addr = get_reg(Rb) + get_reg(Ro);

    if (!S && !H) // STRH
    {
        if (memoria == nullptr)
        {
            std::ostringstream ss;
            ss << "[CPU][thumb_format_8]: FATAL - Memoria nula ao escrever STRH em 0x"
               << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << addr;
            LOG_MSG(ss.str());
            return;
        }
        memoria->set16(addr, get_reg(Rd) & 0xFFFF);
    }
    else if (!S && H) // LDRH
    {
        set_reg(Rd, cpu_read16(addr));
    }
    else if (S && !H) // LDSB
    {
        uint32_t val = cpu_read8(addr);
        set_reg(Rd, static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(val))));
    }
    else if (S && H) // LDSH
    {
        uint32_t val = cpu_read16(addr);
        set_reg(Rd, static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(val))));
    }
}
void Cpu::thumb_format_9(uint16_t opcode)
{
    bool B = (opcode >> 12) & 1;
    bool L = (opcode >> 11) & 1;
    uint32_t imm5 = (opcode >> 6) & 0x1F;
    uint8_t Rb = (opcode >> 3) & 0b111;
    uint8_t Rd = opcode & 0b111;

    uint32_t offset = B ? imm5 : (imm5 << 2);
    uint32_t addr = get_reg(Rb) + offset;

    if (L)
    {
        if (B)
        {
            set_reg(Rd, cpu_read8(addr));
        }
        else
        {
            set_reg(Rd, cpu_read32(addr));
        }
    }
    else
    {
        if (memoria == nullptr)
        {
            std::ostringstream ss;
            ss << "[CPU][thumb_format_9]: FATAL - Memoria nula ao escrever em 0x"
               << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << addr;
            LOG_MSG(ss.str());
            return;
        }

        if (B)
        {
            memoria->set8(addr, get_reg(Rd) & 0xFF);
        }
        else
        {
            memoria->set32(addr, get_reg(Rd));
        }
    }
}
void Cpu::thumb_format_10(uint16_t opcode)
{
    bool L = (opcode >> 11) & 1;
    uint32_t imm5 = (opcode >> 6) & 0x1F;
    uint8_t Rb = (opcode >> 3) & 0b111;
    uint8_t Rd = opcode & 0b111;

    uint32_t offset = imm5 << 1;
    uint32_t addr = get_reg(Rb) + offset;

    if (L)
    {
        set_reg(Rd, cpu_read16(addr));
    }
    else
    {
        if (memoria == nullptr)
        {
            std::ostringstream ss;
            ss << "[CPU][thumb_format_10]: FATAL - Memoria nula ao escrever STRH em 0x"
               << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << addr;
            LOG_MSG(ss.str());
            return;
        }
        memoria->set16(addr, get_reg(Rd) & 0xFFFF);
    }
}
void Cpu::thumb_format_11(uint16_t opcode)
{
    bool L = (opcode >> 11) & 1;
    uint8_t Rd = (opcode >> 8) & 0b111;
    uint32_t imm8 = opcode & 0xFF;

    uint32_t offset = imm8 << 2;
    uint32_t addr = get_reg(13) + offset;

    if (L)
    {
        set_reg(Rd, cpu_read32(addr));
    }
    else
    {
        if (memoria == nullptr)
        {
            std::ostringstream ss;
            ss << "[CPU][thumb_format_11]: FATAL - Memoria nula ao escrever em 0x"
               << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << addr;
            LOG_MSG(ss.str());
            return;
        }
        memoria->set32(addr, get_reg(Rd));
    }
}
void Cpu::thumb_format_12(uint16_t opcode)
{
    bool isSP = (opcode >> 11) & 1;
    uint8_t Rd = (opcode >> 8) & 0b111;
    uint32_t imm8 = opcode & 0xFF;
    uint32_t offset = imm8 << 2;

    uint32_t base = isSP ? get_reg(13) : (get_reg(15) & ~2U);
    set_reg(Rd, base + offset);
}
void Cpu::thumb_format_13(uint16_t opcode)
{
    bool isSub = (opcode >> 7) & 1;
    uint32_t imm7 = opcode & 0x7F;
    uint32_t offset = imm7 << 2;

    if (isSub)
    {
        set_reg(13, get_reg(13) - offset);
    }
    else
    {
        set_reg(13, get_reg(13) + offset);
    }
}
void Cpu::thumb_format_14(uint16_t opcode)
{
    bool isPop = (opcode >> 11) & 1;
    bool storeLR_loadPC = (opcode >> 8) & 1;
    uint8_t regList = opcode & 0xFF;

    if (!isPop) // PUSH
    {
        if (memoria == nullptr)
        {
            std::ostringstream ss;
            ss << "[CPU][thumb_format_14]: FATAL - Memoria nula durante PUSH";
            LOG_MSG(ss.str());
            return;
        }

        if (storeLR_loadPC)
        {
            set_reg(13, get_reg(13) - 4);
            memoria->set32(get_reg(13), get_reg(14));
        }
        for (int i = 7; i >= 0; i--)
        {
            if ((regList >> i) & 1)
            {
                set_reg(13, get_reg(13) - 4);
                memoria->set32(get_reg(13), get_reg(i));
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
                set_reg(13, get_reg(13) + 4);
            }
        }
        if (storeLR_loadPC)
        {
            uint32_t val = cpu_read32(get_reg(13));
            set_reg(15, val & ~1U); // Garante alinhamento de instrução no PC
            set_reg(13, get_reg(13) + 4);

            pc_alterado = true; // Notifica a CPU sobre o salto de retorno
        }
    }
}
void Cpu::thumb_format_15(uint16_t opcode)
{
    bool isLoad = (opcode >> 11) & 1;
    uint8_t Rb = (opcode >> 8) & 0b111;
    uint8_t regList = opcode & 0xFF;

    if (regList == 0)
    {
        std::ostringstream ss;
        ss << "[CPU][thumb_format_15]: WARN - Lista de registradores vazia no opcode 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << opcode;
        LOG_MSG(ss.str());
    }

    uint32_t addr = get_reg(Rb);

    if (addr & 0x3)
    {
        std::ostringstream ss;
        ss << "[CPU][thumb_format_15]: WARN - Leitura/Escrita em bloco desalinhada no endereco 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << addr;
        LOG_MSG(ss.str());
    }

    for (int i = 0; i <= 7; i++)
    {
        if ((regList >> i) & 1)
        {
            if (isLoad)
            {
                uint32_t val = cpu_read32(addr);
                set_reg(i, val);
            }
            else
            {
                if (memoria == nullptr)
                {
                    std::ostringstream ss;
                    ss << "[CPU][thumb_format_15]: FATAL - Memoria nula ao executar STMIA no endereco 0x"
                       << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << addr;
                    LOG_MSG(ss.str());
                    return;
                }
                memoria->set32(addr, get_reg(i));
            }
            addr += 4;
        }
    }

    // Writeback só se aplica se for STMIA ou se Rb NÃO foi um dos registradores carregados
    if (!isLoad || !((regList >> Rb) & 1))
    {
        set_reg(Rb, addr);
    }
}
void Cpu::thumb_format_16(uint16_t opcode)
{
    uint8_t cond = (opcode >> 8) & 0xF;

    if (cond == 0xF)
    {
        // SWI (Tratado no formato 17)
        interrupt(CpuMode::Supervisor, 0x00000008);
        return;
    }

    // checar_condicao espera a condicao nos bits 31..28
    bool satisfeita = checar_condicao(static_cast<uint32_t>(cond) << 28);

    if (satisfeita)
    {
        int32_t offset = static_cast<int8_t>(opcode & 0xFF);
        set_reg(15, get_reg(15) + (offset << 1));
        pc_alterado = true; // Notifica a CPU sobre o salto efetuado
    }
}
void Cpu::thumb_format_17()
{
    // Chama o manipulador de interrupção de Software (SWI)
    interrupt(CpuMode::Supervisor, 0x00000008);
}
void Cpu::thumb_format_18(uint16_t opcode)
{
    // Extrai o offset de 11 bits e faz extensão de sinal para 32 bits
    int32_t offset = (static_cast<int32_t>(opcode & 0x7FF) << 21) >> 20;

    set_reg(15, get_reg(15) + offset);
    pc_alterado = true; // Notifica a CPU sobre o salto efetuado
}
void Cpu::thumb_format_19(uint16_t opcode)
{
    bool H = (opcode >> 11) & 1;
    uint32_t offset11 = opcode & 0x7FF;

    if (!H) // Primeira instrução (Parte 1)
    {
        int32_t off = (static_cast<int32_t>(offset11) << 21) >> 21;
        set_reg(14, get_reg(15) + (off << 12));
    }
    else // Segunda instrução (Parte 2)
    {
        uint32_t next_pc = get_reg(14) + (offset11 << 1);
        set_reg(14, (get_reg(15) - 2) | 1);
        set_reg(15, next_pc & ~1U);
        pc_alterado = true; // Notifica a CPU sobre o salto efetuado
    }
}
void Cpu::step_thumb()
{
    uint32_t pc_atual = *regMap[15];

    if (memoria == nullptr)
    {
        std::ostringstream ss;
        ss << "[CPU][step_thumb]: FATAL - Memoria nula ao buscar instrucao no PC: 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << pc_atual;
        LOG_MSG(ss.str());
        return;
    }

    OpResult fetchRes = memoria->get16(pc_atual);
    if (fetchRes.status_code != StCode::SUCESS)
    {
        std::ostringstream ss;
        ss << "[CPU][step_thumb]: ERRO - Falha no fetch da instrucao no PC: 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << pc_atual;
        LOG_MSG(ss.str());
        return;
    }

    uint16_t opcode = fetchRes.result;

    // Em Thumb (16 bits), o barramento de 32 bits da BIOS duplica a halfword lida
    if (pc_atual < 0x00004000)
    {
        biosLatch = (static_cast<uint32_t>(opcode) << 16) | opcode;
    }

    pc_alterado = false;

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
            std::ostringstream ss;
            ss << "[CPU][step_thumb]: ERRO - Opcode THUMB invalido (0xB...): 0x"
               << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << opcode
               << " no PC: 0x" << std::setw(8) << pc_atual;
            LOG_MSG(ss.str());

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
            thumb_format_17(); // SWI em Thumb
        }
        else
        {
            thumb_format_16(opcode);
        }
        break;

    case 0x1C:
    case 0x1D:
        thumb_format_18(opcode);
        break;

    case 0x1E:
    case 0x1F:
        thumb_format_19(opcode);
        break;

    default:
        std::ostringstream ss;
        ss << "[CPU][step_thumb]: ERRO - Opcode THUMB nao mapeado: 0x"
           << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << opcode
           << " no PC: 0x" << std::setw(8) << pc_atual;
        LOG_MSG(ss.str());

        interrupt(CpuMode::Undefined, 0x00000004);
        break;
    }

    if (!pc_alterado)
    {
        *regMap[15] += 2;
    }
}

uint32_t Cpu::step()
{
    pc_alterado = false;

    if (memoria == nullptr)
    {
        LOG_MSG("[CPU][step]: FATAL - Ponteiro de memoria e nulo");
        return 0;
    }

    // Leitura dos registradores de interrupção
    uint16_t IE = memoria->get16(static_cast<uint32_t>(IorAddr::IE)).result;
    uint16_t IF = memoria->get16(static_cast<uint32_t>(IorAddr::IF)).result;
    uint16_t IME = memoria->get16(static_cast<uint32_t>(IorAddr::IME)).result;

    bool IRQhabilitada = (CPSR.irq_disable == 0); // Bit I = 0 no CPSR

    // Checa se o bit 0 do IME está ativo E se há interrupção pendente
    bool irqPendente = (IME & 1) && ((IE & IF) != 0) && IRQhabilitada;

    if (irqPendente)
    {
        interrupt(CpuMode::IRQ, 0x00000018);
        return 7;
    }

    pc_alterado = false;
    if (CPSR.thumb)
    {
        step_thumb();
    }
    else
    {
        step_arm();
    }
    return 3;
}
