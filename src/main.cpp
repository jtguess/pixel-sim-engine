#include <SDL.h>
#include <SDL_metal.h>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

#include <cstdio>
#include <cstdint>

static void die_sdl(const char* msg) {
  std::fprintf(stderr, "%s: %s\n", msg, SDL_GetError());
}

int main(int, char**) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
    die_sdl("SDL_Init failed");
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow(
    "Pixel Sim Engine - Step 1 (SDL + bgfx)",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    1280, 720,
    SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
  );

  if (!window) {
    die_sdl("SDL_CreateWindow failed");
    SDL_Quit();
    return 1;
  }

#if defined(__APPLE__)
  // 1) Create Metal view
  void* metal_view = SDL_Metal_CreateView(window);
  if (!metal_view) {
    die_sdl("SDL_Metal_CreateView failed");
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  // 2) Get CAMetalLayer* from the view (THIS is what bgfx wants)
  void* metal_layer = SDL_Metal_GetLayer(metal_view);
  if (!metal_layer) {
    die_sdl("SDL_Metal_GetLayer returned null");
    SDL_Metal_DestroyView(metal_view);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  std::fprintf(stderr, "SDL Metal view:  %p\n", metal_view);
  std::fprintf(stderr, "SDL Metal layer: %p\n", metal_layer);

  // bgfx recommends calling renderFrame once before init on some platforms
  bgfx::renderFrame();

  bgfx::PlatformData pd{};
  pd.ndt = nullptr;
  pd.nwh = metal_layer; // <-- CAMetalLayer*
  pd.context = nullptr;
  pd.backBuffer = nullptr;
  pd.backBufferDS = nullptr;

  // Do both (belt + suspenders)
  bgfx::setPlatformData(pd);
#else
# error "macOS-only sample"
#endif

  int drawable_w = 0, drawable_h = 0;
  SDL_GL_GetDrawableSize(window, &drawable_w, &drawable_h);
  if (drawable_w <= 0 || drawable_h <= 0) {
    SDL_GetWindowSize(window, &drawable_w, &drawable_h);
  }

  bgfx::Init init;
  init.type = bgfx::RendererType::Metal;  // be explicit
  init.vendorId = BGFX_PCI_ID_NONE;
  init.resolution.width  = (uint32_t)drawable_w;
  init.resolution.height = (uint32_t)drawable_h;
  init.resolution.reset  = BGFX_RESET_VSYNC;

  // Also pass platform data here to guarantee it’s seen during init
  init.platformData = pd;

  if (!bgfx::init(init)) {
    std::fprintf(stderr, "bgfx::init failed\n");
#if defined(__APPLE__)
    SDL_Metal_DestroyView(metal_view);
#endif
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x102030ff, 1.0f, 0);
  bgfx::setViewRect(0, 0, 0, bgfx::BackbufferRatio::Equal);

  bool running = true;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) running = false;

      if (e.type == SDL_WINDOWEVENT &&
          (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
           e.window.event == SDL_WINDOWEVENT_RESIZED)) {
        SDL_GL_GetDrawableSize(window, &drawable_w, &drawable_h);
        if (drawable_w <= 0 || drawable_h <= 0) SDL_GetWindowSize(window, &drawable_w, &drawable_h);

        bgfx::reset((uint32_t)drawable_w, (uint32_t)drawable_h, BGFX_RESET_VSYNC);
        bgfx::setViewRect(0, 0, 0, bgfx::BackbufferRatio::Equal);
      }
    }

    bgfx::touch(0);
    bgfx::frame();
  }

  bgfx::shutdown();

#if defined(__APPLE__)
  SDL_Metal_DestroyView(metal_view);
#endif
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
