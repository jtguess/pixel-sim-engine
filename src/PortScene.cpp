/*
 * PortScene.cpp
 * 
 * A ship docked at a port with buildings and crane.
 */

#include "PortScene.h"
#include "SpriteBatch.h"
#include "Scene.h"
#include <SDL.h>
#include <cmath>
#include <cstdio>

// Forward declare SailingScene for switching
#include "SailingScene.h"

// Screen dimensions
static constexpr float GAME_W = 640.0f;
static constexpr float GAME_H = 360.0f;

PortScene::PortScene(TextureManager& textures)
    : m_textures(textures)
{
    // Sky - warmer sunset-ish color for port
    m_skyTex = m_textures.createTestSpriteSheet("port_sky", 1, 180, 1,
        [](int frame, int x, int y) -> uint32_t {
            float t = y / 180.0f;
            uint8_t r = (uint8_t)(180 + t * 50);   // Warm orange tint
            uint8_t g = (uint8_t)(160 + t * 60);
            uint8_t b = (uint8_t)(140 + t * 80);
            return 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    );
    
    // Calm water for harbor
    m_waterTex = m_textures.createTestSpriteSheet("port_water", 64, 32, 4,
        [](int frame, int x, int y) -> uint32_t {
            // Calmer water than sailing scene
            float wave = std::sin((x + frame * 4) * 0.15f) * 2.0f;
            float depth = (y + wave) / 32.0f;
            
            uint8_t r = (uint8_t)(40 + (1.0f - depth) * 30);
            uint8_t g = (uint8_t)(70 + (1.0f - depth) * 50);
            uint8_t b = (uint8_t)(110 + (1.0f - depth) * 50);
            
            return 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    );
    
    // Wooden dock
    m_dockTex = m_textures.createTestSpriteSheet("port_dock", 32, 32, 1,
        [](int frame, int x, int y) -> uint32_t {
            // Wood plank pattern
            bool plankGap = (y % 8 == 0) || (x % 32 < 2);
            
            if (plankGap) {
                return 0xFF3D2817; // Dark gap between planks
            }
            
            // Wood grain variation
            int grain = (x * 3 + y * 7) % 20;
            uint8_t r = (uint8_t)(139 - grain);
            uint8_t g = (uint8_t)(90 - grain/2);
            uint8_t b = (uint8_t)(43 - grain/3);
            
            return 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    );
    
    // Building (warehouse style)
    m_buildingTex = m_textures.createTestSpriteSheet("port_building", 80, 100, 1,
        [](int frame, int x, int y) -> uint32_t {
            // Roof (top 20 pixels)
            if (y < 20) {
                bool roofTile = ((x + y) / 8) % 2 == 0;
                return roofTile ? 0xFF8B4513 : 0xFF6B3510;
            }
            
            // Windows
            bool inWindow = (y > 30) && (y < 50) && ((x > 15 && x < 30) || (x > 50 && x < 65));
            bool inWindow2 = (y > 60) && (y < 80) && ((x > 15 && x < 30) || (x > 50 && x < 65));
            
            if (inWindow || inWindow2) {
                return 0xFF87CEEB; // Light blue window
            }
            
            // Door
            bool inDoor = (y > 70) && (x > 32) && (x < 48);
            if (inDoor) {
                return 0xFF4A3728;
            }
            
            // Brick pattern
            int brickX = x / 16;
            int brickY = y / 8;
            bool brickEdge = (x % 16 == 0) || (y % 8 == 0);
            
            // Offset every other row
            if (brickY % 2 == 1) {
                brickEdge = ((x + 8) % 16 == 0) || (y % 8 == 0);
            }
            
            if (brickEdge) {
                return 0xFF808080; // Mortar
            }
            
            return 0xFFB22222; // Brick red
        }
    );
    
    // Crane
    m_craneTex = m_textures.createTestSpriteSheet("port_crane", 40, 80, 1,
        [](int frame, int x, int y) -> uint32_t {
            // Vertical beam
            bool inBeam = (x > 16) && (x < 24) && (y > 10);
            
            // Horizontal arm
            bool inArm = (y > 6) && (y < 14) && (x > 10);
            
            // Base
            bool inBase = (y > 70) && (x > 8) && (x < 32);
            
            // Cable
            bool inCable = (x > 34) && (x < 38) && (y > 12) && (y < 60);
            
            // Hook
            bool inHook = (y > 55) && (y < 65) && (x > 32) && (x < 40);
            
            if (inBeam || inArm || inBase) return 0xFFFFD700; // Yellow crane
            if (inCable) return 0xFF404040; // Dark cable
            if (inHook) return 0xFF808080; // Gray hook
            
            return 0x00000000;
        }
    );
    
    // Reuse ship from sailing scene if it exists, otherwise create new
    TextureHandle existingShip = m_textures.get("sailing_ship");
    if (existingShip.isValid()) {
        m_shipSheet = existingShip;
    } else {
        // Create ship sprite (same as sailing scene)
        m_shipSheet = m_textures.createTestSpriteSheet("port_ship", 48, 40, 4,
            [](int frame, int x, int y) -> uint32_t {
                const int w = 48, h = 40;
                const int cx = w / 2;
                
                float rock = std::sin(frame * 1.57f) * 0.05f; // Less rocking in port
                int rx = x - cx;
                int ry = y - h + 10;
                float cosR = std::cos(rock);
                float sinR = std::sin(rock);
                int tx = (int)(rx * cosR - ry * sinR) + cx;
                int ty = (int)(rx * sinR + ry * cosR) + h - 10;
                
                bool inHull = (ty > h - 16) && (ty < h - 4) &&
                              (tx > 6 + (ty - (h-16)) * 0.5f) && 
                              (tx < w - 6 - (ty - (h-16)) * 0.5f);
                bool inDeck = (ty > h - 20) && (ty < h - 14) &&
                              (tx > 8) && (tx < w - 8);
                bool inCabin = (ty > h - 28) && (ty < h - 18) &&
                               (tx > cx - 6) && (tx < cx + 6);
                bool inMast = (tx > cx - 1) && (tx < cx + 2) &&
                              (ty > 4) && (ty < h - 18);
                // Sail furled in port
                bool inSail = (ty > 10) && (ty < h - 24) &&
                              (tx > cx + 2) && (tx < cx + 8);
                
                if (inHull) return 0xFF654321;
                if (inDeck) return 0xFF8B7355;
                if (inCabin) return 0xFFDEB887;
                if (inMast) return 0xFF4A3728;
                if (inSail) return 0xFFD4C4A8; // Furled sail
                
                return 0x00000000;
            }
        );
    }
    
    // Set up ship animation (slower in port)
    m_shipAnim = Animation::fromGrid(0, 0, 48, 40, 4, 0.4f, true);
    m_ship = AnimatedSprite(m_shipSheet, m_shipAnim);
}

void PortScene::onEnter() {
    std::printf("PortScene: Entered (press SPACE to set sail)\n");
    m_time = 0.0f;
}

void PortScene::onExit() {
    std::printf("PortScene: Exited\n");
}

void PortScene::update(float dt) {
    m_time += dt;
    m_ship.update(dt);
}

void PortScene::render(SpriteBatch& batch) {
    // Draw sky
    batch.draw(m_skyTex, 0, 0, GAME_W, GAME_H * 0.5f);
    
    // Draw buildings in background
    batch.draw(m_buildingTex, 50, GAME_H * 0.35f - 100, 100, 130);
    batch.draw(m_buildingTex, 180, GAME_H * 0.35f - 80, 80, 110);
    batch.draw(m_buildingTex, 450, GAME_H * 0.35f - 90, 90, 120);
    
    // Draw crane
    batch.draw(m_craneTex, 350, GAME_H * 0.35f - 80, 60, 120);
    
    // Draw water
    int waterFrame = ((int)(m_time * 3)) % 4;
    Rect waterSrc(waterFrame * 64, 0, 64, 32);
    for (float wx = 0; wx < GAME_W; wx += 64) {
        batch.drawRegion(m_waterTex, wx, GAME_H * 0.5f, 64, GAME_H * 0.5f, waterSrc);
    }
    
    // Draw dock
    for (float dx = 0; dx < 200; dx += 32) {
        batch.draw(m_dockTex, dx, GAME_H * 0.45f, 32, 40);
    }
    
    // Draw ship (docked, gentle bob)
    float bobY = std::sin(m_time * 1.5f) * 2.0f;
    m_ship.draw(batch, 120, GAME_H * 0.42f + bobY, 72, 60);
}

bool PortScene::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) {
        if (m_manager) {
            m_manager->queueSwitch(std::make_unique<SailingScene>(m_textures));
        }
        return true;
    }
    
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (m_manager) {
            m_manager->queueSwitch(std::make_unique<SailingScene>(m_textures));
        }
        return true;
    }
    
    return false;
}
