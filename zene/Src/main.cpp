
// ================================================================================
// Protected source: @imguisource_bot
// Official channels: t.me/ImGui_Source_FIXNEY & t.me/il2cpp_src
// ================================================================================

#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <dlfcn.h>

#include "And64InlineHook.hpp"
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "gui/gui.hpp"
#include "gui/touch.hpp"
#include "inter_font.hpp"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "freecamk", __VA_ARGS__)

using EGLSwapBuffersFn = EGLBoolean (*)(EGLDisplay, EGLSurface);

static EGLSwapBuffersFn originalEglSwapBuffers = nullptr;
static EGLContext initializedContext = EGL_NO_CONTEXT;

static bool InitializeImGui() {
    EGLContext context = eglGetCurrentContext();

    if (context == EGL_NO_CONTEXT)
        return false;

    if (initializedContext == context)
        return true;

    if (initializedContext != EGL_NO_CONTEXT) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui::DestroyContext();
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsLight();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    fontConfig.OversampleH = 2;
    fontConfig.OversampleV = 2;
    fontConfig.PixelSnapH = true;

    ImFont* interFont = io.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char*>(inter_font_data),
        sizeof(inter_font_data),
        18.0f,
        &fontConfig,
        io.Fonts->GetGlyphRangesCyrillic()
    );

    if (interFont != nullptr) {
        io.FontDefault = interFont;
        LOGI("Inter font loaded");
    } else {
        LOGI("Inter font loading failed");
    }

    if (!ImGui_ImplOpenGL3_Init("#version 300 es")) {
        ImGui::DestroyContext();
        return false;
    }

    initializedContext = context;
    LOGI("ImGui initialized");

    return true;
}

static EGLBoolean HookedEglSwapBuffers(EGLDisplay display, EGLSurface surface) {
    if (originalEglSwapBuffers == nullptr)
        return EGL_FALSE;

    if (InitializeImGui()) {
        EGLint width = 0;
        EGLint height = 0;

        eglQuerySurface(display, surface, EGL_WIDTH, &width);
        eglQuerySurface(display, surface, EGL_HEIGHT, &height);

        if (width > 0 && height > 0) {
            ImGuiIO& io = ImGui::GetIO();

            io.DisplaySize = ImVec2(
                static_cast<float>(width),
                static_cast<float>(height)
            );

            io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
            io.DeltaTime = 1.0f / 60.0f;

            ImGui_ImplOpenGL3_NewFrame();
            ImGui::NewFrame();

            UpdateTouchInput();
            DrawMenu();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
    }

    return originalEglSwapBuffers(display, surface);
}

static void InstallHook() {
    void* library = dlopen("libEGL.so", RTLD_NOW);

    if (library == nullptr) {
        LOGI("libEGL.so not found");
        return;
    }

    void* target = dlsym(library, "eglSwapBuffers");

    if (target == nullptr) {
        LOGI("eglSwapBuffers not found");
        return;
    }

    A64HookFunction(
        target,
        reinterpret_cast<void*>(HookedEglSwapBuffers),
        reinterpret_cast<void**>(&originalEglSwapBuffers)
    );

    LOGI("eglSwapBuffers hooked");
}

extern "C" int freecamk_init() {
    LOGI("freecamk_init");
    InstallTouchHook();
    InstallHook();
    return 0;
}

__attribute__((constructor))
static void on_load() {
    LOGI("constructor: freecamk.so loaded");
    freecamk_init();
}
