/*
 * ParallaxLayer.cpp
 *
 * Implementation of parallax scrolling layers.
 */

#include "ParallaxLayer.h"
#include "SpriteBatch.h"
#include <cmath>

void ParallaxLayer::setTexture(const TextureHandle& texture, float tileWidth, float tileHeight) {
  m_texture = texture;
  m_tileWidth = tileWidth;
  m_tileHeight = tileHeight;
  m_frameCount = 1;
  m_currentFrame = 0;
}

void ParallaxLayer::setAnimation(int frameCount, float frameDuration) {
  m_frameCount = frameCount;
  m_frameDuration = frameDuration;
  m_animTime = 0.0f;
  m_currentFrame = 0;
}

void ParallaxLayer::setScroll(float speedX, float speedY) {
  m_scrollSpeedX = speedX;
  m_scrollSpeedY = speedY;
}

void ParallaxLayer::setVerticalBob(float amplitude, float frequency, float phase) {
  m_bobAmplitude = amplitude;
  m_bobFrequency = frequency;
  m_bobPhase = phase;
}

void ParallaxLayer::setWaveMotion(float amplitude, float frequency, float speed) {
  m_waveAmplitude = amplitude;
  m_waveFrequency = frequency;
  m_waveSpeed = speed;
}

void ParallaxLayer::update(float dt) {
  m_time += dt;

  // Update scroll position
  m_scrollX += m_scrollSpeedX * dt;
  m_scrollY += m_scrollSpeedY * dt;

  // Wrap scroll to avoid floating point precision issues
  if (m_tileWidth > 0) {
    while (m_scrollX >= m_tileWidth) m_scrollX -= m_tileWidth;
    while (m_scrollX < 0) m_scrollX += m_tileWidth;
  }

  if (m_tileHeight > 0) {
    while (m_scrollY >= m_tileHeight) m_scrollY -= m_tileHeight;
    while (m_scrollY < 0) m_scrollY += m_tileHeight;
  }

  // Update vertical bob
  if (m_bobAmplitude > 0) {
    m_bobOffset = std::sin((m_time * m_bobFrequency + m_bobPhase) * 2.0f * 3.14159f) * m_bobAmplitude;
  }

  // Update wave time
  m_waveTime += m_waveSpeed * dt;

  // Update animation
  if (m_frameCount > 1) {
    m_animTime += dt;
    while (m_animTime >= m_frameDuration) {
      m_animTime -= m_frameDuration;
      m_currentFrame = (m_currentFrame + 1) % m_frameCount;
    }
  }
}

void ParallaxLayer::render(SpriteBatch& batch, float x, float y, float width, float height) {
  if (!m_texture.isValid() || m_tileWidth <= 0 || m_tileHeight <= 0) return;

  // Calculate the source rect for current animation frame
  Rect srcRect(
      m_currentFrame * m_tileWidth,
      0,
      m_tileWidth,
      m_tileHeight
      );

  // Calculate starting position with scroll offset
  float startX = x - std::fmod(m_scrollX, m_tileWidth);
  float startY = y - std::fmod(m_scrollY, m_tileHeight) + m_bobOffset;

  // Need to start one tile earlier to handle scroll
  if (m_scrollX > 0) startX -= m_tileWidth;
  if (m_scrollY > 0) startY -=  m_tileHeight;

  // Tile across the region
  for (float ty = startY; ty < y + height; ty += m_tileHeight) {
    for (float tx = startX; tx < x + width + m_tileWidth; tx += m_tileWidth) {
      float drawX = tx;
      float drawY = ty;

      // Apply wave motion (horizontal offset based on vertical position)
      if (m_waveAmplitude > 0) {
        float waveOffset = std::sin((ty * m_waveFrequency / 100.0f + m_waveTime) * 2.0f * 3.14159f) * m_waveAmplitude;
        drawX += waveOffset;
      }

      batch.drawRegion(m_texture, drawX, drawY, m_tileWidth, m_tileHeight, srcRect, m_tint);
    }
  }
}

// ============================================
// ParallaxBackground
// ============================================

void ParallaxBackground::addLayer(ParallaxLayer layer) {
  m_layers.push_back(std::move(layer));
}

void ParallaxBackground::update(float dt) {
  for (auto& layer : m_layers) {
    layer.update(dt);
  }
}

void ParallaxBackground::render(SpriteBatch& batch, float x, float y, float width, float height) {
  // Render back to front
  for (auto& layer : m_layers) {
    layer.render(batch, x, y, width, height);
  }
}


