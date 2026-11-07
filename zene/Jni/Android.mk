LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := freecamk

LOCAL_SRC_FILES := \
    ../Src/main.cpp \
    ../Src/core/mem.cpp \
    ../Src/gui/gui.cpp \
    ../Src/gui/touch.cpp \
    ../Hook/And64InlineHook/And64InlineHook.cpp \
    ../Include/imgui/imgui.cpp \
    ../Include/imgui/imgui_draw.cpp \
    ../Include/imgui/imgui_tables.cpp \
    ../Include/imgui/imgui_widgets.cpp \
    ../Include/imgui/backends/imgui_impl_opengl3.cpp

LOCAL_CPPFLAGS := -std=c++17 -fvisibility=hidden -DIMGUI_IMPL_OPENGL_ES3
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../Src \
    $(LOCAL_PATH)/../Src/core \
    $(LOCAL_PATH)/../Src/gui \
    $(LOCAL_PATH)/../Include/imgui \
    $(LOCAL_PATH)/../Include/imgui/backends \
    $(LOCAL_PATH)/../Hook/And64InlineHook \
    $(LOCAL_PATH)/../build/generated

LOCAL_LDLIBS := -landroid -lEGL -lGLESv3 -llog -ldl
LOCAL_LDFLAGS := -Wl,--build-id

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := inj
LOCAL_SRC_FILES := ../Inj/inj.cpp
LOCAL_CPPFLAGS := -std=c++17
include $(BUILD_EXECUTABLE)
