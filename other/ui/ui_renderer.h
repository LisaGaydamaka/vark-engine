#pragma once
#include <windows.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <vector>
#include <algorithm>

using Microsoft::WRL::ComPtr;

class Renderer;

class UIRenderer
{
public:
    bool initialize(Renderer* renderer, int width, int height);
    void shutdown();
    void resize(int width, int height);

    // ---- Frame management ----
    void begin_frame();
    void end_frame();

    // ---- Drawing (accumulated) ----
    void draw_rect(float x, float y, float w, float h, float r, float g, float b, float a);
    void draw_text(float x, float y, const char* text, float r, float g, float b, float a);

    float get_text_width(const char* text) const;

    // ---- Clipping (flushes current batch) ----
    void push_clip_rect(float x, float y, float w, float h);
    void pop_clip_rect();

    int get_width() const { return m_width; }
    int get_height() const { return m_height; }

private:
    // ---- Vertex structures ----
    struct RectVertex { float x, y; float r, g, b, a; };
    struct TextVertex { float x, y, u, v; float r, g, b, a; };

    // ---- Internal helpers ----
    void flush_rects();
    void flush_text();
    void restore_engine_pipeline();
    bool create_font_resources();
    void create_buffers();
    void ensure_rect_buffer_capacity(size_t count);
    void ensure_text_buffer_capacity(size_t count);

    Renderer* m_renderer = nullptr;
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;

    int m_width = 0;
    int m_height = 0;
    bool m_initialized = false;
    bool m_fontReady = false;

    // ---- Batched vertex buffers ----
    std::vector<RectVertex> m_rectVertices;
    std::vector<TextVertex> m_textVertices;

    // ---- D3D resources ----
    ComPtr<ID3D11Buffer> m_rectVertexBuffer;
    ComPtr<ID3D11Buffer> m_rectIndexBuffer;
    ComPtr<ID3D11Buffer> m_textVertexBuffer;
    ComPtr<ID3D11Buffer> m_constantBuffer; // still used for matrices, but not color

    ComPtr<ID3D11VertexShader> m_rectVS;
    ComPtr<ID3D11PixelShader> m_rectPS;
    ComPtr<ID3D11InputLayout> m_rectInputLayout;

    ComPtr<ID3D11VertexShader> m_textVS;
    ComPtr<ID3D11PixelShader> m_textPS;
    ComPtr<ID3D11InputLayout> m_textInputLayout;
    ComPtr<ID3D11ShaderResourceView> m_fontTexture;
    ComPtr<ID3D11SamplerState> m_samplerState;

    ComPtr<ID3D11RasterizerState> m_scissorRasterizerState;
    ComPtr<ID3D11DepthStencilState> m_depthStencilState;
    ComPtr<ID3D11BlendState> m_blendStateOpaque;
    ComPtr<ID3D11BlendState> m_textBlendState;

    // ---- Saved pipeline state ----
    ID3D11VertexShader* m_savedVS = nullptr;
    ID3D11PixelShader* m_savedPS = nullptr;
    ID3D11InputLayout* m_savedInputLayout = nullptr;
    ID3D11RasterizerState* m_savedRasterizer = nullptr;
    ID3D11DepthStencilState* m_savedDepthStencil = nullptr;
    ID3D11BlendState* m_savedBlend = nullptr;
    UINT m_savedStencilRef = 0;
    D3D11_RECT m_savedScissor = {};

    // ---- Clip state ----
    D3D11_RECT m_currentScissor = {};
    std::vector<D3D11_RECT> m_scissorStack;

    // ---- Flush tracking ----
    bool m_hasRectData = false;
    bool m_hasTextData = false;
};