#pragma once

#include "SpriteBatch.h"
#include "TextureManager.h"
#include <vector>

// Defines a single animation
struct Animation {
  std::vector<Rect> frames;
  float frameDuration = 0.1f;
  bool loop = true;

  Animation() = default;

  // Convenience constructor for uniform grid-based sprite sheets
  // Creates frames from a grid starting at (startX, startY)
  static Animation fromGrid(
      float startX, float startY,
      float frameW, float frameH,
      int frameCount,
      float duration = 0.1f,
      bool looping = true
      ) {
    Animation anim;
    anim.frameDuration = duration;
    anim.loop = looping;
    anim.frames.reserve(frameCount);

    for (int i = 0; i < frameCount; ++i) {
      anim.frames.push_back(Rect(
            startX + i * frameW,
            startY,
            frameW,
            frameH
            ));
    }

    return anim;
  }

  // Vertical strip variant
  static Animation fromGridVertical(
      float startX, float startY,
      float frameW, float frameH,
      int frameCount,
      float duration = 0.1f,
      bool looping = true
      ) {
    Animation anim;
    anim.frameDuration = duration;
    anim.loop = looping;
    anim.frames.reserve(frameCount);

    for (int i = 0; i < frameCount; ++i) {
      anim.frames.push_back(Rect(
            startX,
            startY + i * frameH,
            frameW,
            frameH
            ));
    }

    return anim;
  }

  bool empty() const { return frames.empty(); }
  int frameCount() const { return (int)frames.size(); }
  float totalDuration() const { return frameDuration * frames.size(); }
};

// An instance of an animated sprite (tracks playback state)
class AnimatedSprite {
  public:
    AnimatedSprite() = default;

    AnimatedSprite(const TextureHandle& texture, const Animation& animation)
      : m_texture(texture)
      , m_animation(&animation)
      {}

    // Set/change the animation
    void setAnimation(const Animation& animation, bool resetTime = true) {
      m_animation = &animation;
      if (resetTime) {
        m_elapsed = 0.0f;
        m_currentFrame = 0;
        m_finished = false;
      }
    }

    // Update animation timing
    void update(float deltaTime) {
      if (!m_animation || m_animation->empty() || m_paused) return;
      if (m_finished && !m_animation->loop) return;

      m_elapsed += deltaTime * m_speed;

      // Calculate current frame
      float frameDur = m_animation->frameDuration;
      int totalFrames = m_animation->frameCount();

      if (m_animation->loop) {
        // Loop: wrap around
        float totalDur = frameDur * totalFrames;
        while (m_elapsed >= totalDur) {
          m_elapsed -= totalDur;
        }
        m_currentFrame = (int)(m_elapsed / frameDur);
      } else {
        // No loop: clamp to last frame
        m_currentFrame = (int)(m_elapsed / frameDur);
        if (m_currentFrame >= totalFrames) {
          m_currentFrame = totalFrames - 1;
          m_finished = true;
        }
      }
    }

    // Draw at position
    void draw(SpriteBatch& batch, float x, float y, Color tint = Color::white()) const {
      if (!m_animation || m_animation->empty() || !m_texture.isValid()) return;

      const Rect& srcRect = m_animation->frames[m_currentFrame];
      batch.drawRegion(m_texture, x, y, srcRect, tint);
    }

    // Draw with custom size
    void draw(SpriteBatch& batch, float x, float y, float width, float height, Color tint = Color::white()) const {
      if (!m_animation || m_animation->empty() || !m_texture.isValid()) return;

      const Rect& srcRect = m_animation->frames[m_currentFrame];
      batch.drawRegion(m_texture, x, y, width, height, srcRect, tint);
    }

    // Draw with rotation
    void draw(SpriteBatch& batch, float x, float y, float width, float height, float rotation, float originX = 0.5f, float originY = 0.5f, Color tint = Color::white()) const {
      if (!m_animation || m_animation-> empty() || !m_texture.isValid()) return;

      const Rect& srcRect = m_animation->frames[m_currentFrame];
      batch.drawRegion(m_texture, x, y, width, height, srcRect, rotation, originX, originY, tint);
    }

    // Playback controls
    void play() { m_paused = false; }
    void pause() { m_paused = true; }
    void stop() { m_paused = true; m_elapsed = 0.0f; m_currentFrame = 0; m_finished = false; }
    void restart() { m_elapsed = 0.0f; m_currentFrame = 0; m_finished = false; m_paused = false; }

    // Speed multiplier (1.0 = normal, 2.0 = double, 0.5 = half)
    void setSpeed(float speed) { m_speed = speed; }
    float getSpeed() const { return m_speed; }

    // Jump to specific frame
    void setFrame(int frame) {
      if (m_animation && frame >= 0 && frame < m_animation->frameCount()) {
        m_currentFrame = frame;
        m_elapsed = frame * m_animation->frameDuration;
      }
    }

    // State queries
    int getCurrentFrame() const { return m_currentFrame; }
    float getElapsed() const { return m_elapsed; }
    bool isFinished() const { return m_finished; }
    bool isPaused() const { return m_paused; }
    bool isPlaying() const { return !m_paused && !m_finished; }

    const Animation* getAnimation() const { return m_animation; }
    const TextureHandle& getTexture() const { return m_texture; }

  private:
    TextureHandle m_texture;
    const Animation* m_animation = nullptr;

    float m_elapsed = 0.0f;
    int m_currentFrame = 0;
    float m_speed = 1.0f;
    bool m_paused = false;
    bool m_finished = false;

};
