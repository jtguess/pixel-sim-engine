/*
 * TextureManager.cpp
 * 
 * Implementation of texture loading using stb_image.
 * 
 * Dependencies:
 *   - stb_image.h (via vcpkg)
 *   - bgfx
 */

#include "TextureManager.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstdio>
#include <cstring>
#include <cmath>

TextureManager::~TextureManager() {
    clear();
}

TextureManager::TextureManager(TextureManager&& other) noexcept
    : m_cache(std::move(other.m_cache))
{
    other.m_cache.clear();
}

TextureManager& TextureManager::operator=(TextureManager&& other) noexcept {
    if (this != &other) {
        clear();
        m_cache = std::move(other.m_cache);
        other.m_cache.clear();
    }
    return *this;
}

TextureHandle TextureManager::load(const std::string& path) {
    // Check cache first
    auto it = m_cache.find(path);
    if (it != m_cache.end()) {
        return it->second;
    }
    
    // Load image with stb_image
    int width, height, channels;
    
    // Force RGBA output for consistency
    stbi_set_flip_vertically_on_load(false); // Keep top-left origin
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    
    if (!pixels) {
        std::fprintf(stderr, "TextureManager: Failed to load '%s': %s\n", 
                     path.c_str(), stbi_failure_reason());
        return TextureHandle{};
    }
    
    // Convert RGBA to BGRA for bgfx (Metal prefers BGRA)
    const size_t pixelCount = (size_t)width * (size_t)height;
    for (size_t i = 0; i < pixelCount; ++i) {
        unsigned char* p = pixels + i * 4;
        std::swap(p[0], p[2]); // Swap R and B
    }
    
    // Create bgfx texture
    const bgfx::Memory* mem = bgfx::copy(pixels, (uint32_t)(width * height * 4));
    stbi_image_free(pixels);
    
    bgfx::TextureHandle tex = bgfx::createTexture2D(
        (uint16_t)width,
        (uint16_t)height,
        false,  // no mipmaps (pixel art looks better without)
        1,      // single layer
        bgfx::TextureFormat::BGRA8,
        BGFX_TEXTURE_NONE,  // flags set at sample time
        mem
    );
    
    if (!bgfx::isValid(tex)) {
        std::fprintf(stderr, "TextureManager: Failed to create texture for '%s'\n", path.c_str());
        return TextureHandle{};
    }
    
    TextureHandle handle;
    handle.texture = tex;
    handle.width   = (uint16_t)width;
    handle.height  = (uint16_t)height;
    
    m_cache[path] = handle;
    
    std::printf("TextureManager: Loaded '%s' (%dx%d)\n", path.c_str(), width, height);
    
    return handle;
}

TextureHandle TextureManager::createSolidColor(const std::string& name, 
                                                uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // Check cache first
    auto it = m_cache.find(name);
    if (it != m_cache.end()) {
        return it->second;
    }
    
    // Create 1x1 BGRA pixel
    uint8_t pixel[4] = { b, g, r, a }; // BGRA order
    
    const bgfx::Memory* mem = bgfx::copy(pixel, 4);
    
    bgfx::TextureHandle tex = bgfx::createTexture2D(
        1, 1,
        false, 1,
        bgfx::TextureFormat::BGRA8,
        BGFX_TEXTURE_NONE,
        mem
    );
    
    if (!bgfx::isValid(tex)) {
        std::fprintf(stderr, "TextureManager: Failed to create solid color texture '%s'\n", name.c_str());
        return TextureHandle{};
    }
    
    TextureHandle handle;
    handle.texture = tex;
    handle.width   = 1;
    handle.height  = 1;
    
    m_cache[name] = handle;
    
    return handle;
}

TextureHandle TextureManager::createFromRGBA(const std::string& name, uint16_t width, uint16_t height, const uint8_t* pixels) {
    // Check cache first
    auto it = m_cache.find(name);
    if (it != m_cache.end()) {
        return it->second;
    }
    
    // Convert RGBA to BGRA
    std::vector<uint8_t> bgra(width * height * 4);
    for (size_t i = 0; i < width * height; ++i) {
        bgra[i * 4 + 0] = pixels[i * 4 + 2]; // B
        bgra[i * 4 + 1] = pixels[i * 4 + 1]; // G
        bgra[i * 4 + 2] = pixels[i * 4 + 0]; // R
        bgra[i * 4 + 3] = pixels[i * 4 + 3]; // A
    }
    
    const bgfx::Memory* mem = bgfx::copy(bgra.data(), (uint32_t)bgra.size());
    
    bgfx::TextureHandle tex = bgfx::createTexture2D(
        width, height,
        false, 1,
        bgfx::TextureFormat::BGRA8,
        BGFX_TEXTURE_NONE,
        mem
    );
    
    if (!bgfx::isValid(tex)) {
        std::fprintf(stderr, "TextureManager: Failed to create texture '%s'\n", name.c_str());
        return TextureHandle{};
    }
    
    TextureHandle handle;
    handle.texture = tex;
    handle.width   = width;
    handle.height  = height;
    
    m_cache[name] = handle;
    
    return handle;
}

