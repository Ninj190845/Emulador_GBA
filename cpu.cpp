#include "cpu.hpp"
#include <bit>
#include <cstdint>
#include <iostream>

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

OpResult Cpu::shift_barrier(uint32_t shiftType, uint32_t valor, uint8_t count, bool origemImediato, bool flagC)
{
    uint32_t resultado = 0;
    shiftType &= 0b11;
    switch (shiftType)
    {
    case 0b00:
    {
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
            resultado = std::rotr(valor, count);
            flagC = get_bit(resultado, 31);
        }

        break;
    }
    default:
        break;
    }
    OpResult returning(resultado, 0, true);
    return returning;
}

void Cpu::instrução_alu(uint32_t opcode)
{
    uint8_t Rn = (opcode >> 16) & 0xF;
    uint8_t Rd = (opcode >> 12) & 0xF;

    bool updateFlag = (opcode >> 20) & 0x1;
    bool imediatoFlag = (opcode >> 25) & 0x1;

    bool flagN;
    bool flagZ;
    bool flagC = get_bit(CPSR, 29);
    bool flagV = get_bit(CPSR, 28);
    bool Carry = flagC;

    uint32_t val;
    uint8_t alu_op = (opcode >> 21) & 0xF;

    if (imediatoFlag)
    {
        uint8_t rorCount = ((opcode >> 8) & 0xF) * 2;
        uint32_t nn = opcode & 0xFF;
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

        val = op2.valor;
        flagC = op2.carryOut;
    }

    uint32_t resultado;
    bool escrever_rd = true;

    switch (alu_op)
    {
    case 0x0:
        resultado = R[Rn] & val;
        break;

    case 0x1:
        resultado = R[Rn] ^ val;
        break;

    case 0x2:
        resultado = R[Rn] - val;
        flagC = (R[Rn] >= val);
        flagV = ((R[Rn] ^ val) & (R[Rn] ^ resultado)) >> 31;
        break;

    case 0x3:
        resultado = val - R[Rn];
        flagC = (val >= R[Rn]);
        flagV = ((R[Rn] ^ val) & (val ^ resultado)) >> 31;
        break;

    case 0x4:
        resultado = R[Rn] + val;
        flagC = ((uint64_t)R[Rn] + (uint64_t)val) > UINT32_MAX;
        flagV = ((R[Rn] ^ resultado) & (val ^ resultado)) >> 31;
        break;

    case 0x5:
        resultado = R[Rn] + val + Carry;
        flagC = ((uint64_t)R[Rn] + (uint64_t)val + Carry) > UINT32_MAX;
        flagV = ((R[Rn] ^ resultado) & (val ^ resultado)) >> 31;
        break;

    case 0x6:
        resultado = R[Rn] - val - !Carry;
        flagC = ((uint64_t)R[Rn] - (uint64_t)val - !Carry) <= UINT32_MAX;
        flagV = ((R[Rn] ^ val) & (R[Rn] ^ resultado)) >> 31;
        break;

    case 0x7:
        resultado = val - R[Rn] - !Carry;
        flagC = ((uint64_t)val - (uint64_t)R[Rn] - !Carry) <= UINT32_MAX;
        flagV = ((R[Rn] ^ val) & (val ^ resultado)) >> 31;
        break;

    case 0x8:
        resultado = R[Rn] & val;
        escrever_rd = false;
        break;

    case 0x9:
        resultado = R[Rn] ^ val;
        escrever_rd = false;
        break;

    case 0xA:
        resultado = R[Rn] - val;
        escrever_rd = false;
        flagC = (R[Rn] >= val);
        flagV = ((R[Rn] ^ val) & (R[Rn] ^ resultado)) >> 31;
        break;

    case 0xB:
        resultado = R[Rn] + val;
        flagC = ((uint64_t)R[Rn] + (uint64_t)val) > UINT32_MAX;
        flagV = ((R[Rn] ^ resultado) & (val ^ resultado)) >> 31;
        escrever_rd = false;
        break;

    case 0xC:
        resultado = R[Rn] | val;
        break;

    case 0xD:
        resultado = val;
        break;

    case 0xE:
        resultado = R[Rn] & (~val);
        break;

    case 0xF:
        resultado = ~val;
        break;

    default:
        break;
    }

    if (updateFlag)
    {
        flagN = resultado >> 31;
        flagZ = resultado == 0;
        set_bit(CPSR, 31, flagN);
        set_bit(CPSR, 30, flagZ);
        set_bit(CPSR, 29, flagC);
        set_bit(CPSR, 28, flagV);
    }

    if (escrever_rd)
    {
        R[Rd] = resultado;
    }
}
void Cpu::instrução_mul(uint32_t opcode)
{
    bool updateFlag = (opcode >> 20) & 0x1;

    uint8_t Rd = (opcode >> 16) & 0xF;
    uint8_t Rn = (opcode >> 12) & 0xF;
    uint8_t Rs = (opcode >> 8) & 0xF;
    uint8_t Rm = opcode & 0xF;
    uint8_t RdHigh = Rd;
    uint8_t RdLow = Rn;

    uint8_t mulType = (opcode >> 4) & 0xF;
    bool longFlag = (opcode >> 23) & 0x1;
    bool signedFlag = ((opcode >> 22) & 0x1);
    bool aculmulativo = (opcode >> 21) & 0x1;

    uint32_t resultado = 0;
    uint32_t resultadoLow = 0;
    if (longFlag)
    {
        if (RdHigh == RdLow || RdHigh == Rm || RdLow == Rm || RdHigh == 15 || RdLow == 15 || Rm == 15 || Rs == 15)
        {
            return;
        }

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
void Cpu::instrução_branch(uint32_t opcode)
{
    bool linkBranch = (opcode >> 24) & 0x1;

    int32_t offset = (static_cast<int32_t>(opcode & 0x00FFFFFF) << 8) >> 6;

    if (linkBranch)
    {
        R[14] = R[15] - 4;
    }
    R[15] = static_cast<uint32_t>(static_cast<int32_t>(R[15]) + offset);
}
void Cpu::instrução_bx(uint32_t opcode)
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
void Cpu::instrução_msr(uint32_t opcode)
{
    bool imediato = (opcode >> 25) & 0x1;

    bool spsrFlag = (opcode >> 22) & 0x1;
    uint32_t *endereçoPSR = &CPSR;
    uint32_t programState = 0;

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
    uint8_t modeBits = CPSR & 0x1F;

    if (modeBits == 0b10000)
    {
        mask &= 0xFF000000;
    }

    if (spsrFlag)
    {
        switch (modeBits)
        {
        case 0b10000:
        case 0b11111:
        case 0b10001:
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

        default:
            return;
        }
    }

    if (imediato)
    {
        uint32_t nn = opcode & 0xFF;
        uint8_t rorCount = ((opcode >> 8) & 0xF) * 2;

        programState = std::rotr(nn, rorCount);
    }
    else
    {
        uint8_t Rm = opcode & 0xF;
        programState = R[Rm];
    }

    *endereçoPSR = (*endereçoPSR & ~mask) | (programState & mask);
}
void Cpu::instrução_mrs(uint32_t opcode)
{
    uint8_t Rd = (opcode >> 12) & 0xF;

    bool spsrFlag = (opcode >> 22) & 0x1;
    uint32_t *endereçoPSR = &CPSR;

    if (spsrFlag)
    {
        uint8_t modeBits = CPSR & 0x1F;
        switch (modeBits)
        {
        case 0b10000:
        case 0b11111:
        case 0b10001:
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

        default:
            return;
        }
    }

    R[Rd] = *endereçoPSR;
}
void Cpu::instrução_sdt(uint32_t opcode)
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

        OpResult op2 = shift_barrier(shiftType, R[Rm], shiftCount, true, get_bit(CPSR, 29));

        offset = op2.valor;
    }
    else
    {
        offset = opcode & 0xFFF;
    }

    uint32_t endereço = R[Rn];
    if (preIndex)
    {
        endereço = (upDown) ? R[Rn] + offset : R[Rn] - offset;
    }

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
            status.valor = std::rotr(status.valor, desalinhamentoEmBits);
        }
        if (status.sucesso)
        {
            R[Rd] = status.valor;
        }
    }
    else
    {
        if (wordByte)
        {
            memoria->set8(endereço, R[Rd] & 0xFF);
        }
        else
        {
            memoria->set32(endereço, R[Rd]);
        }
    }

    if (writeBack || !preIndex)
    {
        if (!loadStore || (Rd != Rn))
        {
            R[Rn] = (upDown) ? R[Rn] + offset : R[Rn] - offset;
        }
    }
}
void Cpu::instrução_hw_s(uint32_t opcode)
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
    uint32_t endereço = R[Rn];

    if (imediato)
    {
        offset = R[Rm];
    }
    if (preIndex)
    {
        endereço = (upDown) ? R[Rn] + offset : R[Rn] - offset;
    }

    if (loadStore)
    {
        if (halfword)
        {
            OpResult status = memoria->get16(endereço);
            if (status.sucesso)
            {
                uint32_t resultado = status.valor;
                if (signed_)
                {
                    resultado |= (((get_bit(status.valor, 15)) ? 0xFFFF : 0x00) << 16);
                }
                R[Rd] = resultado;
            }
        }
        else
        {
            OpResult status = memoria->get8(endereço);
            if (status.sucesso)
            {
                uint32_t resultado = status.valor;
                if (signed_)
                {
                    resultado |= (((get_bit(status.valor, 7)) ? 0xFFFFFF : 0x00) << 8);
                }
                R[Rd] = resultado;
            }
        }
    }
    else
    {
        if (halfword)
        {
            memoria->set16(endereço, R[Rd] & 0xFFFF);
        }
        else
        {
            memoria->set8(endereço, R[Rd] & 0xFF);
        }
    }

    if (writeBack || !preIndex)
    {
        if (!loadStore || (Rd != Rn))
        {
            R[Rn] = (upDown) ? R[Rn] + offset : R[Rn] - offset;
        }
    }
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
    return SPSR_irq;
}
void Cpu::step()
{
    // 1. FETCH
    OpResult result = memoria->get32(R[15]);
    uint32_t opcode = result.result;
    uint32_t pc_atual = R[15];
    R[15] += 4; // O PC avança 4 bytes no estado ARM (32 bits)

    // 2. CHECK CONDITION (Bits 31..28)
    uint8_t cond = (opcode >> 28) & 0xF;
    if (false) // imagina que verifica se a função bate com a condição ou n
    {
        return; // Instrução descartada se a condição der falsa
    }

    // 3. DECODE & DISPATCH (Bits 27..25)
    uint8_t grupo = (opcode >> 25) & 0x7;

    switch (grupo)
    {
    case 0b000:
    {
        // Checa a assinatura dos bits 7 e 4 (Ambos precisam ser 1)
        if (((opcode >> 7) & 1) && ((opcode >> 4) & 1))
        {
            uint8_t sh = (opcode >> 5) & 0x3; // Bits 6..5 (S e H)

            if (sh != 0)
            {
                // [TransImm10 / TransReg10] Halfword e Signed Data Transfer
                // Instruções: LDRH, STRH, LDRSB, LDRSH
                // (Sua função: executar_ldr_str_extra)
            }
            else
            {
                // Bits 6..5 == 00 (Multiplicações, Swaps e Exclusivos)
                uint8_t op_mul = (opcode >> 20) & 0xFF; // Bits 27..20

                if ((op_mul & 0xFC) == 0x00)
                {
                    // [Multiply] MUL, MLA (Multiplicação simples de 32 bits)
                }
                else if (op_mul == 0x04)
                {
                    // [ARM11: UMAAL] Unsigned Multiply Accumulate Accumulate Long
                }
                else if ((op_mul & 0xF8) == 0x08)
                {
                    // [MulLong] UMULL, UMLAL, SMULL, SMLAL (Multiplicação de 64 bits)
                }
                else if ((op_mul & 0xFB) == 0x10 && ((opcode >> 4) & 0xF) == 0x9)
                {
                    // [TransSwp12] SWP, SWPB (Atomic Swap de memória)
                }
                else if ((op_mul & 0xF8) == 0x18 && ((opcode >> 4) & 0xF) == 0x9)
                {
                    // [ARM11: LDREX / STREX] Leitura/Escrita exclusiva
                }
                else
                {
                    // Opcode reservado / Não mapeado neste subgrupo
                }
            }
        }
        else
        {
            // Bits 7 e 4 NÃO são ambos '1'
            // Checa instruções de status, desvios por registrador e DSP
            uint8_t op_misc = (opcode >> 20) & 0x1F; // Bits 24..20

            if ((op_misc & 0x19) == 0x10) // Padrão bits 24..20 == 10x00
            {
                uint8_t bits_11_4 = (opcode >> 4) & 0xFF;

                if ((bits_11_4 & 0x0F) == 0x0)
                {
                    // [PSR Reg] MRS (Ler Status) ou MSR (Escrever Status via Registrador)
                }
                else if (bits_11_4 == 0x11)
                {
                    // [BX / BLX] Branch and Exchange (Troca para THUMB / Salto por registrador)
                }
                else if (bits_11_4 == 0x31)
                {
                    // [BLX] Branch with Link and Exchange (Registrador)
                }
                else if ((bits_11_4 & 0xF0) == 0x70 && (bits_11_4 & 0x0F) == 0x1)
                {
                    // [ARM9: BKPT] Breakpoint de Software
                }
                else if (bits_11_4 == 0x15)
                {
                    // [ARM9: CLZ] Count Leading Zeros
                }
                else if ((bits_11_4 & 0x0F) == 0x5)
                {
                    // [ARM9: QALU] Operações de Aritmética Saturada (QADD, QSUB, etc.)
                }
                else if ((bits_11_4 & 0x09) == 0x08)
                {
                    // [MulHalfARM9] Multiplicações DSP de 16 bits (SMLAxy, SMULxy)
                }
                else
                {
                    // [DataProc] ALU com operando vindo de registrador com Shift
                    // (Sua função: executar_alu)
                }
            }
            else
            {
                // [DataProc] ALU Padrão: AND, EOR, SUB, RSB, ADD, ADC, SBC, RSC, TST, TEQ, CMP, CMN, ORR, MOV, BIC, MVN
                // Operando 2 vem de Registrador (com deslocamento por constante ou registrador)
                // (Sua função: executar_alu)
            }
        }
        break;
    }

    case 0b001:
    {
        uint8_t op_misc = (opcode >> 20) & 0x1F; // Bits 24..20

        if (op_misc == 0x12 || op_misc == 0x16) // Bits 24..20 = 10010 ou 10110
        {
            // [PSR Imm] MSR (Escrever no PSR com valor Imediato rotacionado)
        }
        else if ((opcode & 0x0FFFFFFF) == 0x0320F000)
        {
            // [ARM11: Hint] NOP, YIELD, WFE, WFI, SEV
        }
        else
        {
            // [DataProc] ALU Padrão com Operando 2 Imediato de 8 bits rotacionado
            // (Sua função: executar_alu)
        }
        break;
    }

    case 0b010:
    {
        // [TransImm9] Single Data Transfer: LDR, STR, LDRB, STRB
        // Offset Imediato de 12 bits
        // (Sua função: executar_ldr_str_simples)
        break;
    }

    case 0b011:
    {
        bool bit4 = (opcode >> 4) & 1;
        if (!bit4)
        {
            // [TransReg9] Single Data Transfer: LDR, STR, LDRB, STRB
            // Offset de Registrador com Barrel Shifter
            // (Sua função: executar_ldr_str_simples)
        }
        else
        {
            // [Undefined / ARM11: Media] Instrução Indefinida na arquitetura ARMv4/v5
        }
        break;
    }

    case 0b100:
    {
        // [BlockTrans] Block Data Transfer: LDM, STM
        // Carregar / Salvar múltiplos registradores de uma vez
        break;
    }

    case 0b101:
    {
        // [B, BL, BLX] Desvios de código com Offset Imediato de 24 bits
        break;
    }

    case 0b110:
    {
        // [CoDataTrans / CoRR] Transferências de dados de Coprocessador (LDC, STC)
        break;
    }

    case 0b111:
    {
        bool bit24 = (opcode >> 24) & 1;
        if (bit24)
        {
            // [SWI / SVC] Software Interrupt (Interrupção de software / Call da BIOS)
        }
        else
        {
            bool bit4 = (opcode >> 4) & 1;
            if (!bit4)
            {
                // [CoDataOp] Operações Internas de Coprocessador (CDP)
            }
            else
            {
                // [CoRegTrans] Transferência Registrador-Coprocessador (MCR, MRC)
            }
        }
        break;
    }

    default:
    {
        std::cout << "[ERRO] Opcode nao reconhecido: 0x" << std::hex << opcode
                  << " no PC: 0x" << pc_atual << std::dec << "\n";
        break;
    }
    }
}