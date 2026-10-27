#pragma once

#include <cstdint>

enum class StCode : uint8_t
{
    SUCESS = 0,
    FAIL = 1
};

struct OpResult
{
    uint32_t result = 0;
    uint32_t ciclos = 0;
    StCode status_code = StCode::FAIL; // Nome esperado pelo ARMv4T.cpp
    uint32_t extra = 0;                // Usado para carregar a flag Carry/informação extra nos shifts
};