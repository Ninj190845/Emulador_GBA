#pragma once

#include <cstdint>
#include "memoria.hpp"
#include "common/structs.hpp"

class Cpu
{
private:
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

    Mem *memoria = nullptr;

    bool pc_alterado = false;

    void set_reg(uint8_t index, uint32_t valor);
    uint32_t get_reg(uint8_t index);

    OpResult shift_barrier(uint32_t shiftType, uint32_t valor, uint8_t count, bool origemImediato, bool flagC);
    bool checar_condicao(uint32_t opcode);

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

public:
    Cpu(Mem *mem);
    ~Cpu() = default;

    void set_r(uint8_t index, uint32_t value);
    uint32_t get_r(uint8_t index);

    void step();
};