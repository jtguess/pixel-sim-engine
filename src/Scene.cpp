/*
 * Scene.cpp
 *
 * SceneManager implementation.
 */

#include "Scene.h"
#include "SpriteBatch.h"
#include <SDL.h>

SceneManager::~SceneManager() {
  if (m_currentScene) {
    m_currentScene->onExit();
    m_currentScene.reset();
  }
  m_queuedScene.reset();
}

void SceneManager::switchTo(std::unique_ptr<Scene> scene) {
  // Exit current scene
  if (m_currentScene) {
    m_currentScene->onExit();
  }

  // Switch to new scene
  m_currentScene = std::move(scene);

  if (m_currentScene) {
    m_currentScene->setManager(this);
    m_currentScene->onEnter();
  }
}

void SceneManager::queueSwitch(std::unique_ptr<Scene> scene) {
  m_queuedScene = std::move(scene);
  m_hasPendingSwitch = true;
}

void SceneManager::update(float dt) {
  // Process any queued scene switch first
  processQueuedSwitch();

  if (m_currentScene) {
    m_currentScene->update(dt);
  }

  // Process again in case update() queued a switch
  processQueuedSwitch();
}

void SceneManager::render(SpriteBatch& batch) {
  if (m_currentScene) {
    m_currentScene->render(batch);
  }
}

bool SceneManager::handleEvent(const SDL_Event& event) {
  if (m_currentScene) {
    return m_currentScene->handleEvent(event);
  }
  return false;
}

void SceneManager::processQueuedSwitch() {
  if (m_hasPendingSwitch) {
    m_hasPendingSwitch = false;
    switchTo(std::move(m_queuedScene));
  }
}
