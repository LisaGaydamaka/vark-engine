#include "ui_renderer.h"
#include "../common/renderer/renderer.h"
#include "../common/core/font.h"
#include "../common/core/logger.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

static bool compile_shader(const char* code, const char* entry, const char* target, ID3DBlob** blob)
{
    ID3DBlob* error = nullptr;
    HRESULT hr = D3DCompile(code, strlen(code), nullptr, nullptr, nullptr, entry, target, D3DCOMPILE_DEBUG, 0, blob, &error);
    if (FAILED(hr)) {
        if (error) error->Release();
        return false;
    }
    if (error) error->Release();
    return true;
}

bool UIRenderer::initialize(Renderer* renderer, int width, int height)
{
    if (!renderer || width == 0 || height == 0) return false;

    m_renderer = renderer;
    m_device = (ID3D11Device*)renderer->get_device();
    m_context = (ID3D11DeviceContext*)renderer->get_context();
    m_width = width;
    m_height = height;

    if (!m_device || !m_context) return false;

    // -------- Solid rect shaders --------
    const char* vsRectCode = R"(
        cbuffer ColorBuffer : register(b1) {
            float4 color;
        };
        struct VSInput { float2 pos : POSITION; };
        struct VSOutput { float4 pos : SV_POSITION; float4 color : COLOR; };
        VSOutput main(VSInput input) {
            VSOutput output;
            output.pos = float4(input.pos, 0.0f, 1.0f);
            output.color = color;
            return output;
        }
    )";
    ID3DBlob* vsRectBlob = nullptr;
    if (!compile_shader(vsRectCode, "main", "vs_4_0", &vsRectBlob)) return false;
    HRESULT hr = m_device->CreateVertexShader(vsRectBlob->GetBufferPointer(), vsRectBlob->GetBufferSize(), nullptr, m_vertexShader.GetAddressOf());
    if (FAILED(hr)) { vsRectBlob->Release(); return false; }

    const char* psRectCode = R"(
        struct PSInput { float4 pos : SV_POSITION; float4 color : COLOR; };
        float4 main(PSInput input) : SV_TARGET { return input.color; }
    )";
    ID3DBlob* psRectBlob = nullptr;
    if (!compile_shader(psRectCode, "main", "ps_4_0", &psRectBlob)) return false;
    hr = m_device->CreatePixelShader(psRectBlob->GetBufferPointer(), psRectBlob->GetBufferSize(), nullptr, m_pixelShader.GetAddressOf());
    if (FAILED(hr)) { psRectBlob->Release(); return false; }

    D3D11_INPUT_ELEMENT_DESC layoutRect[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    hr = m_device->CreateInputLayout(layoutRect, 1, vsRectBlob->GetBufferPointer(), vsRectBlob->GetBufferSize(), m_inputLayout.GetAddressOf());
    vsRectBlob->Release();
    psRectBlob->Release();
    if (FAILED(hr)) return false;

    // Constant buffer (slot 1)
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.ByteWidth = sizeof(float) * 4;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = m_device->CreateBuffer(&cbd, nullptr, m_constantBuffer.GetAddressOf());
    if (FAILED(hr)) return false;

    // Dynamic vertex buffer for rects
    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DYNAMIC;
    vbd.ByteWidth = 4 * 2 * sizeof(float);
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = m_device->CreateBuffer(&vbd, nullptr, m_vertexBuffer.GetAddressOf());
    if (FAILED(hr)) return false;

    // Index buffer (quad)
    unsigned short indices[] = {0, 1, 2, 0, 2, 3};
    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(indices);
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA idata = {indices};
    hr = m_device->CreateBuffer(&ibd, &idata, m_indexBuffer.GetAddressOf());
    if (FAILED(hr)) return false;

    // Rasterizer, depth
    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_NONE;
    rsDesc.FrontCounterClockwise = FALSE;
    hr = m_device->CreateRasterizerState(&rsDesc, m_rasterizerState.GetAddressOf());
    if (FAILED(hr)) return false;

    // Scissor-enabled rasterizer
    D3D11_RASTERIZER_DESC rsDescScissor = rsDesc;
    rsDescScissor.ScissorEnable = TRUE;
    hr = m_device->CreateRasterizerState(&rsDescScissor, m_scissorRasterizerState.GetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = FALSE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = D3D11_COMPARISON_NEVER;
    hr = m_device->CreateDepthStencilState(&dsDesc, m_depthStencilState.GetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_BLEND_DESC opaqueBlend = {};
    opaqueBlend.RenderTarget[0].BlendEnable = FALSE;
    opaqueBlend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = m_device->CreateBlendState(&opaqueBlend, m_blendStateOpaque.GetAddressOf());
    if (FAILED(hr)) return false;

    // Text resources
    if (!create_font_resources()) {
        LOG_WARN("UI: Font resources failed, text will not be available.");
    }

    m_currentScissor = { 0, 0, width, height };
    m_scissorStack.clear();

    m_initialized = true;
    return true;
}

bool UIRenderer::create_font_resources()
{
    if (!BitmapFont::create_texture(m_device, m_fontTexture.GetAddressOf())) {
        return false;
    }

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(m_device->CreateSamplerState(&sampDesc, m_samplerState.GetAddressOf()))) {
        return false;
    }

    const char* vsTextCode = R"(
        cbuffer ColorBuffer : register(b1) {
            float4 color;
        };
        struct VSInput {
            float2 pos : POSITION;
            float2 uv : TEXCOORD;
        };
        struct VSOutput {
            float4 pos : SV_POSITION;
            float2 uv : TEXCOORD;
            float4 color : COLOR;
        };
        VSOutput main(VSInput input) {
            VSOutput output;
            output.pos = float4(input.pos, 0.0f, 1.0f);
            output.uv = input.uv;
            output.color = color;
            return output;
        }
    )";
    ID3DBlob* vsTextBlob = nullptr;
    if (!compile_shader(vsTextCode, "main", "vs_4_0", &vsTextBlob)) return false;
    HRESULT hr = m_device->CreateVertexShader(vsTextBlob->GetBufferPointer(), vsTextBlob->GetBufferSize(), nullptr, m_textVertexShader.GetAddressOf());
    if (FAILED(hr)) { vsTextBlob->Release(); return false; }

    const char* psTextCode = R"(
        Texture2D tex0 : register(t0);
        SamplerState samp0 : register(s0);
        struct PSInput {
            float4 pos : SV_POSITION;
            float2 uv : TEXCOORD;
            float4 color : COLOR;
        };
        float4 main(PSInput input) : SV_TARGET {
            float4 texColor = tex0.Sample(samp0, input.uv);
            return float4(input.color.rgb, texColor.a * input.color.a);
        }
    )";
    ID3DBlob* psTextBlob = nullptr;
    if (!compile_shader(psTextCode, "main", "ps_4_0", &psTextBlob)) return false;
    hr = m_device->CreatePixelShader(psTextBlob->GetBufferPointer(), psTextBlob->GetBufferSize(), nullptr, m_textPixelShader.GetAddressOf());
    if (FAILED(hr)) { psTextBlob->Release(); return false; }

    D3D11_INPUT_ELEMENT_DESC layoutText[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    hr = m_device->CreateInputLayout(layoutText, 2, vsTextBlob->GetBufferPointer(), vsTextBlob->GetBufferSize(), m_textInputLayout.GetAddressOf());
    vsTextBlob->Release();
    psTextBlob->Release();
    if (FAILED(hr)) return false;

    D3D11_BUFFER_DESC vbdText = {};
    vbdText.Usage = D3D11_USAGE_DYNAMIC;
    vbdText.ByteWidth = 256 * 6 * (2 + 2) * sizeof(float);
    vbdText.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbdText.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = m_device->CreateBuffer(&vbdText, nullptr, m_textVertexBuffer.GetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_BLEND_DESC textBlend = {};
    textBlend.RenderTarget[0].BlendEnable = TRUE;
    textBlend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    textBlend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    textBlend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    textBlend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    textBlend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    textBlend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    textBlend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(m_device->CreateBlendState(&textBlend, m_textBlendState.GetAddressOf()))) {
        return false;
    }

    m_fontReady = true;
    return true;
}

