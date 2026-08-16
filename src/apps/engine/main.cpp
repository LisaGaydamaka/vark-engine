#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include "engine/engine.h"
#include "core/logger.h"
#include "game/game.h"         // NEW: include concrete game
#include <memory>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    Logger::instance().init();   // start logger

    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    SetConsoleTitleA("Vark Engine - Debug Console");
    LOG_INFO("=== Vark Engine ===");

    Engine engine;

    if (!engine.initialize())
    {
        LOG_ERROR("Engine initialization failed!");
        system("pause");
        CoUninitialize();
        return -1;
    }

    // ---- Create and set the game ----
    auto game = std::make_unique<Game>();
    engine.set_game(std::move(game));

    engine.run();
    engine.shutdown();

    LOG_INFO("Engine shut down cleanly.");
    LOG_INFO("Console will stay open. Close this window to exit.");

    while (true) {
        Sleep(1000);
    }

    CoUninitialize();
    return 0;
}