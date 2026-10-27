#include "ppu.hpp"
#include <cstdint>
#include <vector>
#include <algorithm>

using uint = unsigned int;

// ============================================================================
// MÉTODOS AUXILIARES DE LEITURA I/O DA MEMÓRIA
// ============================================================================
uint16_t Ppu::read_io16(uint32_t addr)
{
    return *reinterpret_cast<uint16_t *>(&memoria->ior[addr - 0x04000000]);
}

uint32_t Ppu::read_io32(uint32_t addr)
{
    return *reinterpret_cast<uint32_t *>(&memoria->ior[addr - 0x04000000]);
}

// ============================================================================
// RENDERIZADOR DE BACKGROUND DE TEXTO (Modos 0 e 1)
// ============================================================================
void Ppu::render_text_bg(int bg, uint32_t line_buffer[240])
{
    uint16_t bgcnt = read_io16(0x04000008 + bg * 2);
    uint16_t hofs = read_io16(0x04000010 + bg * 4) & 0x01FF;
    uint16_t vofs = read_io16(0x04000012 + bg * 4) & 0x01FF;

    uint32_t char_base = ((bgcnt >> 2) & 0x03) * 0x4000;
    bool is_8bpp = (bgcnt & (1 << 7)) != 0;
    uint32_t screen_base = ((bgcnt >> 8) & 0x1F) * 0x0800;
    uint8_t screen_size = (bgcnt >> 14) & 0x03;

    int map_w = (screen_size & 1) ? 512 : 256;
    int map_h = (screen_size & 2) ? 512 : 256;

    uint16_t *pram = reinterpret_cast<uint16_t *>(memoria->palleteRam);

    for (int x = 0; x < 240; x++)
    {
        int px = (x + hofs) % map_w;
        int py = (yPointer + vofs) % map_h;

        int sb_index = 0;
        if (screen_size == 1)
            sb_index = (px >= 256) ? 1 : 0;
        else if (screen_size == 2)
            sb_index = (py >= 256) ? 1 : 0;
        else if (screen_size == 3)
            sb_index = ((py >= 256) ? 2 : 0) + ((px >= 256) ? 1 : 0);

        int local_tx = (px % 256) / 8;
        int local_ty = (py % 256) / 8;

        uint32_t map_addr = (screen_base + sb_index * 0x0800) + (local_ty * 32 + local_tx) * 2;
        uint16_t tile_entry = *reinterpret_cast<uint16_t *>(&memoria->vram[map_addr]);

        uint16_t tile_id = tile_entry & 0x03FF;
        bool flip_h = (tile_entry & (1 << 10)) != 0;
        bool flip_v = (tile_entry & (1 << 11)) != 0;
        uint8_t pal_bank = (tile_entry >> 12) & 0x0F;

        int tx = px % 8;
        int ty = py % 8;
        if (flip_h)
            tx = 7 - tx;
        if (flip_v)
            ty = 7 - ty;

        uint8_t color_idx = 0;

        if (!is_8bpp)
        {
            uint32_t tile_addr = char_base + (tile_id * 32) + (ty * 4) + (tx / 2);
            uint8_t byte_val = memoria->vram[tile_addr];
            color_idx = (tx % 2 == 0) ? (byte_val & 0x0F) : (byte_val >> 4);
        }
        else
        {
            uint32_t tile_addr = char_base + (tile_id * 64) + (ty * 8) + tx;
            color_idx = memoria->vram[tile_addr];
        }

        if (color_idx == 0)
            continue; // Transparente

        uint16_t color16 = is_8bpp ? pram[color_idx] : pram[(pal_bank * 16) + color_idx];

        uint8_t r = (color16 & 0x1F) << 3;
        uint8_t g = ((color16 >> 5) & 0x1F) << 3;
        uint8_t b = ((color16 >> 10) & 0x1F) << 3;

        line_buffer[x] = (0xFF000000) | (r << 16) | (g << 8) | b;
    }
}

