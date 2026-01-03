#pragma once

/*
 * TextureManager.h
 * 
 * Manages texture loading, caching, and lifetime for the sprite system.
 * 
 * Features:
 *   - Loads PNG images into bgfx textures
 *   - Caches textures by path (no duplicate loads)
 *   - Handles texture destruction on shutdown
 *   - Provides texture metadata (dimensions)
 * 
 * Usage:
 *   TextureManager textures;
 *   TextureHandle handle = textures.load("assets/player.png");
 *   // Use handle.texture with SpriteBatch
 *   // Textures auto-destroyed when TextureManager is destroyed
 */

#include <bgfx/bgfx.h>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <vector>
#include <functional>

struct TextureHandle {
    bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
    uint16_t width  = 0;
    uint16_t height = 0;
    
    bool isValid() const { return bgfx::isValid(texture); }
};

class TextureManager {
public:
    TextureManager() = default;
    ~TextureManager();
    
    // Non-copyable (owns GPU resources)
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;
    
    // Move-able
    TextureManager(TextureManager&& other) noexcept;
    TextureManager& operator=(TextureManager&& other) noexcept;
    
    /**
     * Load a texture from a PNG file.
     * 
     * @param path Path to the PNG file
     * @return TextureHandle with valid texture, or invalid handle on failure
     * 
     * Notes:
     *   - Cached: calling load() twice with same path returns same handle
     *   - Supports RGBA and RGB PNGs (RGB converted to RGBA)
     *   - Textures use BGRA8 format for best Metal compatibility
     */
    TextureHandle load(const std::string& path);
    
    /**
     * Create a solid color 1x1 texture (useful for untextured colored quads).
     * 
     * @param name Unique name to cache this texture under
     * @param r, g, b, a Color components (0-255)
     * @return TextureHandle
     */
    TextureHandle createSolidColor(const std::string& name, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

    /**
     * Create a texture from raw RGBA pixel data.
     *
     * @param name Unique name to cache this texture under
     * @param width, height dimensions
     * @param pixels RGBA pixel data (4 bytes per pixel, width * height * 4 total)
     * @return TextureHandle
     */
    TextureHandle createFromRGBA(const std::string& name, uint16_t width, uint16_t height, const uint8_t* pixels);

    /**
     * Creat a test sprite sheet with colored frames.
     * Useful for testing animations without real art assets.
     *
     * @param name Unique name to cache this texture under
     * @param frameWidth Width of each frame
     * @param frameHeight Height of each frame
     * @param frameCount Number of frames (arranged horizontally)
     * @param colors Optional: specific colors for each frame (cycles if fewer than frameCount)
     * @return TextureHandle
     *
     * If colors are empty, generates a rainbow gradient of colors.
     */
    TextureHandle createTestSpriteSheet(
        const std::string& name,
        uint16_t frameWidth,
        uint16_t frameHeight,
        int frameCount,
        const std::vector<uint32_t>& colors = {}
        );

    /**
     * Create a test sprite sheet using a generator function.
     * Allows custom patterns per frame.
     *
     * @param name Unique name
     * @param frameWidth, frameHeight Frame dimensions
     * @param frameCount Number of frames
     * @param generator Function called for each pixel: (frameIndex, x, y) -> RGBA color
     * @return TextureHandle
     */
    TextureHandle createTestSpriteSheet(
        const std::string& name,
        uint16_t frameWidth,
        uint16_t frameHeight,
        int frameCount,
        std::function<uint32_t(int frame, int x, int y)> generator
        );
    
    /**
     * Get a previously loaded texture by path.
     * 
     * @param path The path used when loading
     * @return TextureHandle, or invalid handle if not found
     */
    TextureHandle get(const std::string& path) const;
    
    /**
     * Unload a specific texture (removes from cache and destroys GPU resource).
     */
    void unload(const std::string& path);
    
    /**
     * Unload all textures.
     */
    void clear();
    
private:
    std::unordered_map<std::string, TextureHandle> m_cache;
};