void UIRenderer::shutdown()
{
    m_initialized = false;
    m_fontReady = false;
}

void UIRenderer::resize(int width, int height)
{
    m_width = width;
    m_height = height;
    m_currentScissor = { 0, 0, width, height };
    m_scissorStack.clear();
}

void UIRenderer::push_clip_rect(float x, float y, float w, float h)
{
    if (!m_initialized) return;
    m_scissorStack.push_back(m_currentScissor);

    LONG left   = std::max<LONG>(m_currentScissor.left,   static_cast<LONG>(x));
    LONG top    = std::max<LONG>(m_currentScissor.top,    static_cast<LONG>(y));
    LONG right  = std::min<LONG>(m_currentScissor.right,  static_cast<LONG>(x + w));
    LONG bottom = std::min<LONG>(m_currentScissor.bottom, static_cast<LONG>(y + h));

    if (right <= left || bottom <= top) {
        m_currentScissor = { 0, 0, 0, 0 };
    } else {
        m_currentScissor = { left, top, right, bottom };
    }
    m_context->RSSetScissorRects(1, &m_currentScissor);
}

void UIRenderer::pop_clip_rect()
{
    if (!m_initialized) return;
    if (!m_scissorStack.empty()) {
        m_currentScissor = m_scissorStack.back();
        m_scissorStack.pop_back();
        m_context->RSSetScissorRects(1, &m_currentScissor);
    }
}

