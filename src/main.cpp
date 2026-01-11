/*
 * main.cpp - Pixel Sim Engine with Scene System
 * 
 * Step 5: Added Scene and SceneManager for organizing game screens.
 * 
 * Architecture:
 *   - 640x360 offscreen canvas (pixel-perfect game rendering)
 *   - SpriteBatch for efficient batched 2D drawing
 *   - Animation system for sprite sheet animations
 *   - Scene system for managing different game screens
 *   - Integer-scaled blit to window with letterboxing
 * 
 * Controls:
 *   - SPACE or Click: Switch between scenes
 *   - ESC: Quit
 */

#include <SDL.h>
#include <SDL_metal.h>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

#include "TextureManager.h"
#include "SpriteBatch.h"
#include "Animation.h"
#include "Scene.h"
#include "SailingScene.h"
#include "PortScene.h"

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <fstream>
#include <memory>

// -------------------------
// Constants
// -------------------------
static constexpr uint32_t GAME_W = 640;
static constexpr uint32_t GAME_H = 360;

static constexpr uint16_t VIEW_GAME = 0;
static constexpr uint16_t VIEW_BLIT = 1;

// -------------------------
// File helpers
// -------------------------
static std::vector<uint8_t> readFileBytes(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    f.seekg(0, std::ios::end);
    const std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> data((size_t)size);
    if (!f.read(reinterpret_cast<char*>(data.data()), size)) return {};
    return data;
}

static bgfx::ShaderHandle loadShader(const char* path) {
    auto bytes = readFileBytes(path);
    if (bytes.empty()) {
        std::fprintf(stderr, "Failed to read shader: %s\n", path);
        return BGFX_INVALID_HANDLE;
    }
    const bgfx::Memory* mem = bgfx::copy(bytes.data(), (uint32_t)bytes.size());
    return bgfx::createShader(mem);
}

static bgfx::ProgramHandle loadProgram(const char* vsPath, const char* fsPath) {
    bgfx::ShaderHandle vsh = loadShader(vsPath);
    bgfx::ShaderHandle fsh = loadShader(fsPath);
    if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) {
        if (bgfx::isValid(vsh)) bgfx::destroy(vsh);
        if (bgfx::isValid(fsh)) bgfx::destroy(fsh);
        return BGFX_INVALID_HANDLE;
    }
    return bgfx::createProgram(vsh, fsh, true);
}

// -------------------------
// Blit quad vertex
// -------------------------
struct BlitVertex {
    float x, y, z;
    float u, v;

    static bgfx::VertexLayout layout() {
        bgfx::VertexLayout l;
        l.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
        return l;
    }
};

// -------------------------
// Letterbox + integer scaling
// -------------------------
struct RectPx { int x, y, w, h; };

static RectPx computeIntegerScaledRect(int backW, int backH) {
    const int scaleX = backW / (int)GAME_W;
    const int scaleY = backH / (int)GAME_H;
    const int scale = (scaleX < scaleY) ? scaleX : scaleY;
    const int s = (scale <= 0) ? 1 : scale;
    const int w = (int)GAME_W * s;
    const int h = (int)GAME_H * s;
    return { (backW - w) / 2, (backH - h) / 2, w, h };
}

static void rectToClipQuad(const RectPx& r, int backW, int backH, BlitVertex out[4]) {
    auto pxToClipX = [&](float px) { return (px / (float)backW) * 2.0f - 1.0f; };
    auto pxToClipY = [&](float py) { return 1.0f - (py / (float)backH) * 2.0f; };

    const float x0 = pxToClipX((float)r.x);
    const float y0 = pxToClipY((float)r.y);
    const float x1 = pxToClipX((float)(r.x + r.w));
    const float y1 = pxToClipY((float)(r.y + r.h));

    out[0] = { x0, y0, 0.0f, 0.0f, 0.0f };
    out[1] = { x1, y0, 0.0f, 1.0f, 0.0f };
    out[2] = { x0, y1, 0.0f, 0.0f, 1.0f };
    out[3] = { x1, y1, 0.0f, 1.0f, 1.0f };
}

int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Pixel Sim Engine - Step 5 (Scene System)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

