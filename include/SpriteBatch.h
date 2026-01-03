#pragma once

/*
 * SpriteBatch.h
 * 
 * Efficient batched 2D sprite rendering for bgfx.
 * 
 * Features:
 *   - Batches sprites by texture to minimize draw calls
 *   - Supports position, scale, rotation, tint color
 *   - Handles alpha blending
 *   - Automatic depth sorting (optional)
 * 
 * Usage:
 *   SpriteBatch batch;
 *   batch.init();
 *   
 *   // In your render loop:
 *   batch.begin(viewId, screenWidth, screenHeight);
 *   batch.draw(texture, x, y);
 *   batch.draw(texture, x, y, width, height, color, rotation);
 *   batch.drawRegion(texture, x, y, srcX, srcY, srcW, srcH); // sprite sheets
 *   batch.end();
 * 
 * Performance tips:
 *   - Draw sprites with the same texture consecutively when possible
 *   - The batcher auto-sorts by texture, but pre-sorting saves work
 *   - Default max batch size is 8192 sprites (configurable)
 */

#include <bgfx/bgfx.h>
#include <cstdint>
#include <vector>

// Forward declare
struct TextureHandle;

// RGBA color packed into uint32_t (ABGR byte order for bgfx)
struct Color {
    uint8_t r, g, b, a;
    
    Color() : r(255), g(255), b(255), a(255) {}
    Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255) 
        : r(r_), g(g_), b(b_), a(a_) {}
    
    // Convert to packed ABGR for vertex attribute
    uint32_t toABGR() const {
        return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
    }
    
    // Common colors
    static Color white()       { return Color(255, 255, 255); }
    static Color black()       { return Color(0, 0, 0); }
    static Color red()         { return Color(255, 0, 0); }
    static Color green()       { return Color(0, 255, 0); }
    static Color blue()        { return Color(0, 0, 255); }
    static Color yellow()      { return Color(255, 255, 0); }
    static Color transparent() { return Color(0, 0, 0, 0); }
};

// Rectangle for UV regions (sprite sheets)
struct Rect {
    float x, y, w, h;
    
    Rect() : x(0), y(0), w(0), h(0) {}
    Rect(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}
};

class SpriteBatch {
public:
    // Max sprites per batch (can be overridden in init)
    static constexpr uint32_t DEFAULT_MAX_SPRITES = 8192;
    
    SpriteBatch() = default;
    ~SpriteBatch();
    
    // Non-copyable
    SpriteBatch(const SpriteBatch&) = delete;
    SpriteBatch& operator=(const SpriteBatch&) = delete;
    
    /**
     * Initialize the sprite batch.
     * 
     * @param vsPath Path to compiled vertex shader (.bin)
     * @param fsPath Path to compiled fragment shader (.bin)
     * @param maxSprites Maximum sprites per batch (affects memory usage)
     * @return true on success
     */
    bool init(const char* vsPath, const char* fsPath, 
              uint32_t maxSprites = DEFAULT_MAX_SPRITES);
    
    /**
     * Shutdown and release GPU resources.
     */
    void shutdown();
    
    /**
     * Begin a new batch.
     * 
     * Sets up orthographic projection where (0,0) is top-left.
     * 
     * @param viewId bgfx view ID to render to
     * @param screenWidth Width of render target in pixels
     * @param screenHeight Height of render target in pixels
     */
    void begin(bgfx::ViewId viewId, uint16_t screenWidth, uint16_t screenHeight);
    
    /**
     * Draw a sprite (full texture).
     * 
     * @param texture The texture to draw
     * @param x, y Position (top-left corner)
     * @param color Tint color (default white = no tint)
     */
    void draw(const TextureHandle& texture, float x, float y, 
              Color color = Color::white());
    
    /**
     * Draw a sprite with custom size.
     */
    void draw(const TextureHandle& texture, float x, float y, 
              float width, float height,
              Color color = Color::white());
    
    /**
     * Draw a sprite with rotation.
     * 
     * @param rotation Rotation in radians (clockwise)
     * @param originX, originY Rotation origin relative to sprite (0-1, default center)
     */
    void draw(const TextureHandle& texture, float x, float y,
              float width, float height,
              float rotation, float originX = 0.5f, float originY = 0.5f,
              Color color = Color::white());
    
    /**
     * Draw a region of a texture (for sprite sheets/atlases).
     * 
     * @param srcRect Region of texture in pixels
     */
    void drawRegion(const TextureHandle& texture, float x, float y,
                    const Rect& srcRect,
                    Color color = Color::white());
    
    /**
     * Draw a region with custom destination size.
     */
    void drawRegion(const TextureHandle& texture, float x, float y,
                    float dstWidth, float dstHeight,
                    const Rect& srcRect,
                    Color color = Color::white());
    
    /**
     * Draw a region with rotation.
     */
    void drawRegion(const TextureHandle& texture, float x, float y,
                    float dstWidth, float dstHeight,
                    const Rect& srcRect,
                    float rotation, float originX = 0.5f, float originY = 0.5f,
                    Color color = Color::white());
    
    /**
     * End the batch and submit all draw calls.
     */
    void end();
    
    /**
     * Statistics from last frame.
     */
    struct Stats {
        uint32_t spriteCount = 0;
        uint32_t drawCalls   = 0;
        uint32_t textureSwaps = 0;
    };
    
    Stats getStats() const { return m_stats; }
    
private:
    // Vertex format for sprites
    struct SpriteVertex {
        float x, y, z;      // Position
        float u, v;         // Texture coordinates
        uint32_t color;     // ABGR packed color
        
        static bgfx::VertexLayout& getLayout();
    };
    
    // A queued sprite
    struct SpriteItem {
        bgfx::TextureHandle texture;
        SpriteVertex vertices[4];
        float depth; // For sorting (lower = behind)
    };
    
    void flush();
    void addQuad(bgfx::TextureHandle texture, const SpriteVertex verts[4], float depth);
    
    // GPU resources
    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_texUniform = BGFX_INVALID_HANDLE;
    
    // Batch state
    bool m_begun = false;
    bgfx::ViewId m_viewId = 0;
    uint16_t m_screenW = 0;
    uint16_t m_screenH = 0;
    
    // Sprite queue
    std::vector<SpriteItem> m_sprites;
    uint32_t m_maxSprites = DEFAULT_MAX_SPRITES;
    float m_currentDepth = 0.0f;
    
    // Stats
    Stats m_stats;
};
