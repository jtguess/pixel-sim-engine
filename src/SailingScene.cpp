/*
 * SailingScene.cpp
 * 
 * A ship sailing across a dynamic ocean with spawning swells.
 */

#include "SailingScene.h"
#include "SpriteBatch.h"
#include "Scene.h"
#include "PortScene.h"
#include <SDL.h>
#include <cmath>
#include <cstdio>

// Screen dimensions
static constexpr float GAME_W = 640.0f;
static constexpr float GAME_H = 360.0f;

// Ocean region
static constexpr float HORIZON_Y = 160.0f;
static constexpr float OCEAN_HEIGHT = GAME_H - HORIZON_Y;

SailingScene::SailingScene(TextureManager& textures)
    : m_textures(textures)
{
    createTextures();
    loadAssets();
    
    // Initialize ocean system
    m_ocean.init(textures);
    m_ocean.setRegion(0, HORIZON_Y, GAME_W, OCEAN_HEIGHT);
    m_ocean.setBaseColor(
        Color(40, 80, 140),    // Top: lighter blue at horizon
        Color(15, 35, 80)      // Bottom: darker blue
    );
    m_ocean.setSwellDensity(1.0f);   // Swells per second
    m_ocean.setScrollSpeed(1.0f);
}

void SailingScene::createTextures() {
    // Sky gradient
    m_skyTex = m_textures.createTestSpriteSheet("sailing_sky", 1, (int)HORIZON_Y, 1,
        [](int frame, int x, int y) -> uint32_t {
            float t = y / 160.0f;
            uint8_t r = (uint8_t)(100 + t * 80);
            uint8_t g = (uint8_t)(160 + t * 60);
            uint8_t b = (uint8_t)(220 + t * 30);
            return 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    );
    
    // Cloud
    m_cloudTex = m_textures.createTestSpriteSheet("cloud", 48, 24, 1,
        [](int frame, int x, int y) -> uint32_t {
            const int cx = 24, cy = 14;
            auto inCircle = [](int px, int py, int ccx, int ccy, int r) {
                int dx = px - ccx, dy = py - ccy;
                return (dx*dx + dy*dy) < (r*r);
            };
            
            bool inCloud = inCircle(x, y, cx, cy, 12) ||
                           inCircle(x, y, cx - 14, cy + 2, 8) ||
                           inCircle(x, y, cx + 14, cy + 2, 9) ||
                           inCircle(x, y, cx - 8, cy - 4, 7) ||
                           inCircle(x, y, cx + 8, cy - 3, 8);
            
            if (inCloud) {
                float shade = 1.0f - (y - cy + 12) / 30.0f * 0.15f;
                uint8_t v = (uint8_t)(255 * shade);
                return 0xDD000000 | (v << 16) | (v << 8) | v;
            }
            return 0x00000000;
        }
    );
    
    // Spray particle
    m_sprayTex = m_textures.createTestSpriteSheet("spray", 8, 8, 1,
        [](int frame, int x, int y) -> uint32_t {
            int cx = 4, cy = 4;
            int dx = x - cx, dy = y - cy;
            float dist = std::sqrt((float)(dx*dx + dy*dy));
            
            if (dist < 3.5f) {
                float alpha = (1.0f - dist / 3.5f) * 200;
                return ((uint32_t)alpha << 24) | 0xFFFFFF;
            }
            return 0x00000000;
        }
    );
    
    // Initialize clouds
    m_clouds = {
        { 100, 20, 12.0f, 1.0f },
        { 280, 45, 8.0f, 0.8f },
        { 450, 25, 15.0f, 1.1f },
        { 180, 70, 6.0f, 0.6f },
        { 550, 40, 10.0f, 0.9f },
    };
}

void SailingScene::loadAssets() {
    // Try to load custom ship sprite
    m_shipSheet = m_textures.load("assets/cog_water.png");
    
    if (m_shipSheet.isValid()) {
        std::printf("SailingScene: Loaded cog_water.png (%dx%d)\n", 
                    m_shipSheet.width, m_shipSheet.height);
        // Assuming 128x128 frame
        m_shipAnim = Animation::fromGrid(0, 0, 128, 128, 10, 0.2f, true);
    } else {
        std::printf("SailingScene: Using procedural ship\n");
        m_shipSheet = m_textures.createTestSpriteSheet("proc_ship", 48, 40, 4,
            [](int frame, int x, int y) -> uint32_t {
                const int w = 48, h = 40, cx = w / 2;
                float rock = std::sin(frame * 1.57f) * 0.08f;
                int rx = x - cx, ry = y - h + 10;
                float cosR = std::cos(rock), sinR = std::sin(rock);
                int tx = (int)(rx * cosR - ry * sinR) + cx;
                int ty = (int)(rx * sinR + ry * cosR) + h - 10;
                
                bool inHull = (ty > h - 16) && (ty < h - 4) &&
                              (tx > 6 + (ty - (h-16)) * 0.5f) && 
                              (tx < w - 6 - (ty - (h-16)) * 0.5f);
                bool inDeck = (ty > h - 20) && (ty < h - 14) && (tx > 8) && (tx < w - 8);
                bool inCabin = (ty > h - 28) && (ty < h - 18) && (tx > cx - 6) && (tx < cx + 6);
                bool inMast = (tx > cx - 1) && (tx < cx + 2) && (ty > 4) && (ty < h - 18);
                float sailBillow = std::sin(frame * 0.8f) * 2.0f;
                bool inSail = (ty > 8) && (ty < h - 22) &&
                              (tx > cx + 2) && (tx < cx + 18 + sailBillow - (ty - 8) * 0.3f);
                
                if (inHull) return 0xFF654321;
                if (inDeck) return 0xFF8B7355;
                if (inCabin) return 0xFFDEB887;
                if (inMast) return 0xFF4A3728;
                if (inSail) return 0xFFF5F5DC;
                return 0x00000000;
            }
        );
        m_shipAnim = Animation::fromGrid(0, 0, 48, 40, 4, 0.2f, true);
    }
    
    m_ship = AnimatedSprite(m_shipSheet, m_shipAnim);
}

void SailingScene::onEnter() {
    std::printf("SailingScene: Entered (press SPACE to dock at port)\n");
    m_time = 0.0f;
    m_shipX = GAME_W * 0.3f;
    m_shipBaseY = HORIZON_Y - 30.0f;
    m_spray.clear();
    m_sprayTimer = 0.0f;
}

void SailingScene::onExit() {
    std::printf("SailingScene: Exited\n");
}

void SailingScene::update(float dt) {
    m_time += dt;
    
    // Update ship animation
    m_ship.update(dt);
    
    // Ship bobs in place (or moves slowly)
    // m_shipX += dt * 10.0f;
    // if (m_shipX > GAME_W + 100) m_shipX = -100;
    
    // Update ocean
    m_ocean.update(dt);
    
    // Update clouds
    for (auto& cloud : m_clouds) {
        cloud.x += cloud.speed * dt;
        if (cloud.x < -60) {
            cloud.x = GAME_W + 60;
        }
    }
    
    // Update spray
    for (auto& p : m_spray) {
        p.life -= dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.vy += 60.0f * dt;
    }
    
    m_spray.erase(
        std::remove_if(m_spray.begin(), m_spray.end(),
            [](const SprayParticle& p) { return p.life <= 0; }),
        m_spray.end()
    );
}

void SailingScene::render(SpriteBatch& batch) {
    // Sky
    batch.draw(m_skyTex, 0, 0, GAME_W, HORIZON_Y);
    
    // Clouds
    for (const auto& cloud : m_clouds) {
        float w = 48 * cloud.scale;
        float h = 24 * cloud.scale;
        batch.draw(m_cloudTex, cloud.x - w/2, cloud.y, w, h);
    }
    
    // Ocean with dynamic swells
    m_ocean.render(batch);
    
    // Ship
    float shipBob = std::sin(m_time * 2.0f) * 3.0f;
    float shipRock = std::sin(m_time * 1.5f) * 0.01f;
    float shipDrawY = m_shipBaseY + shipBob;
    
    float shipW, shipH;
    if (m_shipSheet.width > 100) {
        shipW = 160; shipH = 160;
    } else {
        shipW = 72; shipH = 60;
    }
    
    m_ship.draw(batch, m_shipX, shipDrawY, shipW, shipH, shipRock, 0.5f, 0.8f);
    
    // Spray
    for (const auto& p : m_spray) {
        float alpha = p.life / p.maxLife;
        uint8_t a = (uint8_t)(alpha * 180);
        float size = 5.0f + (1.0f - alpha) * 4.0f;
        batch.draw(m_sprayTex, p.x, p.y, size, size, Color(255, 255, 255, a));
    }
}

bool SailingScene::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) {
        if (m_manager) {
            m_manager->queueSwitch(std::make_unique<PortScene>(m_textures));
        }
        return true;
    }
    
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (m_manager) {
            m_manager->queueSwitch(std::make_unique<PortScene>(m_textures));
        }
        return true;
    }
    
    return false;
}
