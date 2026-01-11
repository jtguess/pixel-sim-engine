#pragma once

/*
 * OceanSystem.h
 * 
 * Dynamic ocean rendering with spawning swells and waves.
 * 
 * Usage:
 *   OceanSystem ocean;
 *   ocean.init(textureManager);
 *   ocean.setRegion(0, 180, 640, 180);  // Where ocean appears
 * 
 *   // In update:
 *   ocean.update(deltaTime);
 * 
 *   // In render:
 *   ocean.render(spriteBatch);
 */

#include "TextureManager.h"
#include "Animation.h"
#include "SpriteBatch.h"
#include <vector>
#include <string>

// Configuration for a swell type
struct SwellType {
  std::string texturePath;
  float frameWidth;
  float frameHeight;
  int frameCount;
  float frameDuration;

  // Spawn settings
  float minSpeed;
  float maxSpeed;
  float minScale;
  float maxScale;
  float spawnWeight;
  float depthMin;
  float depthMax;

  // Optional tint variation
  bool varyTint;
  uint8_t tintVariation;      // How much R/G/B can  vary
};

class OceanSystem {
  public:
    OceanSystem() = default;

    /**
     * Initialize with texture manager and load swell sprites.
     * Call this after bgfx is initialized.
     */
    void init(TextureManager& textures);

    /**
     * Set the region where ocean is rendered.
     */
    void setRegion(float x, float y, float width, float height);
    
    /**
     * Set base water color (drawn behind swells).
     */
    void setBaseColor(Color topColor, Color bottomColor);

    /**
     * Set how busy the ocean is.
     * @param density Swells per second to spawn (e.g., 2.0 = 2 swells/sec)
     */
    void setSwellDensity(float density) { m_swellDensity = density; }

    /**
     * Set overall scroll speed multiplier.
     */
    void setScrollSpeed(float multiplier) { m_speedMultiplier = multiplier; }

    /**
     * Add a custom swell type.
     */
    void addSwellType(const SwellType& type);

    /**
     * Clear all swell types (to replace with custom ones).
     */
    void clearSwellTypes() { m_swellTypes.clear(); }

    /**
     * Update swell spawning and movement.
     */
    void update(float dt);

    /**
     * Render the ocean.
     */
    void render(SpriteBatch& batch);

    /**
     * Get number of active swells (for debugging).
     */
    size_t activeSwellCount() const { return m_swells.size(); }

  private:
    struct LoadedSwellType {
      SwellType config;
      TextureHandle texture;
      Animation animation;
    };

    struct ActiveSwell {
      int typeIndex;
      AnimatedSprite sprite;
      float x, y;
      float speed;
      float scale;
      float depth;
      Color tint;
    };

    void spawnSwell();
    float randomFloat(float min, float max);
    int randomInt(int min, int max);

    TextureManager* m_textures = nullptr;

    // Region
    float m_regionX = 0;
    float m_regionY = 180;
    float m_regionW = 640;
    float m_regionH = 180;

    // Base water
    Color m_baseColorTop = Color(30, 60, 120);
    Color m_baseColorBottom = Color(10, 30, 60);
    TextureHandle m_baseTex;

    // Swell types
    std::vector<LoadedSwellType> m_swellTypes;
    float m_totalSpawnWeight = 0;

    // Active swells
    std::vector<ActiveSwell> m_swells;

    // Spawning
    float m_swellDensity = 3.0f;    // Swells per second
    float m_spawnTimer = 0;
    float m_speedMultiplier = 1.0f;

    // For random variation
    unsigned int m_randomSeed = 12345;
};
