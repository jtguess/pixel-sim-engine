/*
 * SpriteBatch.cpp
 * 
 * Efficient batched sprite rendering implementation.
 */

#include "SpriteBatch.h"
#include "TextureManager.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
#include <bx/math.h>

// -------------------------
// File helpers (same as your main.cpp)
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
        std::fprintf(stderr, "SpriteBatch: Failed to read shader: %s\n", path);
        return BGFX_INVALID_HANDLE;
    }
    const bgfx::Memory* mem = bgfx::copy(bytes.data(), (uint32_t)bytes.size());
    return bgfx::createShader(mem);
}

// -------------------------
// Vertex layout (static)
// -------------------------
bgfx::VertexLayout& SpriteBatch::SpriteVertex::getLayout() {
    static bgfx::VertexLayout layout;
    static bool initialized = false;
    if (!initialized) {
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true) // normalized
        .end();
        initialized = true;
    }
    return layout;
}

// -------------------------
// SpriteBatch implementation
// -------------------------

SpriteBatch::~SpriteBatch() {
    shutdown();
}

bool SpriteBatch::init(const char* vsPath, const char* fsPath, uint32_t maxSprites) {
    // Load shaders
    bgfx::ShaderHandle vsh = loadShader(vsPath);
    bgfx::ShaderHandle fsh = loadShader(fsPath);
    
    if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) {
        if (bgfx::isValid(vsh)) bgfx::destroy(vsh);
        if (bgfx::isValid(fsh)) bgfx::destroy(fsh);
        std::fprintf(stderr, "SpriteBatch: Failed to load shaders\n");
        return false;
    }
    
    m_program = bgfx::createProgram(vsh, fsh, true);
    if (!bgfx::isValid(m_program)) {
        std::fprintf(stderr, "SpriteBatch: Failed to create program\n");
        return false;
    }
    
    // Create sampler uniform
    m_texUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    
    // Reserve sprite storage
    m_maxSprites = maxSprites;
    m_sprites.reserve(maxSprites);
    
    std::printf("SpriteBatch: Initialized (max %u sprites)\n", maxSprites);
    return true;
}

void SpriteBatch::shutdown() {
    if (bgfx::isValid(m_program)) {
        bgfx::destroy(m_program);
        m_program = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_texUniform)) {
        bgfx::destroy(m_texUniform);
        m_texUniform = BGFX_INVALID_HANDLE;
    }
    m_sprites.clear();
}

void SpriteBatch::begin(bgfx::ViewId viewId, uint16_t screenWidth, uint16_t screenHeight) {
    if (m_begun) {
        std::fprintf(stderr, "SpriteBatch: begin() called without end()\n");
        return;
    }
    
    m_begun = true;
    m_viewId = viewId;
    m_screenW = screenWidth;
    m_screenH = screenHeight;
    
    // Clear sprite queue
    m_sprites.clear();
    m_currentDepth = 0.0f;
    
    // Reset stats
    m_stats = Stats{};
    
    // Set up orthographic projection using bgfx helper
    // (0,0) at top-left, (screenWidth, screenHeight) at bottom-right
    float ortho[16];
    bx::mtxOrtho(ortho, 
        0.0f, (float)screenWidth,    // left, right
        (float)screenHeight, 0.0f,   // bottom, top (flipped for top-left origin)
        0.0f, 1000.0f,               // near, far
        0.0f,                        // offset
        bgfx::getCaps()->homogeneousDepth);  // API-specific depth
    
    // Identity view matrix
    float identity[16];
    bx::mtxIdentity(identity);
    
    bgfx::setViewTransform(viewId, identity, ortho);
}

void SpriteBatch::draw(const TextureHandle& texture, float x, float y, Color color) {
    if (!texture.isValid()) return;
    draw(texture, x, y, (float)texture.width, (float)texture.height, color);
}

void SpriteBatch::draw(const TextureHandle& texture, float x, float y,
                       float width, float height, Color color) {
    if (!texture.isValid()) return;
    
    // Full texture UV
    const float u0 = 0.0f, v0 = 0.0f;
    const float u1 = 1.0f, v1 = 1.0f;
    
    const float x0 = x, y0 = y;
    const float x1 = x + width, y1 = y + height;
    const uint32_t c = color.toABGR();
    
    SpriteVertex verts[4] = {
        { x0, y0, m_currentDepth, u0, v0, c }, // TL
        { x1, y0, m_currentDepth, u1, v0, c }, // TR
        { x0, y1, m_currentDepth, u0, v1, c }, // BL
        { x1, y1, m_currentDepth, u1, v1, c }, // BR
    };
    
    addQuad(texture.texture, verts, m_currentDepth);
}

