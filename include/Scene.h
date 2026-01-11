#pragma once

/*
 * Scene.h
 *
 * Simple scene managment system for organizing game screens
 *
 * Usage:
 *  // Define your screens
 *  class MenuScene : public Scene {
 *  public:
 *    void update(float dt) override { ... }
 *    void render(SpriteBatch& batch) override { ... }
 *  };
 *
 * // In main:
 * SceneManager scenes;
 * scenes.switchTo(std::make_unique<Menu_Scene>());
 *
 * // In game loop:
 * scenes.update(deltaTime);
 * scenes.render(spriteBatch);
 */

#include <memory>
#include <functional>

// Forward declarations
class SpriteBatch;
class SceneManager;
union SDL_Event;

// Base class for all scenes
class Scene {
  public:
    virtual ~Scene() = default;

    // Called when this scene becomes active
    virtual void onEnter() {}

    // Called when leaving this scene (before destruction or switch)
    virtual void onExit() {}

    // Called every frame to update game logic
    virtual void update(float dt) = 0;

    // Called every frame to render
    // @param batch The SpriteBatch to draw with (already has begin() called)
    virtual void render(SpriteBatch& batch) = 0;

    // Called for each SDL event (input handling)
    // Return true if the event was consumed (stop propagation)
    virtual bool handleEvent(const SDL_Event& event) { return false; }

    // Set by SceneManager - allows scene to request transitions
    void setManager(SceneManager* manager) { m_manager = manager; }

  protected:
    SceneManager* m_manager = nullptr;
};

// Manages scene transitions and lifecycle
class SceneManager {
  public:
    SceneManager() = default;
    ~SceneManager();

    // Non-copyable
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    /**
     * Switch to a new scene.
     * The current scene's onExit() is called, then it's destroyed.
     * The new scene's onEnter() is called.
     *
     * @param scene The new scene (takes ownership)
     */
    void switchTo(std::unique_ptr<Scene> scene);

    /**
     * Queue a scene switch for the end of the current frame.
     * Useful when switching scenes from within a scene's update().
     *
     * @param scene The scene (takes ownership)
     */
    void queueSwitch(std::unique_ptr<Scene> scene);

    /**
     * Update the current scene.
     * Also processes any queued scene switches.
     *
     * @param dt Delta time in seconds
     */
    void update(float dt);

    /**
     * Render the current scene.
     *
     * @param batch The SpriteBatch to draw with (already has begin() called)
     */
    void render(SpriteBatch& batch);

    /**
     * Pass an SDL event to the current scene.
     *
     * @param event The SDL event
     * @return true if the event was consumed
     */
    bool handleEvent(const SDL_Event& event);

    /**
     * Check if there's an active scene.
     */
    bool hasScene() const { return m_currentScene != nullptr; }

    /**
     * Get the current scene (may be mullptr).
     */
    Scene* getCurrentScene() { return m_currentScene.get(); }

  private:
    void processQueuedSwitch();

    std::unique_ptr<Scene> m_currentScene;
    std::unique_ptr<Scene> m_queuedScene;
    bool m_hasPendingSwitch = false;
};
