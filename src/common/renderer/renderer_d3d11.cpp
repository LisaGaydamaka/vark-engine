#include "renderer.h"
#include "../core/geometry.h"
#include "../core/font.h"
#include "../core/logger.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "windowscodecs.lib")

static bool compile_shader(const char* code, const char* entry, const char* target, ID3DBlob** blob)
{
    ID3DBlob* errorBlob = nullptr;
    HRESULT hr = D3DCompile(code, strlen(code), nullptr, nullptr, nullptr, entry, target, D3DCOMPILE_DEBUG, 0, blob, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob) {
            LOG_ERROR("Shader compile error: %s", (char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }
    if (errorBlob) errorBlob->Release();
    return true;
}

static bool load_texture_from_file(ID3D11Device* device, const wchar_t* filename, ID3D11ShaderResourceView** outView)
{
    using Microsoft::WRL::ComPtr;

    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return false;

    hr = factory->CreateDecoderFromFilename(filename, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) return false;

    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return false;

    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return false;

    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return false;

    UINT width, height;
    converter->GetSize(&width, &height);

    UINT stride = width * 4;
    UINT bufferSize = width * height * 4;
    BYTE* pixelData = new BYTE[bufferSize];

    hr = converter->CopyPixels(nullptr, stride, bufferSize, pixelData);
    if (FAILED(hr)) {
        delete[] pixelData;
        return false;
    }

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixelData;
    initData.SysMemPitch = stride;

    ComPtr<ID3D11Texture2D> texture;
    hr = device->CreateTexture2D(&texDesc, &initData, &texture);
    delete[] pixelData;

    if (FAILED(hr)) return false;

    hr = device->CreateShaderResourceView(texture.Get(), nullptr, outView);
    if (FAILED(hr)) return false;

    return true;
}

bool Renderer::initialize(HWND hwnd, int width, int height)
{
    LOG_INFO("Renderer::initialize: width=%d, height=%d", width, height);

    m_width = width;
    m_height = height;

    // ---- Swap Chain ----
    DXGI_SWAP_CHAIN_DESC swapDesc = {};
    swapDesc.BufferCount = 1;
    swapDesc.BufferDesc.Width = width;
    swapDesc.BufferDesc.Height = height;
    swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.OutputWindow = hwnd;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.SampleDesc.Quality = 0;
    swapDesc.Windowed = TRUE;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &swapDesc,
        swapChain.GetAddressOf(),
        device.GetAddressOf(),
        nullptr,
        context.GetAddressOf()
    );
    if (FAILED(hr) || !device || !swapChain) {
        LOG_ERROR("D3D11CreateDeviceAndSwapChain failed (hr=0x%08X)", hr);
        return false;
    }
    LOG_INFO("D3D11 device created.");
    LOG_INFO("D3D11 context created.");

    // ---- Render Target ----
    ComPtr<ID3D11Texture2D> backBuffer;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
    device->CreateRenderTargetView(backBuffer.Get(), nullptr, renderTargetView.GetAddressOf());

    // ---- Depth Stencil ----
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ComPtr<ID3D11Texture2D> depthTexture;
    device->CreateTexture2D(&depthDesc, nullptr, &depthTexture);
    device->CreateDepthStencilView(depthTexture.Get(), nullptr, depthStencilView.GetAddressOf());

    context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), depthStencilView.Get());

    // ---- Viewport ----
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)width;
    vp.Height = (float)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context->RSSetViewports(1, &vp);

    // ---- Rasterizer State ----
    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_BACK;
    rasterDesc.FrontCounterClockwise = FALSE;
    device->CreateRasterizerState(&rasterDesc, m_rasterizerState.GetAddressOf());
    context->RSSetState(m_rasterizerState.Get());

    // ---- Main Vertex Shader ----
    const char* vertexShaderCode = R"(
        cbuffer MatrixBuffer : register(b0) {
            row_major matrix mvp;
        };
        struct VSInput {
            float3 pos : POSITION;
            float2 uv : TEXCOORD0;
            float3 color : COLOR;
        };
        struct VSOutput {
            float4 pos : SV_POSITION;
            float2 uv : TEXCOORD0;
            float3 color : COLOR;
        };
        VSOutput main(VSInput input) {
            VSOutput output;
            output.pos = mul(float4(input.pos, 1.0f), mvp);
            output.uv = input.uv;
            output.color = input.color;
            return output;
        }
    )";
    ID3DBlob* vsBlob = nullptr;
    if (!compile_shader(vertexShaderCode, "main", "vs_4_0", &vsBlob)) return false;
    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, vertexShader.GetAddressOf());

    // ---- Main Pixel Shader ----
    const char* pixelShaderCode = R"(
        Texture2D tex0 : register(t0);
        SamplerState samp0 : register(s0);
        struct PSInput {
            float4 pos : SV_POSITION;
            float2 uv : TEXCOORD0;
            float3 color : COLOR;
        };
        float4 main(PSInput input) : SV_TARGET {
            float4 texColor = tex0.Sample(samp0, input.uv);
            return float4(texColor.rgb * input.color, 1.0f);
        }
    )";
    ID3DBlob* psBlob = nullptr;
    if (!compile_shader(pixelShaderCode, "main", "ps_4_0", &psBlob)) return false;
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, pixelShader.GetAddressOf());

    // ---- Main Input Layout ----
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    hr = device->CreateInputLayout(
        layoutDesc,
        sizeof(layoutDesc) / sizeof(layoutDesc[0]),
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        inputLayout.GetAddressOf()
    );
    if (FAILED(hr) || inputLayout == nullptr) {
        // Fallback without COLOR
        D3D11_INPUT_ELEMENT_DESC fallbackDesc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        device->CreateInputLayout(
            fallbackDesc,
            sizeof(fallbackDesc) / sizeof(fallbackDesc[0]),
            vsBlob->GetBufferPointer(),
            vsBlob->GetBufferSize(),
            inputLayout.GetAddressOf()
        );
    }
    vsBlob->Release();
    psBlob->Release();

    // ---- Constant Buffer ----
    D3D11_BUFFER_DESC constBufferDesc = {};
    constBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    constBufferDesc.ByteWidth = sizeof(Mat4);
    constBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&constBufferDesc, nullptr, constantBuffer.GetAddressOf());

    // ---- Create a default white fallback texture ----
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = 1; texDesc.Height = 1;
    texDesc.MipLevels = 1; texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    unsigned char white[4] = { 255, 255, 255, 255 };
    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = white;
    data.SysMemPitch = 4;
    ComPtr<ID3D11Texture2D> fallbackTex;
    device->CreateTexture2D(&texDesc, &data, &fallbackTex);
    device->CreateShaderResourceView(fallbackTex.Get(), nullptr, m_defaultTextureView.GetAddressOf());

    // ---- Sampler ----
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    device->CreateSamplerState(&sampDesc, sampler.GetAddressOf());
    context->PSSetSamplers(0, 1, sampler.GetAddressOf());

    // ---- FONT PIPELINE ----
    ID3DBlob* fontVSBlob = nullptr;
    if (!compile_shader(vertexShaderCode, "main", "vs_4_0", &fontVSBlob)) return false;
    device->CreateVertexShader(fontVSBlob->GetBufferPointer(), fontVSBlob->GetBufferSize(), nullptr, fontVertexShader.GetAddressOf());

    const char* fontPixelShaderCode = R"(
        Texture2D tex0 : register(t0);
        SamplerState samp0 : register(s0);
        struct PSInput {
            float4 pos : SV_POSITION;
            float2 uv : TEXCOORD0;
            float3 color : COLOR;
        };
        float4 main(PSInput input) : SV_TARGET {
            float4 texColor = tex0.Sample(samp0, input.uv);
            return float4(texColor.rgb * input.color, texColor.a);
        }
    )";
    ID3DBlob* fontPSBlob = nullptr;
    if (!compile_shader(fontPixelShaderCode, "main", "ps_4_0", &fontPSBlob)) return false;
    device->CreatePixelShader(fontPSBlob->GetBufferPointer(), fontPSBlob->GetBufferSize(), nullptr, fontPixelShader.GetAddressOf());

    device->CreateInputLayout(
        layoutDesc,
        sizeof(layoutDesc) / sizeof(layoutDesc[0]),
        fontVSBlob->GetBufferPointer(),
        fontVSBlob->GetBufferSize(),
        fontInputLayout.GetAddressOf()
    );
    fontVSBlob->Release();
    fontPSBlob->Release();

    // ---- Font texture ----
    if (!BitmapFont::create_texture(device.Get(), fontTextureView.GetAddressOf())) {
        // fallback green
        D3D11_TEXTURE2D_DESC texDescF = {};
        texDescF.Width = 16; texDescF.Height = 16;
        texDescF.MipLevels = 1; texDescF.ArraySize = 1;
        texDescF.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDescF.SampleDesc.Count = 1;
        texDescF.Usage = D3D11_USAGE_DEFAULT;
        texDescF.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        unsigned char pixels[16*16*4];
        for (int y=0; y<16; ++y) {
            for (int x=0; x<16; ++x) {
                int idx = (y*16 + x)*4;
                pixels[idx+0] = 0;
                pixels[idx+1] = 255;
                pixels[idx+2] = 0;
                pixels[idx+3] = 255;
            }
        }
        D3D11_SUBRESOURCE_DATA dataF = {};
        dataF.pSysMem = pixels;
        dataF.SysMemPitch = 16*4;
        ComPtr<ID3D11Texture2D> fallbackTexF;
        device->CreateTexture2D(&texDescF, &dataF, &fallbackTexF);
        device->CreateShaderResourceView(fallbackTexF.Get(), nullptr, fontTextureView.GetAddressOf());
        LOG_INFO("FONT: Using fallback green texture");
    }

    // Font sampler (point, clamp)
    D3D11_SAMPLER_DESC fontSampDesc = {};
    fontSampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    fontSampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    fontSampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    fontSampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    fontSampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    fontSampDesc.MinLOD = 0;
    fontSampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    device->CreateSamplerState(&fontSampDesc, fontSampler.GetAddressOf());

    fontConstantBuffer = constantBuffer;  // reuse

    // ---- Alpha blend state for font ----
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&blendDesc, fontBlendState.GetAddressOf());

    // ---- LINE PIPELINE ----
    const char* linePSCode = R"(
        struct PSInput {
            float4 pos : SV_POSITION;
            float3 color : COLOR;
        };
        float4 main(PSInput input) : SV_TARGET {
            return float4(input.color, 1.0f);
        }
    )";
    ID3DBlob* linePSBlob = nullptr;
    if (!compile_shader(linePSCode, "main", "ps_4_0", &linePSBlob)) return false;
    device->CreatePixelShader(linePSBlob->GetBufferPointer(), linePSBlob->GetBufferSize(), nullptr, linePixelShader.GetAddressOf());
    linePSBlob->Release();

    LOG_INFO("Renderer initialization complete.");
    return true;
}

