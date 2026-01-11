/*
 * OceanSystem.cpp
 * 
 * Dynamic ocean with spawning swells.
 */

#include "OceanSystem.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

void OceanSystem::init(TextureManager& textures) {
    m_textures = &textures;
    m_swells.clear();
    m_swellTypes.clear();
    m_totalSpawnWeight = 0;
    
    // Create base water gradient texture
    m_baseTex = textures.createSolidColor("ocean_base", 
        m_baseColorTop.r, m_baseColorTop.g, m_baseColorTop.b);
    
    // Add default swell types - these use procedural textures as fallback
    // User can call clearSwellTypes() and addSwellType() to customize
    
    // Try to load custom sprites, fall back to procedural
    TextureHandle smallTex = textures.load("assets/swell_small.png");
    TextureHandle mediumTex = textures.load("assets/swell_med.png");
    TextureHandle largeTex = textures.load("assets/swell_large.png");
    TextureHandle crestTex = textures.load("assets/wave_crest.png");
    
    // Small swells
    if (smallTex.isValid()) {
        SwellType small;
        small.texturePath = "assets/swell_small.png";
        small.frameWidth = smallTex.width / 23.0f;
        small.frameHeight = (float)smallTex.height;
        small.frameCount = 23;
        small.frameDuration = 0.15f;
        small.minSpeed = -30; small.maxSpeed = -50;
        small.minScale = 0.8f; small.maxScale = 1.2f;
        small.spawnWeight = 2.0f;
        small.depthMin = 0.0f; small.depthMax = 1.0f;
        small.varyTint = true; small.tintVariation = 20;
        addSwellType(small);
        std::printf("OceanSystem: Loaded swell_small.png\n");
    }
    
    // Medium swells
    if (mediumTex.isValid()) {
        SwellType medium;
        medium.texturePath = "assets/swell_med.png";
        medium.frameWidth = mediumTex.width / 14.0f;
        medium.frameHeight = (float)mediumTex.height;
        medium.frameCount = 14;
        medium.frameDuration = 0.2f;
        medium.minSpeed = -20; medium.maxSpeed = -40;
        medium.minScale = 0.9f; medium.maxScale = 1.3f;
        medium.spawnWeight = 1.0f;
        medium.depthMin = 0.01f; medium.depthMax = 0.99f;
        medium.varyTint = true; medium.tintVariation = 15;
        addSwellType(medium);
        std::printf("OceanSystem: Loaded swell_medium.png\n");
    }
    
    // Large swells
    if (largeTex.isValid()) {
        SwellType large;
        large.texturePath = "assets/swell_large.png";
        large.frameWidth = largeTex.width / 4.0f;
        large.frameHeight = (float)largeTex.height;
        large.frameCount = 4;
        large.frameDuration = 0.25f;
        large.minSpeed = 15; large.maxSpeed = 30;
        large.minScale = 1.0f; large.maxScale = 1.5f;
        large.spawnWeight = 1.0f;
        large.depthMin = 0.3f; large.depthMax = 0.6f;
        large.varyTint = false; large.tintVariation = 0;
        //addSwellType(large);
        std::printf("OceanSystem: Loaded swell_large.png\n");
    }
    
    // Wave crests (foam)
    if (crestTex.isValid()) {
        SwellType crest;
        crest.texturePath = "assets/wave_crest.png";
        crest.frameWidth = crestTex.width / 34.0f;  // Assume 3 frames
        crest.frameHeight = (float)crestTex.height;
        crest.frameCount = 34;
        crest.frameDuration = 0.12f;
        crest.minSpeed = -40; crest.maxSpeed = -60;
        crest.minScale = 0.7f; crest.maxScale = 1.1f;
        crest.spawnWeight = 1.0f;
        crest.depthMin = 0.0f; crest.depthMax = 0.4f;  // Near surface
        crest.varyTint = false; crest.tintVariation = 0;
        addSwellType(crest);
        std::printf("OceanSystem: Loaded wave_crest.png\n");
    }
    
    // If no custom sprites loaded, create procedural ones
    if (m_swellTypes.empty()) {
        std::printf("OceanSystem: No swell sprites found, using procedural\n");
        
        // Procedural small wave
        textures.createTestSpriteSheet("proc_swell_small", 64, 24, 4,
            [](int frame, int x, int y) -> uint32_t {
                float cx = 32, cy = 16;
                float phase = frame * 0.25f * 3.14159f * 2;
                
                // Wave shape
                float waveY = cy + std::sin((x / 64.0f + phase) * 3.14159f) * 8.0f;
                float dist = std::abs(y - waveY);
                
                if (dist < 6) {
                    float alpha = (1.0f - dist / 6.0f);
                    uint8_t a = (uint8_t)(alpha * 180);
                    // Blue-white gradient
                    uint8_t r = (uint8_t)(100 + alpha * 100);
                    uint8_t g = (uint8_t)(140 + alpha * 80);
                    uint8_t b = (uint8_t)(180 + alpha * 60);
                    return (a << 24) | (r << 16) | (g << 8) | b;
                }
                return 0x00000000;
            }
        );
        
        SwellType procSmall;
        procSmall.texturePath = "proc_swell_small";
        procSmall.frameWidth = 64; procSmall.frameHeight = 24;
        procSmall.frameCount = 4; procSmall.frameDuration = 0.15f;
        procSmall.minSpeed = 30; procSmall.maxSpeed = 50;
        procSmall.minScale = 0.8f; procSmall.maxScale = 1.2f;
        procSmall.spawnWeight = 3.0f;
        procSmall.depthMin = 0.1f; procSmall.depthMax = 0.9f;
        procSmall.varyTint = true; procSmall.tintVariation = 20;
        addSwellType(procSmall);
        
        // Procedural medium wave
        textures.createTestSpriteSheet("proc_swell_medium", 128, 40, 4,
            [](int frame, int x, int y) -> uint32_t {
                float cx = 64, cy = 24;
                float phase = frame * 0.25f * 3.14159f * 2;
                
                float waveY = cy + std::sin((x / 128.0f + phase) * 3.14159f) * 14.0f;
                float dist = std::abs(y - waveY);
                
                if (dist < 10) {
                    float alpha = (1.0f - dist / 10.0f);
                    uint8_t a = (uint8_t)(alpha * 200);
                    uint8_t r = (uint8_t)(80 + alpha * 80);
                    uint8_t g = (uint8_t)(120 + alpha * 80);
                    uint8_t b = (uint8_t)(170 + alpha * 60);
                    return (a << 24) | (r << 16) | (g << 8) | b;
                }
                return 0x00000000;
            }
        );
        
        SwellType procMedium;
        procMedium.texturePath = "proc_swell_medium";
        procMedium.frameWidth = 128; procMedium.frameHeight = 40;
        procMedium.frameCount = 4; procMedium.frameDuration = 0.2f;
        procMedium.minSpeed = 20; procMedium.maxSpeed = 40;
        procMedium.minScale = 0.9f; procMedium.maxScale = 1.3f;
        procMedium.spawnWeight = 2.0f;
        procMedium.depthMin = 0.2f; procMedium.depthMax = 0.7f;
        procMedium.varyTint = true; procMedium.tintVariation = 15;
        addSwellType(procMedium);
        
        // Procedural foam crest
        textures.createTestSpriteSheet("proc_crest", 48, 16, 3,
            [](int frame, int x, int y) -> uint32_t {
                float phase = frame * 0.33f;
                float foamLine = 8 + std::sin((x * 0.2f + phase * 6.28f)) * 3;
                float dist = std::abs(y - foamLine);
                
                if (dist < 5) {
                    float alpha = (1.0f - dist / 5.0f) * (0.7f + std::sin(x * 0.5f) * 0.3f);
                    uint8_t a = (uint8_t)(alpha * 220);
                    return (a << 24) | 0xF0F8FF;  // White foam
                }
                return 0x00000000;
            }
        );
        
        SwellType procCrest;
        procCrest.texturePath = "proc_crest";
        procCrest.frameWidth = 48; procCrest.frameHeight = 16;
        procCrest.frameCount = 3; procCrest.frameDuration = 0.12f;
        procCrest.minSpeed = 40; procCrest.maxSpeed = 60;
        procCrest.minScale = 0.7f; procCrest.maxScale = 1.1f;
        procCrest.spawnWeight = 2.5f;
        procCrest.depthMin = 0.0f; procCrest.depthMax = 0.4f;
        procCrest.varyTint = false; procCrest.tintVariation = 0;
        addSwellType(procCrest);
    }
    
    std::printf("OceanSystem: Initialized with %zu swell types\n", m_swellTypes.size());
}