// ============================================================================
// RENDERIZADOR DE BACKGROUND AFINE (Modos 1 e 2 - Rotação e Escala)
// ============================================================================
void Ppu::render_affine_bg(int bg, uint32_t line_buffer[240])
{
    uint16_t bgcnt = read_io16(0x04000008 + bg * 2);

    uint32_t char_base = ((bgcnt >> 2) & 0x03) * 0x4000;
    uint32_t screen_base = ((bgcnt >> 8) & 0x1F) * 0x0800;
    bool wrap = (bgcnt & (1 << 13)) != 0;
    uint8_t screen_size = (bgcnt >> 14) & 0x03;

    int map_dim = 128 << screen_size;
    int num_tiles_side = map_dim / 8;

    int16_t pa = read_io16(0x04000020 + (bg - 2) * 0x10);
    int16_t pb = read_io16(0x04000022 + (bg - 2) * 0x10);
    int16_t pc = read_io16(0x04000024 + (bg - 2) * 0x10);
    int16_t pd = read_io16(0x04000026 + (bg - 2) * 0x10);

    int32_t ref_x = read_io32(0x04000028 + (bg - 2) * 0x10);
    int32_t ref_y = read_io32(0x0400002C + (bg - 2) * 0x10);

    int32_t start_x = ref_x + (pb * yPointer);
    int32_t start_y = ref_y + (pd * yPointer);

    uint16_t *pram = reinterpret_cast<uint16_t *>(memoria->palleteRam);

    for (int x = 0; x < 240; x++)
    {
        int32_t curr_x = (start_x + (pa * x)) >> 8;
        int32_t curr_y = (start_y + (pc * x)) >> 8;

        if (wrap)
        {
            curr_x = (curr_x % map_dim + map_dim) % map_dim;
            curr_y = (curr_y % map_dim + map_dim) % map_dim;
        }
        else if (curr_x < 0 || curr_x >= map_dim || curr_y < 0 || curr_y >= map_dim)
        {
            continue;
        }

        int tile_x = curr_x / 8;
        int tile_y = curr_y / 8;

        uint32_t map_addr = screen_base + (tile_y * num_tiles_side) + tile_x;
        uint8_t tile_id = memoria->vram[map_addr];

        int tx = curr_x % 8;
        int ty = curr_y % 8;

        uint32_t tile_addr = char_base + (tile_id * 64) + (ty * 8) + tx;
        uint8_t color_idx = memoria->vram[tile_addr];

        if (color_idx == 0)
            continue;

        uint16_t color16 = pram[color_idx];

        uint8_t r = (color16 & 0x1F) << 3;
        uint8_t g = ((color16 >> 5) & 0x1F) << 3;
        uint8_t b = ((color16 >> 10) & 0x1F) << 3;

        line_buffer[x] = (0xFF000000) | (r << 16) | (g << 8) | b;
    }
}

// ============================================================================
// MODOS DE TILE (0, 1, 2)
// ============================================================================
struct LayerRef
{
    int bg_index;
    int priority;
};

void Ppu::render_mode0()
{
    uint16_t dispcnt = read_io16(0x04000000);
    uint16_t bg_col = *reinterpret_cast<uint16_t *>(memoria->palleteRam);
    uint32_t backdrop = 0xFF000000 | ((bg_col & 0x1F) << 19) | (((bg_col >> 5) & 0x1F) << 11) | (((bg_col >> 10) & 0x1F) << 3);

    uint32_t line_buffer[240];
    std::fill_n(line_buffer, 240, backdrop);

    std::vector<LayerRef> layers;
    for (int bg = 0; bg < 4; bg++)
    {
        if (dispcnt & (1 << (8 + bg)))
        {
            uint16_t bgcnt = read_io16(0x04000008 + bg * 2);
            layers.push_back({bg, bgcnt & 0x03});
        }
    }

    std::stable_sort(layers.begin(), layers.end(), [](const LayerRef &a, const LayerRef &b)
                     { return a.priority > b.priority; });

    for (const auto &layer : layers)
    {
        render_text_bg(layer.bg_index, line_buffer);
    }

    std::copy_n(line_buffer, 240, &frameBuffer[yPointer * 240]);
}

void Ppu::render_mode1()
{
    uint16_t dispcnt = read_io16(0x04000000);
    uint16_t bg_col = *reinterpret_cast<uint16_t *>(memoria->palleteRam);
    uint32_t backdrop = 0xFF000000 | ((bg_col & 0x1F) << 19) | (((bg_col >> 5) & 0x1F) << 11) | (((bg_col >> 10) & 0x1F) << 3);

    uint32_t line_buffer[240];
    std::fill_n(line_buffer, 240, backdrop);

    std::vector<LayerRef> layers;
    for (int bg = 0; bg < 3; bg++)
    {
        if (dispcnt & (1 << (8 + bg)))
        {
            uint16_t bgcnt = read_io16(0x04000008 + bg * 2);
            layers.push_back({bg, bgcnt & 0x03});
        }
    }

    std::stable_sort(layers.begin(), layers.end(), [](const LayerRef &a, const LayerRef &b)
                     { return a.priority > b.priority; });

    for (const auto &layer : layers)
    {
        if (layer.bg_index == 2)
            render_affine_bg(2, line_buffer);
        else
            render_text_bg(layer.bg_index, line_buffer);
    }

    std::copy_n(line_buffer, 240, &frameBuffer[yPointer * 240]);
}

