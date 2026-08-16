#include "game.h"
#include "engine/engine.h"
#include "physics/physics_world.h"
#include "world/level.h"          // NEW
#include "core/logger.h"
#include <windows.h>
#include <d3d11.h>
#include <cmath>

static inline float clampf(float value, float minVal, float maxVal) {
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return value;
}

bool Game::initialize(Engine* engine) {
    m_engine = engine;
    if (!m_engine) return false;

    m_camera = m_engine->get_camera();
    if (!m_camera) {
        LOG_ERROR("Game: Engine camera is null");
        return false;
    }

    Level* level = m_engine->get_level();
    if (!level) {
        LOG_ERROR("Game: Level is null");
        return false;
    }

    const Settings& settings = m_engine->get_settings();  // NEW

    const PhysicsWorld* physics = &level->get_physics_world();
    m_player = std::make_unique<Player>(physics, m_camera, &settings);  // pass settings
    m_player->set_position({0.0f, 5.0f, 0.0f});
    m_player->set_yaw(0.0f);
    m_player->set_pitch(0.0f);


    // Create wireframe buffer (always, but only drawn in debug mode)
    ID3D11Device* device = (ID3D11Device*)m_engine->get_renderer()->get_device();
    if (device) {
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth = 4096 * sizeof(Vertex);
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device->CreateBuffer(&bd, nullptr, m_wireframeBuffer.GetAddressOf()))) {
            LOG_WARN("Game: Failed to create wireframe buffer");
        }
    }

    m_gameMode = true;
    m_paused = false;
    m_engine->lock_mouse(true);

    LOG_INFO("Game initialized.");
    return true;
}

void Game::update(float dt) {
    if (m_paused || !m_gameMode) return;

    // --- Mouse look ---
    float dx, dy;
    m_engine->get_mouse_delta(dx, dy);
    m_player->set_yaw(m_player->get_yaw() + dx);
    m_player->set_pitch(m_player->get_pitch() + dy);
    m_player->set_pitch(clampf(m_player->get_pitch(), -1.5f, 1.5f));

    // --- Player physics ---
    m_player->update(dt);

    // --- Update wireframe ---
    update_wireframe();
}

void Game::render() {
    // Render player wireframe ONLY if debug mode is active
    if (m_player && m_wireframeBuffer && m_wireframeVertexCount > 0) {
        Level* level = m_engine->get_level();
        if (level && level->is_debug_mode()) {
            Renderer* renderer = m_engine->get_renderer();
            if (renderer) {
                renderer->draw_lines(m_wireframeBuffer.Get(), m_wireframeVertexCount);
            }
        }
    }
}

void Game::shutdown() {
    m_player.reset();
    m_wireframeBuffer.Reset();
    LOG_INFO("Game shut down.");
}

void Game::on_pause_toggle() {
    m_paused = !m_paused;
    if (m_paused) {
        m_engine->lock_mouse(false);
        LOG_INFO("Game paused");
    } else {
        m_engine->lock_mouse(true);
        LOG_INFO("Game resumed");
    }
}

void Game::on_editor_mode(bool enabled) {
    m_gameMode = !enabled;
    if (m_gameMode) {
        m_paused = false;
        m_engine->lock_mouse(true);
    } else {
        m_engine->lock_mouse(false);
    }
}

void Game::update_wireframe() {
    if (!m_player || !m_wireframeBuffer) return;

    std::vector<Vertex> vertices;
    m_player->get_capsule_wireframe(vertices);
    if (vertices.empty()) {
        m_wireframeVertexCount = 0;
        return;
    }

    ID3D11DeviceContext* ctx = (ID3D11DeviceContext*)m_engine->get_renderer()->get_context();
    if (!ctx) return;

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = ctx->Map(m_wireframeBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) return;
    memcpy(mapped.pData, vertices.data(), vertices.size() * sizeof(Vertex));
    ctx->Unmap(m_wireframeBuffer.Get(), 0);
    m_wireframeVertexCount = (int)vertices.size();
}