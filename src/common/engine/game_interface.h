#pragma once

class Engine;

class IGame {
public:
    virtual ~IGame() = default;

    virtual bool initialize(Engine* engine) = 0;
    virtual void update(float deltaTime) = 0;
    virtual void render() = 0;          // called after engine's 3D render
    virtual void shutdown() = 0;

    virtual void on_pause_toggle() = 0;
    virtual void on_editor_mode(bool enabled) = 0;
};