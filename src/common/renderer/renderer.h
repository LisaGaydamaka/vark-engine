#pragma once
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include "../core/math.h"

using Microsoft::WRL::ComPtr;

class Renderer
{
public:
    bool initialize(HWND windowHandle, int width, int height);
    void shutdown();
    void begin_frame();
    void end_frame();
    
    void set_transform(const Mat4& mvp);
    void draw_vertices(ID3D11Buffer* vertexBuffer, int vertexCount, D3D11_PRIMITIVE_TOPOLOGY topology);
    void draw_lines(ID3D11Buffer* vertexBuffer, int vertexCount);

    void* load_texture(const char* filename);
    void set_texture(void* textureView);

    void* get_device() const { return device.Get(); }
    void* get_context() const { return context.Get(); }
    
    // --- NEW getters ---
    int get_width() const { return m_width; }
    int get_height() const { return m_height; }

    void apply_pipeline();
    void apply_font_pipeline();
    void resize(int width, int height);
    bool is_device_lost() const { return m_deviceLost; }

    void set_depth_test(bool enable);

private:
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

    ComPtr<ID3D11VertexShader> fontVertexShader;
    ComPtr<ID3D11PixelShader> fontPixelShader;
    ComPtr<ID3D11InputLayout> fontInputLayout;
    ComPtr<ID3D11ShaderResourceView> fontTextureView;
    ComPtr<ID3D11SamplerState> fontSampler;
    ComPtr<ID3D11Buffer> fontConstantBuffer;
    ComPtr<ID3D11BlendState> fontBlendState;

    ComPtr<ID3D11ShaderResourceView> m_defaultTextureView;

    ComPtr<ID3D11PixelShader> linePixelShader;
    ComPtr<ID3D11VertexShader> lineVertexShader;
    ComPtr<ID3D11InputLayout> lineInputLayout;

    ComPtr<ID3D11DepthStencilState> m_depthTestState;
    ComPtr<ID3D11DepthStencilState> m_noDepthTestState;
};