void Ppu::render_mode2()
{
    uint16_t dispcnt = read_io16(0x04000000);
    uint16_t bg_col = *reinterpret_cast<uint16_t *>(memoria->palleteRam);
    uint32_t backdrop = 0xFF000000 | ((bg_col & 0x1F) << 19) | (((bg_col >> 5) & 0x1F) << 11) | (((bg_col >> 10) & 0x1F) << 3);

    uint32_t line_buffer[240];
    std::fill_n(line_buffer, 240, backdrop);

    std::vector<LayerRef> layers;
    for (int bg = 2; bg <= 3; bg++)
    {
        if (dispcnt & (1 << (8 + bg)))
        {
            uint16_t bgcnt = read_io16(0x04000008 + bg * 2);
            layers.push_back({bg, bgcnt & 0x03});
        }
    }

    std::stable_sort(layers.begin(), layers.end(), [](const LayerRef &a, const LayerRef &b)
                     { return a.priority > b.priority; });

    for (const auto &layer : layers)
    {
        render_affine_bg(layer.bg_index, line_buffer);
    }

    std::copy_n(line_buffer, 240, &frameBuffer[yPointer * 240]);
}

// ============================================================================
// MODOS BITMAP (3, 4, 5)
// ============================================================================
void Ppu::render_mode3()
{
    uint16_t *vram16 = reinterpret_cast<uint16_t *>(memoria->vram);
    int base = yPointer * 240;

    for (int x = 0; x < 240; x++)
    {
        int index = base + x;
        uint16_t color16 = vram16[index];

        uint8_t r = (color16 & 0x1F) << 3;
        uint8_t g = ((color16 >> 5) & 0x1F) << 3;
        uint8_t b = ((color16 >> 10) & 0x1F) << 3;

        frameBuffer[index] = (0xFF000000) | (r << 16) | (g << 8) | b;
    }
}

void Ppu::render_mode4()
{
    uint16_t dispcnt = read_io16(0x04000000);
    uint32_t frame_offset = (dispcnt & (1 << 4)) ? 0xA000 : 0x0000;

    uint8_t *vram8 = memoria->vram + frame_offset;
    uint16_t *pram = reinterpret_cast<uint16_t *>(memoria->palleteRam);

    int base = yPointer * 240;

    for (int x = 0; x < 240; x++)
    {
        int index = base + x;
        uint8_t palette_index = vram8[index];
        uint16_t color16 = pram[palette_index];

        uint8_t r = (color16 & 0x1F) << 3;
        uint8_t g = ((color16 >> 5) & 0x1F) << 3;
        uint8_t b = ((color16 >> 10) & 0x1F) << 3;

        frameBuffer[index] = (0xFF000000) | (r << 16) | (g << 8) | b;
    }
}

void Ppu::render_mode5()
{
    uint16_t dispcnt = read_io16(0x04000000);
    uint32_t frame_offset = (dispcnt & (1 << 4)) ? 0xA000 : 0x0000;
    uint16_t *vram16 = reinterpret_cast<uint16_t *>(memoria->vram + frame_offset);
    int fb_base = yPointer * 240;

    for (int x = 0; x < 240; x++)
    {
        int fb_index = fb_base + x;

        if (yPointer < 128 && x < 160)
        {
            int vram_index = (yPointer * 160) + x;
            uint16_t color16 = vram16[vram_index];

            uint8_t r = (color16 & 0x1F) << 3;
            uint8_t g = ((color16 >> 5) & 0x1F) << 3;
            uint8_t b = ((color16 >> 10) & 0x1F) << 3;

            frameBuffer[fb_index] = (0xFF000000) | (r << 16) | (g << 8) | b;
        }
        else
        {
            frameBuffer[fb_index] = 0xFF000000;
        }
    }
}