void OceanSystem::setRegion(float x, float y, float width, float height) {
    m_regionX = x;
    m_regionY = y;
    m_regionW = width;
    m_regionH = height;
}

void OceanSystem::setBaseColor(Color topColor, Color bottomColor) {
    m_baseColorTop = topColor;
    m_baseColorBottom = bottomColor;
    
    if (m_textures) {
        m_baseTex = m_textures->createSolidColor("ocean_base_updated",
            topColor.r, topColor.g, topColor.b);
    }
}

void OceanSystem::addSwellType(const SwellType& type) {
    LoadedSwellType loaded;
    loaded.config = type;
    
    // Load or get texture
    if (m_textures) {
        loaded.texture = m_textures->get(type.texturePath);
        if (!loaded.texture.isValid()) {
            loaded.texture = m_textures->load(type.texturePath);
        }
    }
    
    // Create animation
    loaded.animation = Animation::fromGrid(
        0, 0,
        type.frameWidth, type.frameHeight,
        type.frameCount,
        type.frameDuration,
        true  // loop
    );
    
    m_swellTypes.push_back(loaded);
    m_totalSpawnWeight += type.spawnWeight;
}

void OceanSystem::update(float dt) {
    if (m_swellTypes.empty()) return;
    
    // Update existing swells
    for (auto& swell : m_swells) {
        swell.sprite.update(dt);
        swell.x -= swell.speed * m_speedMultiplier * dt;
    }
    
    // Remove swells that have scrolled off screen
    m_swells.erase(
        std::remove_if(m_swells.begin(), m_swells.end(),
            [this](const ActiveSwell& s) {
                auto& type = m_swellTypes[s.typeIndex].config;
                return s.x < m_regionX - type.frameWidth * s.scale;
            }),
        m_swells.end()
    );
    
    // Spawn new swells
    m_spawnTimer += dt;
    float spawnInterval = 1.0f / m_swellDensity;
    
    while (m_spawnTimer >= spawnInterval) {
        m_spawnTimer -= spawnInterval;
        spawnSwell();
    }
}

