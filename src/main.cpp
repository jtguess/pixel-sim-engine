/*
 * main.cpp - Pixel Sim Engine with Animation System
 * 
 * Step 4: Added Animation and AnimatedSprite for frame-based animations.
 * 
 * Architecture:
 *   - 640x360 offscreen canvas (pixel-perfect game rendering)
 *   - SpriteBatch for efficient batched 2D drawing
 *   - Animation system for sprite sheet animations
 *   - Integer-scaled blit to window with letterboxing
 */

#include <SDL.h>
#include <SDL_metal.h>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

#include "TextureManager.h"
#include "SpriteBatch.h"
#include "Animation.h"

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <fstream>

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
        "Pixel Sim Engine - Step 4 (Animation System)",
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

    // --- Create test sprite sheets ---
    
    // Rainbow animation (8 frames, default rainbow colors)
    TextureHandle rainbowSheet = textures.createTestSpriteSheet("rainbow", 32, 32, 8);
    
    // Ship-like animation (simple boat shape that rocks)
    TextureHandle shipSheet = textures.createTestSpriteSheet("ship", 48, 32, 6,
        [](int frame, int x, int y) -> uint32_t {
            // Create a simple boat shape
            const int w = 48, h = 32;
            const int centerX = w / 2;
            
            // Rock angle based on frame
            float rock = std::sin(frame * 0.5f) * 0.15f;
            
            // Transform coordinates for rocking
            int rx = x - centerX;
            int ry = y - h / 2;
            float cosR = std::cos(rock);
            float sinR = std::sin(rock);
            int tx = (int)(rx * cosR - ry * sinR) + centerX;
            int ty = (int)(rx * sinR + ry * cosR) + h / 2;
            
            // Hull shape (bottom half, roughly boat-shaped)
            bool inHull = (ty > h/2 - 4) && (ty < h - 4) && 
                          (tx > 8 + (ty - h/2)) && (tx < w - 8 - (ty - h/2));
            
            // Cabin (small rectangle on top)
            bool inCabin = (ty > h/2 - 10) && (ty < h/2) && 
                           (tx > centerX - 8) && (tx < centerX + 8);
            
            // Mast
            bool inMast = (tx > centerX - 2) && (tx < centerX + 2) && 
                          (ty > 4) && (ty < h/2 - 4);
            
            // Flag (waves with frame)
            int flagOffset = (frame % 3) - 1;
            bool inFlag = (ty > 4) && (ty < 12) && 
                          (tx > centerX + 2) && (tx < centerX + 12 + flagOffset);
            
            // Water line at bottom
            bool inWater = (y > h - 6);
            
            if (inWater) {
                // Animated water
                int wave = (x + frame * 4) % 8;
                if (wave < 4) return 0xFF4488CC; // Light blue
                return 0xFF3366AA; // Darker blue
            }
            if (inHull) return 0xFF8B4513; // Brown
            if (inCabin) return 0xFFDEB887; // Tan
            if (inMast) return 0xFF654321; // Dark brown
            if (inFlag) return 0xFFFF4444; // Red flag
            
            // Sky background gradient
            float skyGrad = (float)y / h;
            uint8_t skyB = (uint8_t)(200 + skyGrad * 55);
            uint8_t skyG = (uint8_t)(180 + skyGrad * 40);
            return 0xFF000000 | (skyB << 16) | (skyG << 8) | 0xFF; // Light blue sky
        }
    );

    std::printf("Rainbow sheet: %dx%d, valid=%d\n", rainbowSheet.width, rainbowSheet.height, rainbowSheet.isValid());
    std::printf("Ship sheet: %dx%d, valid=%d\n", shipSheet.width, shipSheet.height, shipSheet.isValid());
    
    // Pulsing orb animation
    TextureHandle orbSheet = textures.createTestSpriteSheet("orb", 24, 24, 8,
        [](int frame, int x, int y) -> uint32_t {
            const int size = 24;
            const int cx = size / 2;
            const int cy = size / 2;
            
            float dx = (float)(x - cx);
            float dy = (float)(y - cy);
            float dist = std::sqrt(dx * dx + dy * dy);
            
            // Pulsing radius
            float pulse = 8.0f + std::sin(frame * 0.8f) * 3.0f;
            
            if (dist < pulse) {
                // Inside orb - gradient from center
                float intensity = 1.0f - (dist / pulse) * 0.5f;
                
                // Color shifts with frame
                float hue = frame * 45.0f; // Rotate through colors
                float r = std::sin(hue * 0.0174533f) * 0.5f + 0.5f;
                float g = std::sin((hue + 120) * 0.0174533f) * 0.5f + 0.5f;
                float b = std::sin((hue + 240) * 0.0174533f) * 0.5f + 0.5f;
                
                return 0xFF000000 |
                    ((uint32_t)(b * intensity * 255) << 16) |
                    ((uint32_t)(g * intensity * 255) << 8) |
                    (uint32_t)(r * intensity * 255);
            }
            
            // Transparent outside
            return 0x00000000;
        }
    );
    
    // Simple solid background
    TextureHandle bgTex = textures.createSolidColor("bg", 30, 40, 60);
    
    // --- Create animations ---
    Animation rainbowAnim = Animation::fromGrid(0, 0, 32, 32, 8, 0.1f, true);
    Animation shipAnim = Animation::fromGrid(0, 0, 48, 32, 6, 0.15f, true);
    Animation orbAnim = Animation::fromGrid(0, 0, 24, 24, 8, 0.08f, true);
    
    // --- Create animated sprites ---
    AnimatedSprite rainbowSprite(rainbowSheet, rainbowAnim);
    AnimatedSprite shipSprite(shipSheet, shipAnim);
    AnimatedSprite orbSprite1(orbSheet, orbAnim);
    AnimatedSprite orbSprite2(orbSheet, orbAnim);
    AnimatedSprite orbSprite3(orbSheet, orbAnim);
    
    // Offset orb animations so they're not in sync
    orbSprite2.setFrame(3);
    orbSprite3.setFrame(6);
    
    // Different playback speeds
    orbSprite1.setSpeed(1.0f);
    orbSprite2.setSpeed(1.5f);
    orbSprite3.setSpeed(0.7f);

    // View setup
    bgfx::setViewClear(VIEW_GAME, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x1e3050ff, 1.0f, 0);
    bgfx::setViewClear(VIEW_BLIT, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);

    // Timing
    float time = 0.0f;
    uint32_t frameCount = 0;
    Uint64 lastTicks = SDL_GetPerformanceCounter();
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    
    bool running = true;
    while (running) {
        Uint64 currentTicks = SDL_GetPerformanceCounter();
        float deltaTime = (float)(currentTicks - lastTicks) / (float)frequency;
        lastTicks = currentTicks;
        time += deltaTime;
        
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;

            if (e.type == SDL_WINDOWEVENT &&
                (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                 e.window.event == SDL_WINDOWEVENT_RESIZED)) {
                SDL_GL_GetDrawableSize(window, &backW, &backH);
                if (backW <= 0 || backH <= 0) SDL_GetWindowSize(window, &backW, &backH);
                bgfx::reset((uint32_t)backW, (uint32_t)backH, BGFX_RESET_VSYNC);
            }
        }

        // --- Update animations ---
        rainbowSprite.update(deltaTime);
        shipSprite.update(deltaTime);
        orbSprite1.update(deltaTime);
        orbSprite2.update(deltaTime);
        orbSprite3.update(deltaTime);

        // --- Render to game canvas ---
        bgfx::setViewFrameBuffer(VIEW_GAME, gameFbo);
        bgfx::setViewRect(VIEW_GAME, 0, 0, (uint16_t)GAME_W, (uint16_t)GAME_H);
        bgfx::touch(VIEW_GAME);
        
        sprites.begin(VIEW_GAME, (uint16_t)GAME_W, (uint16_t)GAME_H);
        
        // Draw tiled background
        for (int y = 0; y < (int)GAME_H; y += 32) {
            for (int x = 0; x < (int)GAME_W; x += 32) {
                sprites.draw(bgTex, (float)x, (float)y, 32.0f, 32.0f);
            }
        }
        // Draw animated sprites
        
        // Rainbow squares in a row
        rainbowSprite.draw(sprites, 50, 50, 64, 64);
        rainbowSprite.draw(sprites, 130, 50, 48, 48);
        rainbowSprite.draw(sprites, 200, 50, 32, 32);
        
        // Ship sailing across (moves horizontally)
        float shipX = std::fmod(time * 50.0f, GAME_W + 100) - 50;
        shipSprite.draw(sprites, shipX, 150, 96, 64);
        
        // Floating orbs
        float bobOffset1 = std::sin(time * 2.0f) * 10.0f;
        float bobOffset2 = std::sin(time * 2.5f + 1.0f) * 10.0f;
        float bobOffset3 = std::sin(time * 1.8f + 2.0f) * 10.0f;
        
        orbSprite1.draw(sprites, 400, 100 + bobOffset1, 48, 48);
        orbSprite2.draw(sprites, 470, 120 + bobOffset2, 48, 48);
        orbSprite3.draw(sprites, 540, 90 + bobOffset3, 48, 48);
        
        // Show current frame info
        if (frameCount % 300 == 0) {
            auto stats = sprites.getStats();
            std::printf("[Frame %u] Sprites: %u, Draw calls: %u | Rainbow frame: %d, Ship frame: %d\n",
                        frameCount, stats.spriteCount, stats.drawCalls,
                        rainbowSprite.getCurrentFrame(), shipSprite.getCurrentFrame());
        }
        
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
    return 0;
}