// ============================================================================
// RENDERIZADOR DE SPRITES (OBJ)
// ============================================================================
void Ppu::render_sprites()
{
    uint16_t dispcnt = read_io16(0x04000000);
    if (!(dispcnt & (1 << 12)))
        return;

    uint16_t *oam = reinterpret_cast<uint16_t *>(memoria->oam);
    uint16_t *pram_obj = reinterpret_cast<uint16_t *>(memoria->palleteRam + 0x200);

    static const int sprite_sizes[3][4][2] = {
        {{8, 8}, {16, 16}, {32, 32}, {64, 64}},
        {{16, 8}, {32, 8}, {32, 16}, {64, 32}},
        {{8, 16}, {8, 32}, {16, 32}, {32, 64}}};

    bool is_1d_mapping = (dispcnt & (1 << 6)) != 0;

    for (int i = 127; i >= 0; i--)
    {
        uint16_t attr0 = oam[i * 4 + 0];
        uint16_t attr1 = oam[i * 4 + 1];
        uint16_t attr2 = oam[i * 4 + 2];

        if (((attr0 >> 8) & 0x03) == 2)
            continue;

        int y = attr0 & 0xFF;
        uint8_t shape = (attr0 >> 14) & 0x03;
        bool is_8bpp = (attr0 & (1 << 13)) != 0;

        int x = attr1 & 0x01FF;
        if (x >= 240)
            x -= 512;

        bool flip_h = (attr1 & (1 << 12)) != 0;
        bool flip_v = (attr1 & (1 << 13)) != 0;
        uint8_t size = (attr1 >> 14) & 0x03;

        if (shape == 3)
            continue;

        int width = sprite_sizes[shape][size][0];
        int height = sprite_sizes[shape][size][1];

        if (y >= 160)
            y -= 256;
        if (yPointer < y || yPointer >= y + height)
            continue;

        uint16_t tile_id = attr2 & 0x03FF;
        uint8_t pal_bank = (attr2 >> 12) & 0x0F;

        int py = yPointer - y;
        if (flip_v)
            py = height - 1 - py;

        for (int px_offset = 0; px_offset < width; px_offset++)
        {
            int screen_x = x + px_offset;
            if (screen_x < 0 || screen_x >= 240)
                continue;

            int px = flip_h ? (width - 1 - px_offset) : px_offset;

            uint32_t tile_x = px / 8;
            uint32_t tile_y = py / 8;
            uint32_t current_tile = 0;

            if (is_1d_mapping)
            {
                int tiles_per_row = is_8bpp ? (width / 4) : (width / 8);
                current_tile = tile_id + (tile_y * tiles_per_row) + (is_8bpp ? tile_x * 2 : tile_x);
            }
            else
            {
                current_tile = (tile_id & ~1) + (tile_y * 32) + (is_8bpp ? tile_x * 2 : tile_x);
            }

            uint32_t vram_sprite_base = 0x10000;
            int sub_x = px % 8;
            int sub_y = py % 8;

            uint8_t color_idx = 0;

            if (!is_8bpp)
            {
                uint32_t addr = vram_sprite_base + (current_tile * 32) + (sub_y * 4) + (sub_x / 2);
                uint8_t byte_val = memoria->vram[addr];
                color_idx = (sub_x % 2 == 0) ? (byte_val & 0x0F) : (byte_val >> 4);
            }
            else
            {
                uint32_t addr = vram_sprite_base + (current_tile * 32) + (sub_y * 8) + sub_x;
                color_idx = memoria->vram[addr];
            }

            if (color_idx == 0)
                continue;

            uint16_t color16 = is_8bpp ? pram_obj[color_idx] : pram_obj[(pal_bank * 16) + color_idx];

            uint8_t r = (color16 & 0x1F) << 3;
            uint8_t g = ((color16 >> 5) & 0x1F) << 3;
            uint8_t b = ((color16 >> 10) & 0x1F) << 3;

            frameBuffer[(yPointer * 240) + screen_x] = (0xFF000000) | (r << 16) | (g << 8) | b;
        }
    }
}

// ============================================================================
// SCANLINE DISPATCHER PRINCIPAL
// ============================================================================
void Ppu::render_scanline()
{
    uint16_t dispcnt = read_io16(0x04000000);

    // Forced Blank
    if (dispcnt & (1 << 7))
    {
        int base = yPointer * 240;
        for (int x = 0; x < 240; x++)
            frameBuffer[base + x] = 0xFFFFFFFF;
        return;
    }

    uint8_t mode = dispcnt & 0x07;

    switch (mode)
    {
    case 0:
        render_mode0();
        break;
    case 1:
        render_mode1();
        break;
    case 2:
        render_mode2();
        break;
    case 3:
        render_mode3();
        break;
    case 4:
        render_mode4();
        break;
    case 5:
        render_mode5();
        break;
    default:
        break;
    }

    // Desenha os Sprites por cima da camada de fundo
    render_sprites();
}

// ============================================================================
// PASSO TEMPORAL DA PPU
// ============================================================================
void Ppu::step(uint ciclos)
{
    ciclos_acumulados += ciclos;

    // HDRAW -> HBLANK (960 ciclos)
    if (!pixelNulo && ciclos_acumulados >= 960)
    {
        pixelNulo = true;

        if (yPointer < 160)
        {
            render_scanline();
        }
    }

    // Fim da Scanline (1232 ciclos)
    if (ciclos_acumulados >= 1232)
    {
        ciclos_acumulados -= 1232;
        pixelNulo = false;
        yPointer++;

        if (yPointer >= 228)
        {
            yPointer = 0;
        }

        // Sincroniza VCOUNT direto na IOR no endereço 0x04000006
        *reinterpret_cast<uint16_t *>(&memoria->ior[0x06]) = yPointer;
    }
}