void SpriteBatch::draw(const TextureHandle& texture, float x, float y,
                       float width, float height,
                       float rotation, float originX, float originY,
                       Color color) {
    if (!texture.isValid()) return;
    
    // Compute rotated corners
    const float ox = width * originX;
    const float oy = height * originY;
    
    const float cosR = std::cos(rotation);
    const float sinR = std::sin(rotation);
    
    // Corner offsets from origin (before rotation)
    // TL, TR, BL, BR
    float corners[4][2] = {
        { -ox,         -oy },
        { width - ox,  -oy },
        { -ox,         height - oy },
        { width - ox,  height - oy }
    };
    
    // Rotate and translate
    const uint32_t c = color.toABGR();
    SpriteVertex verts[4];
    
    for (int i = 0; i < 4; ++i) {
        float cx = corners[i][0];
        float cy = corners[i][1];
        
        verts[i].x = x + ox + (cx * cosR - cy * sinR);
        verts[i].y = y + oy + (cx * sinR + cy * cosR);
        verts[i].z = m_currentDepth;
        verts[i].color = c;
    }
    
    // UVs
    verts[0].u = 0.0f; verts[0].v = 0.0f; // TL
    verts[1].u = 1.0f; verts[1].v = 0.0f; // TR
    verts[2].u = 0.0f; verts[2].v = 1.0f; // BL
    verts[3].u = 1.0f; verts[3].v = 1.0f; // BR
    
    addQuad(texture.texture, verts, m_currentDepth);
}

void SpriteBatch::drawRegion(const TextureHandle& texture, float x, float y,
                              const Rect& srcRect, Color color) {
    if (!texture.isValid()) return;
    drawRegion(texture, x, y, srcRect.w, srcRect.h, srcRect, color);
}

void SpriteBatch::drawRegion(const TextureHandle& texture, float x, float y,
                              float dstWidth, float dstHeight,
                              const Rect& srcRect, Color color) {
    if (!texture.isValid()) return;
    
    // Convert pixel UV rect to normalized UV
    const float texW = (float)texture.width;
    const float texH = (float)texture.height;
    
    const float u0 = srcRect.x / texW;
    const float v0 = srcRect.y / texH;
    const float u1 = (srcRect.x + srcRect.w) / texW;
    const float v1 = (srcRect.y + srcRect.h) / texH;
    
    const float x0 = x, y0 = y;
    const float x1 = x + dstWidth, y1 = y + dstHeight;
    const uint32_t c = color.toABGR();
    
    SpriteVertex verts[4] = {
        { x0, y0, m_currentDepth, u0, v0, c },
        { x1, y0, m_currentDepth, u1, v0, c },
        { x0, y1, m_currentDepth, u0, v1, c },
        { x1, y1, m_currentDepth, u1, v1, c },
    };
    
    addQuad(texture.texture, verts, m_currentDepth);
}

void SpriteBatch::drawRegion(const TextureHandle& texture, float x, float y,
                              float dstWidth, float dstHeight,
                              const Rect& srcRect,
                              float rotation, float originX, float originY,
                              Color color) {
    if (!texture.isValid()) return;
    
    const float texW = (float)texture.width;
    const float texH = (float)texture.height;
    
    const float u0 = srcRect.x / texW;
    const float v0 = srcRect.y / texH;
    const float u1 = (srcRect.x + srcRect.w) / texW;
    const float v1 = (srcRect.y + srcRect.h) / texH;
    
    const float ox = dstWidth * originX;
    const float oy = dstHeight * originY;
    
    const float cosR = std::cos(rotation);
    const float sinR = std::sin(rotation);
    
    float corners[4][2] = {
        { -ox,            -oy },
        { dstWidth - ox,  -oy },
        { -ox,            dstHeight - oy },
        { dstWidth - ox,  dstHeight - oy }
    };
    
    float uvs[4][2] = {
        { u0, v0 }, { u1, v0 }, { u0, v1 }, { u1, v1 }
    };
    
    const uint32_t c = color.toABGR();
    SpriteVertex verts[4];
    
    for (int i = 0; i < 4; ++i) {
        float cx = corners[i][0];
        float cy = corners[i][1];
        
        verts[i].x = x + ox + (cx * cosR - cy * sinR);
        verts[i].y = y + oy + (cx * sinR + cy * cosR);
        verts[i].z = m_currentDepth;
        verts[i].u = uvs[i][0];
        verts[i].v = uvs[i][1];
        verts[i].color = c;
    }
    
    addQuad(texture.texture, verts, m_currentDepth);
}