void UIRenderer::restore_engine_pipeline()
{
    if (m_savedVS) { m_context->VSSetShader(m_savedVS, nullptr, 0); m_savedVS = nullptr; }
    if (m_savedPS) { m_context->PSSetShader(m_savedPS, nullptr, 0); m_savedPS = nullptr; }
    if (m_savedInputLayout) { m_context->IASetInputLayout(m_savedInputLayout); m_savedInputLayout = nullptr; }
    if (m_savedRasterizer) { m_context->RSSetState(m_savedRasterizer); m_savedRasterizer = nullptr; }
    if (m_savedDepthStencil) { m_context->OMSetDepthStencilState(m_savedDepthStencil, m_savedStencilRef); m_savedDepthStencil = nullptr; }
    if (m_savedBlend) { m_context->OMSetBlendState(m_savedBlend, nullptr, 0xffffffff); m_savedBlend = nullptr; }
    m_context->RSSetScissorRects(1, &m_savedScissor);
}

void UIRenderer::draw_rect(float x, float y, float w, float h, float r, float g, float b, float a)
{
    if (!m_initialized) return;

    m_context->VSGetShader(&m_savedVS, nullptr, nullptr);
    m_context->PSGetShader(&m_savedPS, nullptr, nullptr);
    m_context->IAGetInputLayout(&m_savedInputLayout);
    m_context->RSGetState(&m_savedRasterizer);
    m_context->OMGetDepthStencilState(&m_savedDepthStencil, &m_savedStencilRef);
    m_context->OMGetBlendState(&m_savedBlend, nullptr, nullptr);
    UINT numRects = 1;
    m_context->RSGetScissorRects(&numRects, &m_savedScissor);

    float left   = (x / m_width) * 2.0f - 1.0f;
    float right  = ((x + w) / m_width) * 2.0f - 1.0f;
    float top    = 1.0f - (y / m_height) * 2.0f;
    float bottom = 1.0f - ((y + h) / m_height) * 2.0f;
    struct Pos { float x, y; };
    Pos verts[4] = { {left, top}, {right, top}, {right, bottom}, {left, bottom} };

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = m_context->Map(m_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) { restore_engine_pipeline(); return; }
    memcpy(mapped.pData, verts, sizeof(verts));
    m_context->Unmap(m_vertexBuffer.Get(), 0);

    hr = m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) { restore_engine_pipeline(); return; }
    float color[4] = { r, g, b, a };
    memcpy(mapped.pData, color, sizeof(color));
    m_context->Unmap(m_constantBuffer.Get(), 0);

    UINT stride = sizeof(Pos);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    m_context->VSSetConstantBuffers(1, 1, m_constantBuffer.GetAddressOf());

    m_context->RSSetState(m_scissorRasterizerState.Get());
    m_context->RSSetScissorRects(1, &m_currentScissor);
    m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
    m_context->OMSetBlendState(m_blendStateOpaque.Get(), nullptr, 0xffffffff);

    m_context->DrawIndexed(6, 0, 0);

    restore_engine_pipeline();
}

