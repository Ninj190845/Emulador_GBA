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

    OpResult shift_barrier(uint32_t shiftType, uint32_t valor, uint8_t count, bool origemImediato, bool flagC);
    bool checar_condicao(uint32_t opcode);

    void alu(uint32_t opcode);
    void mul(uint32_t opcode);
    void branch(uint32_t opcode);
    void blx(uint32_t opcode);
    void msr(uint32_t opcode);
    void mrs(uint32_t opcode);
    void sdt(uint32_t opcode);
    void hw_s(uint32_t opcode);
    void bdt(uint32_t opcode);
    void swp(uint32_t opcode);

public:
    Cpu(Mem *mem);
    ~Cpu() = default;

    void set_r(uint8_t index, uint32_t value);
    uint32_t get_r(uint8_t index);

    void step();
};