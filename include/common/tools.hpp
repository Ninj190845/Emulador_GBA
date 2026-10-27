#pragma once

#include <cstdint>

// Altera o bit de qualquer tipo numérico (uint8_t, uint16_t, uint32_t)
template <typename T>
inline void set_bit(T &n, uint8_t i, bool v)
{
    if (v)
    {
        n |= (static_cast<T>(1) << i); // Liga o bit (1)
    }
    else
    {
        n &= ~(static_cast<T>(1) << i); // Desliga o bit (0)
    }
}

// Le o bit de qualquer tipo numérico
template <typename T>
inline bool get_bit(T n, uint8_t i)
{
    return (n >> i) & 1;
}