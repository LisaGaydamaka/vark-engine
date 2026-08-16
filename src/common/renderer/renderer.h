#pragma once
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>        // NEW
#include "../core/math.h"

using Microsoft::WRL::ComPtr;

class Renderer
{
public:
    bool initialize(HWND windowHandle, int width, int height);
    void shutdown();
    void begin_frame();
    void end_frame();
    
    void draw_cube();
    void set_transform(const Mat4& mvp);
    void draw_vertices(ID3D11Buffer* vertexBuffer, int vertexCount, D3D11_PRIMITIVE_TOPOLOGY topology);
    void draw_lines(ID3D11Buffer* vertexBuffer, int vertexCount);

    // ---- Texture loading and switching ----
    void* load_texture(const char* filename);
    void set_texture(void* textureView);

    // ---- Get device/context as void* for compatibility ----
    void* get_device() const { return device.Get(); }     // changed
    void* get_context() const { return context.Get(); }   // changed
    
    void apply_pipeline();
    void apply_font_pipeline();
    void resize(int width, int height);
    bool is_device_lost() const { return m_deviceLost; }

private:
    // ---- Use ComPtr for all D3D resources ----
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swapChain;
    ComPtr<ID3D11RenderTargetView> renderTargetView;
    ComPtr<ID3D11DepthStencilView> depthStencilView;
    
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    ComPtr<ID3D11InputLayout> inputLayout;
    ComPtr<ID3D11Buffer> vertexBuffer;
    ComPtr<ID3D11Buffer> indexBuffer;
    ComPtr<ID3D11Buffer> constantBuffer;

    float clearColor[4] = {0.05f, 0.05f, 0.15f, 1.0f};

    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11RasterizerState> m_rasterizerState;
    int m_width = 800;
    int m_height = 600;
    bool m_deviceLost = false;

    // ---- Font pipeline ----
    ComPtr<ID3D11VertexShader> fontVertexShader;
    ComPtr<ID3D11PixelShader> fontPixelShader;
    ComPtr<ID3D11InputLayout> fontInputLayout;
    ComPtr<ID3D11ShaderResourceView> fontTextureView;
    ComPtr<ID3D11SamplerState> fontSampler;
    ComPtr<ID3D11Buffer> fontConstantBuffer;
    ComPtr<ID3D11BlendState> fontBlendState;

    // ---- Default fallback texture ----
    ComPtr<ID3D11ShaderResourceView> m_defaultTextureView;

    // ---- Line pipeline ----
    ComPtr<ID3D11PixelShader> linePixelShader;
};