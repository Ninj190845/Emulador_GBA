#include "dma.hpp"
#include "memoria.hpp"

void Dma::check_dma_write(uint32_t endereco, uint16_t valor)
{
    // Registradores DMAxCNT_H ficam nos endereços:
    // DMA0CNT_H: 0x040000BA
    // DMA1CNT_H: 0x040000C6
    // DMA2CNT_H: 0x040000D2
    // DMA3CNT_H: 0x040000DE
    if (endereco == 0x040000BA || endereco == 0x040000C6 ||
        endereco == 0x040000D2 || endereco == 0x040000DE)
    {
        int canal = (endereco - 0x040000BA) / 0x0C;

        // Se o bit 15 (Enable) foi ativado
        if (valor & (1 << 15))
        {
            uint8_t timing = (valor >> 12) & 0x03;

            // Timing 0 = Imediato: Roda no mesmo instante!
            if (timing == static_cast<uint8_t>(DmaTiming::Immediate))
            {
                transferir_canal(canal);
            }
        }
    }
}

void Dma::trigger(DmaTiming timing)
{
    // Checa os 4 canais em ordem de prioridade (DMA0 -> DMA3)
    for (int canal = 0; canal < 4; canal++)
    {
        uint32_t base = 0x040000B0 + (canal * 0x0C);
        uint16_t cnt_h = *reinterpret_cast<uint16_t *>(&memoria->ior[base + 0x0A - 0x04000000]);

        bool enable = (cnt_h & (1 << 15)) != 0;
        uint8_t dma_timing = (cnt_h >> 12) & 0x03;

        if (enable && dma_timing == static_cast<uint8_t>(timing))
        {
            transferir_canal(canal);
        }
    }
}

void Dma::transferir_canal(int canal)
{
    uint32_t base = 0x040000B0 + (canal * 0x0C);

    // Lê registradores direto da IOR
    uint32_t src = *reinterpret_cast<uint32_t *>(&memoria->ior[base + 0x00 - 0x04000000]);
    uint32_t dst = *reinterpret_cast<uint32_t *>(&memoria->ior[base + 0x04 - 0x04000000]);
    uint16_t count = *reinterpret_cast<uint16_t *>(&memoria->ior[base + 0x08 - 0x04000000]);
    uint16_t cnt_h = *reinterpret_cast<uint16_t *>(&memoria->ior[base + 0x0A - 0x04000000]);

    if (count == 0)
        count = (canal == 3) ? 0x10000 : 0x4000;

    bool is_32bit = (cnt_h & (1 << 10)) != 0;
    int step_bytes = is_32bit ? 4 : 2;

    int src_control = (cnt_h >> 7) & 0x03;
    int dst_control = (cnt_h >> 5) & 0x03;

    // Loop de transferência rápida usando a sua Memória
    for (uint32_t i = 0; i < count; i++)
    {
        if (is_32bit)
        {
            memoria->set32(dst, memoria->get32(src).result);
        }
        else
        {
            memoria->set16(dst, memoria->get16(src).result);
        }

        if (src_control == 0)
            src += step_bytes;
        else if (src_control == 1)
            src -= step_bytes;

        if (dst_control == 0 || dst_control == 3)
            dst += step_bytes;
        else if (dst_control == 1)
            dst -= step_bytes;
    }

    // Se o bit 9 (Repeat) não estiver ativo, desliga o DMA após concluir
    if (!(cnt_h & (1 << 9)))
    {
        cnt_h &= ~(1 << 15);
        *reinterpret_cast<uint16_t *>(&memoria->ior[base + 0x0A - 0x04000000]) = cnt_h;
    }

    // Substitua o TODO do final por isso:
    if (cnt_h & (1 << 14)) // Bit 14 = Enable IRQ
    {
        uint16_t *IF = reinterpret_cast<uint16_t *>(&memoria->ior[0x0202 - 0x04000000]);
        *IF |= (1 << (8 + canal)); // Bits 8, 9, 10 ou 11 no registrador IF
    }
}