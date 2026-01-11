#pragma once

/*
 * SailingScene.h
 * 
 * Example scene: A ship sailing across the ocean.
 * Press SPACE or click to switch to PortScene.
 */

#include "Scene.h"
#include "Animation.h"
#include "TextureManager.h"
#include "ParallaxLayer.h"
#include "OceanSystem.h"

class SailingScene : public Scene {
  public:
    SailingScene(TextureManager& textures);

    void onEnter() override;
    void onExit() override;
    void update(float dt) override;
    void render(SpriteBatch& batch) override;
    bool handleEvent(const SDL_Event& event) override;

  private:
    void createTextures();
    void loadAssets();

    TextureManager& m_textures;

    // Ship
    TextureHandle m_shipSheet;
    Animation m_shipAnim;
    AnimatedSprite m_ship;
    float m_shipX = 0.0f;
    float m_shipBaseY = 0.0f;

    // Sky
    TextureHandle m_skyTex;

    // Dynamic ocean
    OceanSystem m_ocean;

    // Clouds (simple Parallax)
    TextureHandle m_cloudTex;
    struct Cloud {
      float x, y;
      float speed;
      float scale;
    };
    std::vector<Cloud> m_clouds;

    // Spray particles
    struct SprayParticle {
      float x, y;
      float vx, vy;
      float life;
      float maxLife;
    };
    std::vector<SprayParticle> m_spray;
    TextureHandle m_sprayTex;
    float m_sprayTimer = 0.0f;

    // State
    float m_time = 0.0f;
};
