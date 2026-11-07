
// ================================================================================
// Бот с сурсами: @imguisource_bot
// Основной канал (ImGui Source): @ImGui_Source_FIXNEY
// Дополнительный канал (shit src 🧨): @il2cpp_src
// ================================================================================

#include "touch.hpp"

#include <android/log.h>
#include <jni.h>
#include <link.h>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <mutex>
#include <deque>

#include "And64InlineHook.hpp"
#include "imgui.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "freecamk", __VA_ARGS__)

static constexpr uintptr_t NATIVE_INJECT_EVENT_OFFSET = 0x6ecb84;

struct TouchEvent {
    float x;
    float y;
    int action;
};

struct MenuBounds {
    float x;
    float y;
    float width;
    float height;
};

using NativeInjectEventFn = jboolean (*)(JNIEnv*, jobject, jobject, jint);

static NativeInjectEventFn originalNativeInjectEvent = nullptr;

static std::mutex touchMutex;
static std::deque<TouchEvent> touchEvents;
static MenuBounds menuBounds{30.0f, 30.0f, 220.0f, 90.0f};

static std::atomic<bool> captureGesture{false};

static uintptr_t unityBase = 0;

static int FindUnityLibrary(struct dl_phdr_info* info, size_t, void* data) {
    if (info->dlpi_name && std::strstr(info->dlpi_name, "libunity.so")) {
        *static_cast<uintptr_t*>(data) = static_cast<uintptr_t>(info->dlpi_addr);
        return 1;
    }

    return 0;
}

void UpdateMenuBounds(float x, float y, float width, float height) {
    std::lock_guard<std::mutex> lock(touchMutex);
    menuBounds = {x, y, width, height};
}

static bool IsInsideMenu(float x, float y) {
    std::lock_guard<std::mutex> lock(touchMutex);

    return x >= menuBounds.x &&
           x <= menuBounds.x + menuBounds.width &&
           y >= menuBounds.y &&
           y <= menuBounds.y + menuBounds.height;
}

static void QueueTouch(float x, float y, int action) {
    std::lock_guard<std::mutex> lock(touchMutex);

    if (touchEvents.size() >= 128)
        touchEvents.pop_front();

    touchEvents.push_back({x, y, action});
}

static jboolean HookedNativeInjectEvent(
    JNIEnv* env,
    jobject thiz,
    jobject event,
    jint mode
) {
    if (!env || !event || !originalNativeInjectEvent)
        return JNI_FALSE;

    jclass eventClass = env->GetObjectClass(event);

    if (!eventClass)
        return originalNativeInjectEvent(env, thiz, event, mode);

    jmethodID getAction = env->GetMethodID(eventClass, "getActionMasked", "()I");
    jmethodID getPointerCount = env->GetMethodID(eventClass, "getPointerCount", "()I");
    jmethodID getX = env->GetMethodID(eventClass, "getX", "(I)F");
    jmethodID getY = env->GetMethodID(eventClass, "getY", "(I)F");

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(eventClass);
        return originalNativeInjectEvent(env, thiz, event, mode);
    }

    if (!getAction || !getPointerCount || !getX || !getY) {
        env->DeleteLocalRef(eventClass);
        return originalNativeInjectEvent(env, thiz, event, mode);
    }

    jint action = env->CallIntMethod(event, getAction);
    jint pointerCount = env->CallIntMethod(event, getPointerCount);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(eventClass);
        return originalNativeInjectEvent(env, thiz, event, mode);
    }

    if (pointerCount < 1) {
        env->DeleteLocalRef(eventClass);
        return originalNativeInjectEvent(env, thiz, event, mode);
    }

    jfloat x = env->CallFloatMethod(event, getX, 0);
    jfloat y = env->CallFloatMethod(event, getY, 0);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(eventClass);
        return originalNativeInjectEvent(env, thiz, event, mode);
    }

    env->DeleteLocalRef(eventClass);

    int maskedAction = action & 0xff;

    if (maskedAction == 0) {
        bool inside = IsInsideMenu(x, y);
        captureGesture.store(inside);

        if (inside) {
            QueueTouch(x, y, 1);
            return JNI_TRUE;
        }

        return originalNativeInjectEvent(env, thiz, event, mode);
    }

    if (maskedAction == 2) {
        if (captureGesture.load()) {
            QueueTouch(x, y, 0);
            return JNI_TRUE;
        }

        return originalNativeInjectEvent(env, thiz, event, mode);
    }

    if (maskedAction == 1 || maskedAction == 3) {
        if (captureGesture.exchange(false)) {
            QueueTouch(x, y, 2);
            return JNI_TRUE;
        }

        return originalNativeInjectEvent(env, thiz, event, mode);
    }

    if (maskedAction == 5 || maskedAction == 6) {
        if (captureGesture.load())
            return JNI_TRUE;

        return originalNativeInjectEvent(env, thiz, event, mode);
    }

    if (captureGesture.load())
        return JNI_TRUE;

    return originalNativeInjectEvent(env, thiz, event, mode);
}

void InstallTouchHook() {
    dl_iterate_phdr(FindUnityLibrary, &unityBase);

    if (unityBase == 0) {
        LOGI("libunity.so not found");
        return;
    }

    void* target = reinterpret_cast<void*>(unityBase + NATIVE_INJECT_EVENT_OFFSET);

    A64HookFunction(
        target,
        reinterpret_cast<void*>(HookedNativeInjectEvent),
        reinterpret_cast<void**>(&originalNativeInjectEvent)
    );

    if (!originalNativeInjectEvent) {
        LOGI("nativeInjectEvent hook failed");
        return;
    }

    LOGI("nativeInjectEvent hooked at %p", target);
}

void UpdateTouchInput() {
    std::deque<TouchEvent> events;

    {
        std::lock_guard<std::mutex> lock(touchMutex);
        events.swap(touchEvents);
    }

    ImGuiIO& io = ImGui::GetIO();

    for (const TouchEvent& event : events) {
        io.AddMousePosEvent(event.x, event.y);

        if (event.action == 1)
            io.AddMouseButtonEvent(0, true);
        else if (event.action == 2)
            io.AddMouseButtonEvent(0, false);
    }
}