#if defined(__APPLE__)
    void* metal_view = SDL_Metal_CreateView(window);
    if (!metal_view) {
        std::fprintf(stderr, "SDL_Metal_CreateView failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    void* metal_layer = SDL_Metal_GetLayer(metal_view);
    if (!metal_layer) {
        std::fprintf(stderr, "SDL_Metal_GetLayer failed: %s\n", SDL_GetError());
        SDL_Metal_DestroyView(metal_view);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bgfx::renderFrame();
    bgfx::PlatformData pd{};
    pd.nwh = metal_layer;
    bgfx::setPlatformData(pd);
#else
#   error "This sample is macOS-specific for now."
#endif

    int backW = 0, backH = 0;
    SDL_GL_GetDrawableSize(window, &backW, &backH);
    if (backW <= 0 || backH <= 0) SDL_GetWindowSize(window, &backW, &backH);

    bgfx::Init init;
    init.type = bgfx::RendererType::Metal;
    init.vendorId = BGFX_PCI_ID_NONE;
    init.resolution.width  = (uint32_t)backW;
    init.resolution.height = (uint32_t)backH;
    init.resolution.reset  = BGFX_RESET_VSYNC;
    init.platformData = pd;

    if (!bgfx::init(init)) {
        std::fprintf(stderr, "bgfx::init failed\n");
#if defined(__APPLE__)
        SDL_Metal_DestroyView(metal_view);
#endif
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // --- Offscreen render target ---
    bgfx::TextureHandle gameColorTex = bgfx::createTexture2D(
        (uint16_t)GAME_W, (uint16_t)GAME_H, false, 1,
        bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT
    );
    bgfx::FrameBufferHandle gameFbo = bgfx::createFrameBuffer(1, &gameColorTex, true);

    // --- Load blit program ---
    bgfx::ProgramHandle blitProgram = loadProgram("shaders/bin/vs_blit.bin", "shaders/bin/fs_blit.bin");
    if (!bgfx::isValid(blitProgram)) {
        std::fprintf(stderr, "Failed to load blit shader program.\n");
        bgfx::shutdown();
#if defined(__APPLE__)
        SDL_Metal_DestroyView(metal_view);
#endif
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bgfx::UniformHandle u_tex = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
    const bgfx::VertexLayout blitLayout = BlitVertex::layout();

    // --- Initialize sprite system ---
    TextureManager textures;
    SpriteBatch sprites;
    
    if (!sprites.init("shaders/bin/vs_sprite.bin", "shaders/bin/fs_sprite.bin")) {
        std::fprintf(stderr, "Failed to initialize SpriteBatch.\n");
        bgfx::shutdown();
#if defined(__APPLE__)
        SDL_Metal_DestroyView(metal_view);
#endif
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // --- Initialize scene system ---
    SceneManager scenes;
    
    // Start with the sailing scene
    scenes.switchTo(std::make_unique<SailingScene>(textures));

    // View setup
    bgfx::setViewClear(VIEW_GAME, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);
    bgfx::setViewClear(VIEW_BLIT, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);

    // Timing
    uint32_t frameCount = 0;
    Uint64 lastTicks = SDL_GetPerformanceCounter();
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    
    std::printf("\n=== Pixel Sim Engine ===\n");
    std::printf("Press SPACE or Click to switch scenes\n");
    std::printf("Press ESC to quit\n\n");
    
    bool running = true;
    while (running) {
        Uint64 currentTicks = SDL_GetPerformanceCounter();
        float deltaTime = (float)(currentTicks - lastTicks) / (float)frequency;
        lastTicks = currentTicks;
        
        // Event handling
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
            
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }

            if (e.type == SDL_WINDOWEVENT &&
                (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                 e.window.event == SDL_WINDOWEVENT_RESIZED)) {
                SDL_GL_GetDrawableSize(window, &backW, &backH);
                if (backW <= 0 || backH <= 0) SDL_GetWindowSize(window, &backW, &backH);
                bgfx::reset((uint32_t)backW, (uint32_t)backH, BGFX_RESET_VSYNC);
            }
            
            // Pass events to scene
            scenes.handleEvent(e);
        }

        // --- Update scene ---
        scenes.update(deltaTime);

        // --- Render to game canvas ---
        bgfx::setViewFrameBuffer(VIEW_GAME, gameFbo);
        bgfx::setViewRect(VIEW_GAME, 0, 0, (uint16_t)GAME_W, (uint16_t)GAME_H);
        bgfx::touch(VIEW_GAME);
        
        sprites.begin(VIEW_GAME, (uint16_t)GAME_W, (uint16_t)GAME_H);
        
        // Let the scene render
        scenes.render(sprites);
        
        sprites.end();

        // --- Blit to backbuffer ---
        bgfx::setViewFrameBuffer(VIEW_BLIT, BGFX_INVALID_HANDLE);
        bgfx::setViewRect(VIEW_BLIT, 0, 0, (uint16_t)backW, (uint16_t)backH);

        const RectPx dst = computeIntegerScaledRect(backW, backH);
        BlitVertex quad[4];
        rectToClipQuad(dst, backW, backH, quad);

        bgfx::TransientVertexBuffer tvb;
        if (bgfx::getAvailTransientVertexBuffer(4, blitLayout) >= 4) {
            bgfx::allocTransientVertexBuffer(&tvb, 4, blitLayout);
            std::memcpy(tvb.data, quad, sizeof(quad));

            bgfx::setVertexBuffer(0, &tvb);

            const uint32_t samplerFlags =
                BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT |
                BGFX_SAMPLER_MIP_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;

            bgfx::setTexture(0, u_tex, gameColorTex, samplerFlags);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_PT_TRISTRIP);
            bgfx::submit(VIEW_BLIT, blitProgram);
        }

        bgfx::frame();
        frameCount++;
        
        // Print FPS every 5 seconds
        if (frameCount % 300 == 0) {
            std::printf("[Frame %u] Running...\n", frameCount);
        }
    }

    // Cleanup
    sprites.shutdown();
    textures.clear();
    bgfx::destroy(u_tex);
    bgfx::destroy(blitProgram);
    bgfx::destroy(gameFbo);
    bgfx::shutdown();

#if defined(__APPLE__)
    SDL_Metal_DestroyView(metal_view);
#endif
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    std::printf("Goodbye!\n");
    return 0;
}
