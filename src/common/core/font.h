#pragma once
#include <d3d11.h>
#include <vector>
#include <cstdint>

class BitmapFont {
public:
    static const int GLYPH_WIDTH = 8;
    static const int GLYPH_HEIGHT = 8;
    static const int GLYPHS_PER_ROW = 16;
    static const int GLYPH_ROWS = 16;
    static const int TEXTURE_WIDTH = GLYPH_WIDTH * GLYPHS_PER_ROW;
    static const int TEXTURE_HEIGHT = GLYPH_HEIGHT * GLYPH_ROWS;

    static std::vector<uint8_t> generate_font_texture();
    static void get_char_uv(char c, float& u1, float& v1, float& u2, float& v2);
    static bool create_texture(ID3D11Device* device, ID3D11ShaderResourceView** outView);
};