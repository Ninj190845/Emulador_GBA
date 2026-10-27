#pragma once

#include <cstdint>
#include "memoria.hpp"

#include "common/structs.hpp"
#include "common/config.hpp"
#include "common/tools.hpp"

enum class CpuMode : uint8_t
{
    User = 0b10000,       // 0x10 - Modo normal do jogo
    FIQ = 0b10001,        // 0x11 - Fast Interrupt
    IRQ = 0b10010,        // 0x12 - Interrupções de hardware
    Supervisor = 0b10011, // 0x13 - Modo SVC (gerado por SWI)
    Abort = 0b10111,      // 0x17 - Erro de acesso à memória
    Undefined = 0b11011,  // 0x1B - Instrução inválida
    System = 0b11111      // 0x1F - Modo privilegiado
};

enum class IorAddr : uint32_t
{
    IE = 0x04000200,
    IF = 0x04000202,
    IME = 0x04000208
};

union Psr
{
    uint32_t val = 0;

    struct
    {
        uint32_t mode : 5;        // Bits 0..4: Modo da CPU
        uint32_t thumb : 1;       // Bit 5: Estado Thumb (0 = ARM, 1 = Thumb)
        uint32_t fiq_disable : 1; // Bit 6: Desabilita FIQ
        uint32_t irq_disable : 1; // Bit 7: Desabilita IRQ
        uint32_t reserved : 20;   // Bits 8..27
        uint32_t v : 1;           // Bit 28: Overflow
        uint32_t c : 1;           // Bit 29: Carry
        uint32_t z : 1;           // Bit 30: Zero
        uint32_t n : 1;           // Bit 31: Negative
    };
};

struct Cpu
{
    uint32_t R[16] = {0};
    Psr CPSR;

    uint32_t *regMap[16] = {nullptr};
    Psr *SPSRatual = nullptr;

    uint32_t R8_fiq[7] = {0};
    Psr SPSR_fiq;

    uint32_t R13_svc[2] = {0};
    Psr SPSR_svc;

    uint32_t R13_abt[2] = {0};
    Psr SPSR_abt;

    uint32_t R13_irq[2] = {0};
    Psr SPSR_irq;

    uint32_t R13_und[2] = {0};
    Psr SPSR_und;

    Mem *memoria = nullptr;

    bool pc_alterado = false;

    void set_reg(uint8_t index, uint32_t valor);
    uint32_t get_reg(uint8_t index);

    OpResult shift_barrier(uint32_t shiftType, uint32_t valor, uint8_t count, bool origemImediato, bool flagC);
    bool checar_condicao(uint32_t opcode);

    uint32_t biosLatch = 0xE12FFF1E;

    uint32_t cpu_read32(uint32_t addr);
    uint16_t cpu_read16(uint32_t addr);
    uint8_t cpu_read8(uint32_t addr);

    void update_reg_map();

    void arm_alu(uint32_t opcode);
    void arm_mul(uint32_t opcode);
    void arm_branch(uint32_t opcode);
    void arm_bx(uint32_t opcode);
    void arm_msr(uint32_t opcode);
    void arm_mrs(uint32_t opcode);
    void arm_sdt(uint32_t opcode);
    void arm_hw_s(uint32_t opcode);
    void arm_bdt(uint32_t opcode);
    void arm_swp(uint32_t opcode);
    void step_arm();

    void thumb_format_1(uint16_t opcode);
    void thumb_format_2(uint16_t opcode);
    void thumb_format_3(uint16_t opcode);
    void thumb_format_4(uint16_t opcode);
    void thumb_format_5(uint16_t opcode);
    void thumb_format_6(uint16_t opcode);
    void thumb_format_7(uint16_t opcode);
    void thumb_format_8(uint16_t opcode);
    void thumb_format_9(uint16_t opcode);
    void thumb_format_10(uint16_t opcode);
    void thumb_format_11(uint16_t opcode);
    void thumb_format_12(uint16_t opcode);
    void thumb_format_13(uint16_t opcode);
    void thumb_format_14(uint16_t opcode);
    void thumb_format_15(uint16_t opcode);
    void thumb_format_16(uint16_t opcode);
    void thumb_format_17();
    void thumb_format_18(uint16_t opcode);
    void thumb_format_19(uint16_t opcode);
    void step_thumb();

    void interrupt(CpuMode mode, uint32_t jumpAddr);

    Cpu(Mem *mem);
    ~Cpu() = default;

    void reset();

    void set_r(uint8_t index, uint32_t value);
    uint32_t get_r(uint8_t index);

    // Retorna a quantidade de ciclos gastos na execução
    uint32_t step();
};