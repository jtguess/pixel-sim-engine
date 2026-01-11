#pragma once

/*
 * ParallaxLayer.h
 *
 * Scrolling, tiled background layers with parallax effect.
 *
 * Usage:
 *  ParallaxLayer ocean;
 *  ocean.setTexture(waterTex, 64, 32); // tile size
 *  ocean.setScroll(30.0f, 0.0f);       // pixels per seconds
 *  ocean.setVerticalBob(4.0f, 2.0f);   // amplitude, frequency
 *
 * // In update:
 * ocean.update(deltaTime);
 *
 * // In render:
 * ocean.render(batch, 0, 180, 640, 180); // fill this region
 */

#include "TextureManager.h"
#include "Animation.h"
#include <vector>

class SpriteBatch;

class ParallaxLayer {
  public:
    ParallaxLayer() = default;

    /**
     * Set the texture for this layer.
     *
     * @param texture The texture (can be a sprite sheet for animation)
     * @param tileWidth Width of one tile (or frame)
     * @param tileHeight Height of one tile (or frame)
     */
    void setTexture(const TextureHandle& texture, float tileWidth, float tileHeight);

    /**
     * Set up animation if using a sprite sheet.
     * Frames are assumed to be arranged horizontally.
     *
     * @param frameCount Number of animation frames
     * @param frameDuration Seconds per frame
     */
    void setAnimation(int frameCount, float frameDuration);

    /**
     * Set scroll speed.
     *
     * @param speedX Horizontal scroll in pixels/second (negative = scroll left)
     * @param speedY Vertical scroll in pexels/seconds (negative = scroll up)
     */
    void setScroll(float speedX, float speedY = 0.0f);

    /**
     * Set vertical bobbing (for ocean swells).
     *
     * @param amplitude Max pixels to move up/down
     * @param frequency Oscillations per second
     * @param phase Starting phase offset (0-1)
     */
    void setVerticalBob(float amplitude, float frequency, float phase = 0.0f);

    /**
     * Set horizontal wave motion (tiles move in sine pattern)
     *
     * @param amplitude Max pixels to shift
     * @param frequency Waves per screen width
     * @param speed How fast the wave moves
     */
    void setWaveMotion(float amplitude, float frequency, float speed);

    /**
     * Set tint for this layer.
     */
    void setTint(Color tint) { m_tint = tint; }

    /**
     * Set alpha (0-255).
     */
    void setAlpha(uint8_t alpha) { m_tint.a = alpha; }

    /**
     * Update animation and scroll position.
     */
    void update(float dt);

    /**
     * Render the layer, tiling to fill the given region.
     *
     * @param batch SpriteBatch to draw with
     * @param x, y Top-left of region to fill
     * @param width, height Size of region to fill
     */
    void render(SpriteBatch& batch, float x, float y, float width, float height);

    // Getters
    float getScrollX() const { return m_scrollX; }
    float getScrollY() const { return m_scrollY; }

  private:
    TextureHandle m_texture;
    float m_tileWidth = 64.0f;
    float m_tileHeight = 64.0f;

    // Animation
    int m_frameCount = 1;
    float m_frameDuration = 0.1f;
    float m_animTime = 0.0f;
    int m_currentFrame = 0;

    // Scrolling
    float m_scrollSpeedX = 0.0f;
    float m_scrollSpeedY = 0.0f;
    float m_scrollX = 0.0f;
    float m_scrollY = 0.0f;

    // Vertical bob (swells)
    float m_bobAmplitude = 0.0f;
    float m_bobFrequency = 0.0f;
    float m_bobPhase = 0.0f;
    float m_bobOffset = 0.0f;

    // Wave motion
    float m_waveAmplitude = 0.0f;
    float m_waveFrequency = 0.0f;
    float m_waveSpeed = 0.0f;
    float m_waveTime = 0.0f;

    // Appearance
    Color m_tint = Color::white();

    // Time tracking
    float m_time = 0.0f;
};

/**
 * Manages multiple parallax layers.
 * Layers are rendered back-to-front (index 0 = furthest back).
 */
class ParallaxBackground {
  public:
    /**
     * Add a layer (rendered in order added, first = back).
     */
    void addLayer(ParallaxLayer layer);

    /**
     * Get a layer by index for modification.
     */
    ParallaxLayer& getLayer(size_t index) { return m_layers[index]; }
    const ParallaxLayer& getLayer(size_t index) const { return m_layers[index]; }

    /**
     * Get number of layers.
     */
    size_t layerCount() const { return m_layers.size(); }

    /**
     * Clear all layers.
     */
    void clear() { m_layers.clear(); }

    /**
     * Update all layers.
     */
    void update(float dt);

    /**
     * Render all layers to fill the given region.
     */
    void render(SpriteBatch& batch, float x, float y, float width, float height);

  private:
    std::vector<ParallaxLayer> m_layers;
};
