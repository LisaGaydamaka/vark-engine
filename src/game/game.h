#pragma once
#include "engine/game_interface.h"
#include "core/camera.h"
#include "player/player.h"
#include <memory>
#include <vector>
#include <d3d11.h>
#include <wrl/client.h>

class Game : public IGame {
public:
    bool initialize(Engine* engine) override;
    void update(float dt) override;
    void render() override;
    void shutdown() override;
    void on_pause_toggle() override;
    void on_editor_mode(bool enabled) override;

private:
    void process_input(float dt);
    void update_wireframe();

    Engine* m_engine = nullptr;
    Camera* m_camera = nullptr;
    std::unique_ptr<Player> m_player;

    bool m_paused = false;
    bool m_gameMode = false;

    // Wireframe debug buffer
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_wireframeBuffer;
    int m_wireframeVertexCount = 0;
};