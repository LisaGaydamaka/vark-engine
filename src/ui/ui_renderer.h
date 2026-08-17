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
    void draw_rect(float x, float y, float w, float h, float r, float g, float b, float a);
    void draw_text(float x, float y, const char* text, float r, float g, float b, float a);

    // ---- NEW: helper to compute text width (monospace) ----
    float get_text_width(const char* text) const;

    // Clipping
    void push_clip_rect(float x, float y, float w, float h);
    void pop_clip_rect();

    int get_width() const { return m_width; }
    int get_height() const { return m_height; }

private:
    void restore_engine_pipeline();
    bool create_font_resources();

    Renderer* m_renderer = nullptr;
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;

    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11Buffer> m_indexBuffer;
    ComPtr<ID3D11Buffer> m_constantBuffer;
    ComPtr<ID3D11RasterizerState> m_rasterizerState;
    ComPtr<ID3D11RasterizerState> m_scissorRasterizerState;
    ComPtr<ID3D11DepthStencilState> m_depthStencilState;
    ComPtr<ID3D11BlendState> m_blendStateOpaque;

    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11InputLayout> m_inputLayout;

    ComPtr<ID3D11VertexShader> m_textVertexShader;
    ComPtr<ID3D11PixelShader> m_textPixelShader;
    ComPtr<ID3D11InputLayout> m_textInputLayout;
    ComPtr<ID3D11Buffer> m_textVertexBuffer;
    ComPtr<ID3D11BlendState> m_textBlendState;
    ComPtr<ID3D11ShaderResourceView> m_fontTexture;
    ComPtr<ID3D11SamplerState> m_samplerState;

    ID3D11VertexShader* m_savedVS = nullptr;
    ID3D11PixelShader* m_savedPS = nullptr;
    ID3D11InputLayout* m_savedInputLayout = nullptr;
    ID3D11RasterizerState* m_savedRasterizer = nullptr;
    ID3D11DepthStencilState* m_savedDepthStencil = nullptr;
    ID3D11BlendState* m_savedBlend = nullptr;
    UINT m_savedStencilRef = 0;
    D3D11_RECT m_savedScissor = {};

    int m_width = 0;
    int m_height = 0;
    bool m_initialized = false;
    bool m_fontReady = false;

    D3D11_RECT m_currentScissor = {};
    std::vector<D3D11_RECT> m_scissorStack;
};