void OceanSystem::spawnSwell() {
    if (m_swellTypes.empty() || m_totalSpawnWeight <= 0) return;
    
    // Pick random swell type based on weights
    float roll = randomFloat(0, m_totalSpawnWeight);
    int typeIndex = 0;
    float cumulative = 0;
    
    for (size_t i = 0; i < m_swellTypes.size(); ++i) {
        cumulative += m_swellTypes[i].config.spawnWeight;
        if (roll <= cumulative) {
            typeIndex = (int)i;
            break;
        }
    }
    
    auto& type = m_swellTypes[typeIndex];
    auto& config = type.config;
    
    ActiveSwell swell;
    swell.typeIndex = typeIndex;
    swell.sprite = AnimatedSprite(type.texture, type.animation);
    
    // Random starting frame for variety
    swell.sprite.setFrame(randomInt(0, config.frameCount - 1));
    
    // Position: spawn just off right edge
    swell.x = 0 - config.frameWidth;
    
    // Depth determines Y position
    swell.depth = randomFloat(config.depthMin, config.depthMax);
    swell.y = m_regionY + swell.depth * (m_regionH - config.frameHeight);
    
    // Random speed and scale within range
    swell.speed = randomFloat(config.minSpeed, config.maxSpeed);
    swell.scale = randomFloat(config.minScale, config.maxScale);
    
    // Optional tint variation
    if (config.varyTint && config.tintVariation > 0) {
        int variation = config.tintVariation;
        swell.tint = Color(
            (uint8_t)(255 - randomInt(0, variation)),
            (uint8_t)(255 - randomInt(0, variation)),
            (uint8_t)(255 - randomInt(0, variation / 2)),  // Less blue variation
            255
        );
    } else {
        swell.tint = Color::white();
    }
    
    m_swells.push_back(swell);
}

void OceanSystem::render(SpriteBatch& batch) {
    // Draw base water gradient
    // Simple: draw strips from top to bottom with interpolated color
    int strips = 50;
    float stripH = m_regionH / strips;
    
    for (int i = 0; i < strips; ++i) {
        float t = (float)i / (strips - 1);
        Color c(
            (uint8_t)(m_baseColorTop.r + t * (m_baseColorBottom.r - m_baseColorTop.r)),
            (uint8_t)(m_baseColorTop.g + t * (m_baseColorBottom.g - m_baseColorTop.g)),
            (uint8_t)(m_baseColorTop.b + t * (m_baseColorBottom.b - m_baseColorTop.b))
        );
        batch.draw(m_baseTex, m_regionX, m_regionY + i * stripH, m_regionW, stripH + 1, c);
    }
    
    // Sort swells by depth (back to front)
    std::sort(m_swells.begin(), m_swells.end(),
        [](const ActiveSwell& a, const ActiveSwell& b) {
            return a.depth < b.depth;
        });
    
    // Draw swells
    for (auto& swell : m_swells) {
        auto& config = m_swellTypes[swell.typeIndex].config;
        float w = config.frameWidth * swell.scale;
        float h = config.frameHeight * swell.scale;
        swell.sprite.draw(batch, swell.x, swell.y, w, h, swell.tint);
    }
}

float OceanSystem::randomFloat(float min, float max) {
    m_randomSeed = m_randomSeed * 1103515245 + 12345;
    float t = (float)(m_randomSeed % 10000) / 10000.0f;
    return min + t * (max - min);
}

int OceanSystem::randomInt(int min, int max) {
    m_randomSeed = m_randomSeed * 1103515245 + 12345;
    return min + (m_randomSeed % (max - min + 1));
}