void Renderer::shutdown()
{
    // All ComPtrs auto-release. No manual cleanup needed.
    LOG_INFO("Renderer shutdown complete.");
}

void Renderer::begin_frame()
{
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)m_width;
    vp.Height = (float)m_height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    context->RSSetViewports(1, &vp);

    context->ClearRenderTargetView(renderTargetView.Get(), clearColor);
    if (depthStencilView) {
        context->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    }
}

void Renderer::end_frame()
{
    swapChain->Present(1, 0);
}

void Renderer::set_transform(const Mat4& mvp)
{
    D3D11_MAPPED_SUBRESOURCE mappedData = {};
    HRESULT hr = context->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData);
    if (FAILED(hr)) return;
    memcpy(mappedData.pData, &mvp, sizeof(Mat4));
    context->Unmap(constantBuffer.Get(), 0);
    context->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
}

void Renderer::apply_pipeline()
{
    context->IASetInputLayout(inputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertexShader.Get(), nullptr, 0);
    context->PSSetShader(pixelShader.Get(), nullptr, 0);

    if (sampler) {
        context->PSSetSamplers(0, 1, sampler.GetAddressOf());
    }
    if (m_rasterizerState) {
        context->RSSetState(m_rasterizerState.Get());
    }
    context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
}

void Renderer::apply_font_pipeline()
{
    context->IASetInputLayout(fontInputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(fontVertexShader.Get(), nullptr, 0);
    context->PSSetShader(fontPixelShader.Get(), nullptr, 0);

    if (fontTextureView && fontSampler) {
        context->PSSetShaderResources(0, 1, fontTextureView.GetAddressOf());
        context->PSSetSamplers(0, 1, fontSampler.GetAddressOf());
    }
    if (m_rasterizerState) {
        context->RSSetState(m_rasterizerState.Get());
    }
    if (fontBlendState) {
        context->OMSetBlendState(fontBlendState.Get(), nullptr, 0xffffffff);
    }
}

void* Renderer::load_texture(const char* filename)
{
    // Build full path: "../../assets/textures/" + filename
    std::wstring wpath;
    std::string fullPath = "../../assets/textures/" + std::string(filename);
    int len = MultiByteToWideChar(CP_UTF8, 0, fullPath.c_str(), -1, NULL, 0);
    if (len > 0) {
        wpath.resize(len);
        MultiByteToWideChar(CP_UTF8, 0, fullPath.c_str(), -1, &wpath[0], len);
        wpath.pop_back();
    }

    ComPtr<ID3D11ShaderResourceView> view;

    if (load_texture_from_file(device.Get(), wpath.c_str(), view.GetAddressOf())) {
        view.CopyTo(m_defaultTextureView.GetAddressOf()); // not storing per texture, just return raw pointer
        return view.Detach(); // caller responsible for release; we're returning raw pointer
    }

    if (strcmp(filename, "zebra.png") != 0) {
        std::wstring zebraPath;
        std::string zebraFull = "../../assets/textures/zebra.png";
        int zebraLen = MultiByteToWideChar(CP_UTF8, 0, zebraFull.c_str(), -1, NULL, 0);
        if (zebraLen > 0) {
            zebraPath.resize(zebraLen);
            MultiByteToWideChar(CP_UTF8, 0, zebraFull.c_str(), -1, &zebraPath[0], zebraLen);
            zebraPath.pop_back();
        }
        if (load_texture_from_file(device.Get(), zebraPath.c_str(), view.GetAddressOf())) {
            return view.Detach();
        }
    }

    // fallback
    m_defaultTextureView.Get()->AddRef(); // caller gets a reference
    return m_defaultTextureView.Get();
}

void Renderer::set_texture(void* textureView)
{
    ID3D11ShaderResourceView* tex = textureView ? (ID3D11ShaderResourceView*)textureView : m_defaultTextureView.Get();
    context->PSSetShaderResources(0, 1, &tex);
}

void Renderer::draw_vertices(ID3D11Buffer* vertexBuffer, int vertexCount, D3D11_PRIMITIVE_TOPOLOGY topology)
{
    if (!vertexBuffer || vertexCount == 0) return;

    apply_pipeline();

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    context->IASetPrimitiveTopology(topology);
    context->Draw(vertexCount, 0);
}

void Renderer::draw_lines(ID3D11Buffer* vertexBuffer, int vertexCount)
{
    if (!vertexBuffer || vertexCount == 0) return;

    context->IASetInputLayout(inputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    context->VSSetShader(vertexShader.Get(), nullptr, 0);
    context->PSSetShader(linePixelShader.Get(), nullptr, 0);

    context->PSSetShaderResources(0, 0, nullptr);
    context->PSSetSamplers(0, 0, nullptr);

    if (m_rasterizerState) {
        context->RSSetState(m_rasterizerState.Get());
    }
    context->OMSetBlendState(nullptr, nullptr, 0xffffffff);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    context->Draw(vertexCount, 0);
}

void Renderer::resize(int width, int height)
{
    m_width = width;
    m_height = height;

    if (!swapChain) return;

    renderTargetView.Reset();

    HRESULT hr = swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) return;

    ComPtr<ID3D11Texture2D> backBuffer;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
    device->CreateRenderTargetView(backBuffer.Get(), nullptr, renderTargetView.GetAddressOf());

    context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), depthStencilView.Get());

    D3D11_VIEWPORT vp;
    vp.Width = (float)width;
    vp.Height = (float)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    context->RSSetViewports(1, &vp);
}