// Helper: HSV to RGB
static void hsvToRgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    
    float rf, gf, bf;
    if (h < 60)      { rf = c; gf = x; bf = 0; }
    else if (h < 120) { rf = x; gf = c; bf = 0; }
    else if (h < 180) { rf = 0; gf = c; bf = x; }
    else if (h < 240) { rf = 0; gf = x; bf = c; }
    else if (h < 300) { rf = x; gf = 0; bf = c; }
    else              { rf = c; gf = 0; bf = x; }
    
    r = (uint8_t)((rf + m) * 255);
    g = (uint8_t)((gf + m) * 255);
    b = (uint8_t)((bf + m) * 255);
}

TextureHandle TextureManager::createTestSpriteSheet(
    const std::string& name,
    uint16_t frameWidth,
    uint16_t frameHeight,
    int frameCount,
    const std::vector<uint32_t>& colors
) {
    // Check cache first
    auto it = m_cache.find(name);
    if (it != m_cache.end()) {
        return it->second;
    }
    
    uint16_t totalWidth = frameWidth * frameCount;
    std::vector<uint8_t> pixels(totalWidth * frameHeight * 4);
    
    for (int frame = 0; frame < frameCount; ++frame) {
        // Determine frame color
        uint8_t r, g, b;
        if (!colors.empty()) {
            uint32_t c = colors[frame % colors.size()];
            r = (c >> 16) & 0xFF;
            g = (c >> 8) & 0xFF;
            b = c & 0xFF;
        } else {
            // Generate rainbow color
            float hue = (frame / (float)frameCount) * 360.0f;
            hsvToRgb(hue, 0.8f, 0.9f, r, g, b);
        }
        
        // Fill frame pixels with a simple pattern
        for (int y = 0; y < frameHeight; ++y) {
            for (int x = 0; x < frameWidth; ++x) {
                int px = frame * frameWidth + x;
                int idx = (y * totalWidth + px) * 4;
                
                // Create a bordered square pattern
                bool border = (x < 2 || x >= frameWidth - 2 || y < 2 || y >= frameHeight - 2);
                
                // Add some frame-specific variation (pulsing effect)
                float intensity = border ? 0.5f : 1.0f;
                
                // Add a diagonal line that moves with frame index
                int diag = (x + y + frame * 4) % 16;
                if (diag < 4) intensity *= 0.7f;
                
                pixels[idx + 0] = (uint8_t)(r * intensity);
                pixels[idx + 1] = (uint8_t)(g * intensity);
                pixels[idx + 2] = (uint8_t)(b * intensity);
                pixels[idx + 3] = 255;
            }
        }
    }
    
    return createFromRGBA(name, totalWidth, frameHeight, pixels.data());
}

TextureHandle TextureManager::createTestSpriteSheet(
    const std::string& name,
    uint16_t frameWidth,
    uint16_t frameHeight,
    int frameCount,
    std::function<uint32_t(int frame, int x, int y)> generator
) {
    // Check cache first
    auto it = m_cache.find(name);
    if (it != m_cache.end()) {
        return it->second;
    }
    
    uint16_t totalWidth = frameWidth * frameCount;
    std::vector<uint8_t> pixels(totalWidth * frameHeight * 4);
    
    for (int frame = 0; frame < frameCount; ++frame) {
        for (int y = 0; y < frameHeight; ++y) {
            for (int x = 0; x < frameWidth; ++x) {
                int px = frame * frameWidth + x;
                int idx = (y * totalWidth + px) * 4;
                
                uint32_t color = generator(frame, x, y);
                
                pixels[idx + 0] = (color >> 16) & 0xFF; // R
                pixels[idx + 1] = (color >> 8) & 0xFF;  // G
                pixels[idx + 2] = color & 0xFF;         // B
                pixels[idx + 3] = (color >> 24) & 0xFF; // A
            }
        }
    }
    
    return createFromRGBA(name, totalWidth, frameHeight, pixels.data());
}

TextureHandle TextureManager::get(const std::string& path) const {
    auto it = m_cache.find(path);
    if (it != m_cache.end()) {
        return it->second;
    }
    return TextureHandle{};
}

void TextureManager::unload(const std::string& path) {
    auto it = m_cache.find(path);
    if (it != m_cache.end()) {
        if (bgfx::isValid(it->second.texture)) {
            bgfx::destroy(it->second.texture);
        }
        m_cache.erase(it);
    }
}

void TextureManager::clear() {
    for (auto& [path, handle] : m_cache) {
        if (bgfx::isValid(handle.texture)) {
            bgfx::destroy(handle.texture);
        }
    }
    m_cache.clear();
}
