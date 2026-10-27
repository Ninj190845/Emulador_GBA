#pragma once

#include <cstdint>
#include "memoria.hpp"

class Timers
{
private:
    Mem *memoria = nullptr;

    struct Channel
    {
        uint16_t reload = 0;
        uint16_t control = 0;
        uint32_t counter = 0; // 32 bits para facilitar o calculo do overflow
        uint32_t ticks = 0;   // Acumulador de ciclos do prescaler
    } channels[4];

    // Divisores de frequencia: 1, 64, 256, 1024 ciclos
    const uint32_t prescalers[4] = {1, 64, 256, 1024};

public:
    Timers(Mem *mem) : memoria(mem) {}

    void step(uint ciclos)
    {
        for (int i = 0; i < 4; i++)
        {
            uint16_t cnt_h = *reinterpret_cast<uint16_t *>(&memoria->ior[0x0102 + i * 4]);
            bool enable = (cnt_h & (1 << 7)) != 0;
            bool cascade = (cnt_h & (1 << 2)) != 0;

            if (!enable)
                continue;

            // Em modo Cascade, o timer i so avanca se o timer i-1 estourou
            if (cascade && i > 0)
                continue;

            uint32_t prescaler = prescalers[cnt_h & 0x03];
            channels[i].ticks += ciclos;

            while (channels[i].ticks >= prescaler)
            {
                channels[i].ticks -= prescaler;

                // Lê o valor atual gravado no registrador CNT_L
                uint16_t cnt_l = *reinterpret_cast<uint16_t *>(&memoria->ior[0x0100 + i * 4]);
                uint32_t novo_val = cnt_l + 1;

                if (novo_val > 0xFFFF) // Overflow!
                {
                    uint16_t reload = cnt_l; // No overflow, recarrega o valor base
                    *reinterpret_cast<uint16_t *>(&memoria->ior[0x0100 + i * 4]) = reload;

                    // Se IRQ estiver habilitado (bit 6)
                    if (cnt_h & (1 << 6))
                    {
                        uint16_t *IF = reinterpret_cast<uint16_t *>(&memoria->ior[0x0202]);
                        *IF |= (1 << (3 + i)); // IRQs dos timers sao os bits 3, 4, 5 e 6
                    }

                    // Se o proximo timer estiver em modo Cascade, incrementa ele
                    if (i < 3)
                    {
                        uint16_t next_cnt_h = *reinterpret_cast<uint16_t *>(&memoria->ior[0x0102 + (i + 1) * 4]);
                        if ((next_cnt_h & (1 << 7)) && (next_cnt_h & (1 << 2)))
                        {
                            uint16_t *next_cnt_l = reinterpret_cast<uint16_t *>(&memoria->ior[0x0100 + (i + 1) * 4]);
                            (*next_cnt_l)++;
                        }
                    }
                }
                else
                {
                    *reinterpret_cast<uint16_t *>(&memoria->ior[0x0100 + i * 4]) = static_cast<uint16_t>(novo_val);
                }
            }
        }
    }
};