#pragma once

#include <cstdint>
#include "memoria.hpp"
#include "common/structs.hpp"

enum class CpuMode : uint8_t
{
    User = 0b10000,       // 0x10 - Modo normal do jogo
    FIQ = 0b10001,        // 0x11 - Fast Interrupt (raro no GBA)
    IRQ = 0b10010,        // 0x12 - Interrupções de hardware (VBlank, Timers, etc.)
    Supervisor = 0b10011, // 0x13 - Modo SVC (gerado pela instrução SWI)
    Abort = 0b10111,      // 0x17 - Erro de acesso à memória
    Undefined = 0b11011,  // 0x1B - Instrução inválida
    System = 0b11111      // 0x1F - Modo privilegiado (mesmos regs do User)
};

enum class IorAddr : uint32_t
{
    IE = 0x04000200,
    IF = 0x04000202,
    IME = 0x04000208
};

class Cpu
{
private:
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

    uint32_t *regMap[16] = {nullptr};
    uint32_t *SPSRatual;

    Mem *memoria = nullptr;

    bool pc_alterado = false;

    void set_reg(uint8_t index, uint32_t valor);
    uint32_t get_reg(uint8_t index);

    OpResult shift_barrier(uint32_t shiftType, uint32_t valor, uint8_t count, bool origemImediato, bool flagC);
    bool checar_condicao(uint32_t opcode);

    uint32_t bios_latch = 0xE12FFF1E; // Valor padrão de fallback (BX LR)

    // Wrappers de leitura com proteção anti-dump
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
    void thumb_format_17(uint16_t opcode);
    void thumb_format_18(uint16_t opcode);
    void thumb_format_19(uint16_t opcode);
    void step_thumb();

    void interrupt(CpuMode mode, uint32_t jumpAddr);

public:
    Cpu(Mem *mem);
    ~Cpu() = default;

    void set_r(uint8_t index, uint32_t value);
    uint32_t get_r(uint8_t index);

    void step();
};