void SpriteBatch::addQuad(bgfx::TextureHandle texture, const SpriteVertex verts[4], float depth) {
    if (m_sprites.size() >= m_maxSprites) {
        flush();
    }
    
    SpriteItem item;
    item.texture = texture;
    item.depth = depth;
    std::memcpy(item.vertices, verts, sizeof(item.vertices));
    
    m_sprites.push_back(item);
    
    // Increment depth slightly so sprites drawn later appear on top
    m_currentDepth += 0.001f;
}

void SpriteBatch::end() {
    if (!m_begun) {
        std::fprintf(stderr, "SpriteBatch: end() called without begin()\n");
        return;
    }
    
    flush();
    m_begun = false;
}

void SpriteBatch::flush() {
    if (m_sprites.empty()) return;
    
    // Sort by depth to maintain draw order (back to front)
    // Note: We intentionally do NOT sort by texture here because that would
    // break the painter's algorithm (things drawn later should appear on top).
    // The trade-off is more draw calls, but correct layering.
    std::stable_sort(m_sprites.begin(), m_sprites.end(),
        [](const SpriteItem& a, const SpriteItem& b) {
            return a.depth < b.depth;
        });
    
    const bgfx::VertexLayout& layout = SpriteVertex::getLayout();
    
    bgfx::TextureHandle currentTexture = BGFX_INVALID_HANDLE;
    std::vector<SpriteVertex> batchVerts;
    batchVerts.reserve(m_sprites.size() * 6); // 6 verts per quad (2 triangles)
    
    auto submitBatch = [&]() {
        if (batchVerts.empty() || !bgfx::isValid(currentTexture)) return;
        
        const uint32_t numVerts = (uint32_t)batchVerts.size();
        
        // Check transient buffer availability
        if (bgfx::getAvailTransientVertexBuffer(numVerts, layout) < numVerts) {
            std::fprintf(stderr, "SpriteBatch: Not enough transient VB space\n");
            return;
        }
        
        bgfx::TransientVertexBuffer tvb;
        bgfx::allocTransientVertexBuffer(&tvb, numVerts, layout);
        std::memcpy(tvb.data, batchVerts.data(), numVerts * sizeof(SpriteVertex));
        
        bgfx::setVertexBuffer(0, &tvb);
        
        // Set texture with point sampling (pixel art) and clamping
        const uint32_t samplerFlags =
            BGFX_SAMPLER_MIN_POINT |
            BGFX_SAMPLER_MAG_POINT |
            BGFX_SAMPLER_MIP_POINT |
            BGFX_SAMPLER_U_CLAMP |
            BGFX_SAMPLER_V_CLAMP;
        
        bgfx::setTexture(0, m_texUniform, currentTexture, samplerFlags);
        
        // Enable alpha blending
        bgfx::setState(
            BGFX_STATE_WRITE_RGB |
            BGFX_STATE_WRITE_A |
            BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
        );
        
        bgfx::submit(m_viewId, m_program);
        
        m_stats.drawCalls++;
        batchVerts.clear();
    };
    
    for (const SpriteItem& sprite : m_sprites) {
        // Texture changed? Flush current batch
        if (sprite.texture.idx != currentTexture.idx) {
            submitBatch();
            currentTexture = sprite.texture;
            m_stats.textureSwaps++;
        }
        
        // Convert quad (4 verts) to 2 triangles (6 verts)
        // Triangle 1: TL, TR, BL
        // Triangle 2: TR, BR, BL
        batchVerts.push_back(sprite.vertices[0]); // TL
        batchVerts.push_back(sprite.vertices[1]); // TR
        batchVerts.push_back(sprite.vertices[2]); // BL
        
        batchVerts.push_back(sprite.vertices[1]); // TR
        batchVerts.push_back(sprite.vertices[3]); // BR
        batchVerts.push_back(sprite.vertices[2]); // BL
        
        m_stats.spriteCount++;
    }
    
    // Flush remaining
    submitBatch();
    
    m_sprites.clear();
}