void UIRenderer::draw_text(float x, float y, const char* text, float r, float g, float b, float a)
{
    if (!m_initialized || !m_fontReady || !text || !text[0]) return;

    m_context->VSGetShader(&m_savedVS, nullptr, nullptr);
    m_context->PSGetShader(&m_savedPS, nullptr, nullptr);
    m_context->IAGetInputLayout(&m_savedInputLayout);
    m_context->RSGetState(&m_savedRasterizer);
    m_context->OMGetDepthStencilState(&m_savedDepthStencil, &m_savedStencilRef);
    m_context->OMGetBlendState(&m_savedBlend, nullptr, nullptr);
    UINT numRects = 1;
    m_context->RSGetScissorRects(&numRects, &m_savedScissor);

    struct TextVertex { float x, y, u, v; };
    const int maxChars = 256;
    TextVertex vertices[256 * 6];
    int vertexCount = 0;

    float cursorX = x;
    float cursorY = y;
    float charW = 8.0f;
    float charH = 8.0f;

    for (const char* ch = text; *ch && vertexCount < maxChars * 6; ++ch) {
        if (*ch == ' ') {
            cursorX += charW;
            continue;
        }

        float u1, v1, u2, v2;
        BitmapFont::get_char_uv(*ch, u1, v1, u2, v2);

        float left   = (cursorX / m_width) * 2.0f - 1.0f;
        float right  = ((cursorX + charW) / m_width) * 2.0f - 1.0f;
        float top    = 1.0f - (cursorY / m_height) * 2.0f;
        float bottom = 1.0f - ((cursorY + charH) / m_height) * 2.0f;

        vertices[vertexCount++] = { left,  top,    u1, v1 };
        vertices[vertexCount++] = { right, top,    u2, v1 };
        vertices[vertexCount++] = { right, bottom, u2, v2 };
        vertices[vertexCount++] = { left,  top,    u1, v1 };
        vertices[vertexCount++] = { right, bottom, u2, v2 };
        vertices[vertexCount++] = { left,  bottom, u1, v2 };

        cursorX += charW;
    }

    if (vertexCount == 0) {
        restore_engine_pipeline();
        return;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = m_context->Map(m_textVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) { restore_engine_pipeline(); return; }
    memcpy(mapped.pData, vertices, vertexCount * sizeof(TextVertex));
    m_context->Unmap(m_textVertexBuffer.Get(), 0);

    hr = m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) { restore_engine_pipeline(); return; }
    float color[4] = { r, g, b, a };
    memcpy(mapped.pData, color, sizeof(color));
    m_context->Unmap(m_constantBuffer.Get(), 0);

    UINT stride = sizeof(TextVertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_textVertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->IASetInputLayout(m_textInputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_context->VSSetShader(m_textVertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_textPixelShader.Get(), nullptr, 0);
    m_context->VSSetConstantBuffers(1, 1, m_constantBuffer.GetAddressOf());
    m_context->PSSetShaderResources(0, 1, m_fontTexture.GetAddressOf());
    m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    m_context->RSSetState(m_scissorRasterizerState.Get());
    m_context->RSSetScissorRects(1, &m_currentScissor);
    m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
    m_context->OMSetBlendState(m_textBlendState.Get(), nullptr, 0xffffffff);

    m_context->Draw(vertexCount, 0);

    restore_engine_pipeline();
}