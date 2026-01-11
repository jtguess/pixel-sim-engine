#pragma once

/*
 * PortScene.h
 * 
 * Example scene: A ship docked at a port.
 * Press SPACE or click to set sail again.
 */

#include "Scene.h"
#include "Animation.h"
#include "TextureManager.h"

class PortScene : public Scene {
public:
    PortScene(TextureManager& textures);
    
    void onEnter() override;
    void onExit() override;
    void update(float dt) override;
    void render(SpriteBatch& batch) override;
    bool handleEvent(const SDL_Event& event) override;
    
private:
    TextureManager& m_textures;
    
    // Textures
    TextureHandle m_skyTex;
    TextureHandle m_waterTex;
    TextureHandle m_dockTex;
    TextureHandle m_buildingTex;
    TextureHandle m_shipSheet;
    TextureHandle m_craneTex;
    
    // Animations
    Animation m_shipAnim;
    AnimatedSprite m_ship;
    
    // State
    float m_time = 0.0f;
};
