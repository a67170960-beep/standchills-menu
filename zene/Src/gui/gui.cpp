
// ================================================================================
// TG Project: @imguisource_bot
// Main: @ImGui_Source_FIXNEY - ImGui Source
// Additional: @il2cpp_src - shit src 🧨
// ================================================================================

#include "gui.hpp"
#include "touch.hpp"
#include "mem.hpp"
#include "imgui.h"

#include <cstdio>
#include <vector>

static int activeSection = 0;
static bool touchTest = false;

static bool esp = false;
static bool playerBox = false;
static bool playerNames = false;
static bool distance = false;
static int espColor = 0;

static bool freecam = false;
static bool noClip = false;
static bool speedHack = false;
static int speed = 50;

static bool aimbot = false;
static bool drawFov = false;
static int fov = 50;
static int smooth = 50;

static int playerSpeed = 50;
static int jumpHeight = 50;
static bool fly = false;
static bool godMode = false;

static int menuKey = 0;
static bool showFps = false;
static int theme = 0;

static float uiScale = 1.0f;

static float S(float value) {
    return value * uiScale;
}

static const char* sectionNames[] = {
    "Visual",
    "Misc",
    "Aimbot",
    "Player",
    "Settings"
};

static void UpdateTestEntities(const ImVec2& screenSize) {
    std::vector<Mem::Entity> entities = {
        {
            1,
            Mem::EntityType::NPC,
            {screenSize.x * 0.09f, screenSize.y * 0.38f, 12.0f},
            1,
            100.0f,
            true,
            true
        },
        {
            2,
            Mem::EntityType::NPC,
            {screenSize.x * 0.91f, screenSize.y * 0.38f, 24.0f},
            1,
            75.0f,
            true,
            false
        },
        {
            3,
            Mem::EntityType::NPC,
            {screenSize.x * 0.09f, screenSize.y * 0.70f, 37.0f},
            1,
            50.0f,
            true,
            true
        }
    };

    Mem::SetEntities(entities);
}

static ImU32 GetESPColor(bool visible) {
    ImVec4 color;

    switch (espColor) {
        case 1:
            color = ImVec4(0.95f, 0.25f, 0.25f, 1.0f);
            break;
        case 2:
            color = ImVec4(0.25f, 0.85f, 0.40f, 1.0f);
            break;
        case 3:
            color = ImVec4(0.30f, 0.55f, 1.0f, 1.0f);
            break;
        default:
            color = visible
                ? ImVec4(0.30f, 0.70f, 1.0f, 1.0f)
                : ImVec4(0.95f, 0.55f, 0.25f, 1.0f);
            break;
    }

    return ImGui::ColorConvertFloat4ToU32(color);
}

static void DrawTestESP(const ImVec2& screenSize) {
    UpdateTestEntities(screenSize);

    if (!esp)
        return;

    std::vector<Mem::Entity> entities = Mem::GetEntities();
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    for (const Mem::Entity& entity : entities) {
        if (!entity.alive || entity.type != Mem::EntityType::NPC)
            continue;

        float x = entity.position.x;
        float y = entity.position.y;

        float boxWidth = S(44.0f);
        float boxHeight = S(92.0f);

        ImVec2 topLeft(x - boxWidth * 0.5f, y - boxHeight * 0.5f);
        ImVec2 bottomRight(x + boxWidth * 0.5f, y + boxHeight * 0.5f);

        ImU32 color = GetESPColor(entity.visible);

        if (playerBox) {
            drawList->AddRect(
                topLeft,
                bottomRight,
                color,
                S(2.0f),
                0,
                S(1.5f)
            );
        }

        if (playerNames) {
            char name[32];
            std::snprintf(name, sizeof(name), "TEST NPC %02d", entity.id);

            ImVec2 textSize = ImGui::CalcTextSize(name);

            drawList->AddText(
                ImVec2(x - textSize.x * 0.5f, topLeft.y - S(19.0f)),
                color,
                name
            );
        }

        if (distance) {
            char distanceText[32];
            std::snprintf(
                distanceText,
                sizeof(distanceText),
                "%.0f m",
                entity.position.z
            );

            ImVec2 textSize = ImGui::CalcTextSize(distanceText);

            drawList->AddText(
                ImVec2(x - textSize.x * 0.5f, bottomRight.y + S(4.0f)),
                IM_COL32(220, 225, 235, 255),
                distanceText
            );
        }
    }
}

static void DrawToggle(const char* name, const char* description, bool* value) {
    ImGui::PushID(name);
    ImGui::BeginChild("##row", ImVec2(0.0f, S(58.0f)), true);

    ImGui::SetCursorPos(ImVec2(S(14.0f), S(10.0f)));
    ImGui::TextUnformatted(name);

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - S(48.0f));
    ImGui::Checkbox("##toggle", value);

    ImGui::SetCursorPosX(S(14.0f));
    ImGui::TextDisabled("%s", description);

    ImGui::EndChild();
    ImGui::PopID();
}

static void DrawSlider(const char* name, const char* description, int* value) {
    ImGui::PushID(name);
    ImGui::BeginChild("##row", ImVec2(0.0f, S(68.0f)), true);

    ImGui::SetCursorPos(ImVec2(S(14.0f), S(8.0f)));
    ImGui::TextUnformatted(name);

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - S(42.0f));
    ImGui::TextDisabled("%d", *value);

    ImGui::SetCursorPosX(S(14.0f));
    ImGui::TextDisabled("%s", description);

    ImGui::SetCursorPosX(S(14.0f));
    ImGui::SetNextItemWidth(-S(14.0f));
    ImGui::SliderInt("##slider", value, 0, 100, "");

    ImGui::EndChild();
    ImGui::PopID();
}

static void DrawCombo(const char* name, const char* description, int* value, const char* const items[], int count) {
    ImGui::PushID(name);
    ImGui::BeginChild("##row", ImVec2(0.0f, S(62.0f)), true);

    ImGui::SetCursorPos(ImVec2(S(14.0f), S(8.0f)));
    ImGui::TextUnformatted(name);

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - S(145.0f));
    ImGui::SetNextItemWidth(S(125.0f));
    ImGui::Combo("##combo", value, items, count);

    ImGui::SetCursorPosX(S(14.0f));
    ImGui::TextDisabled("%s", description);

    ImGui::EndChild();
    ImGui::PopID();
}

static void DrawTouchTest() {
    ImGui::BeginChild("##touchrow", ImVec2(0.0f, S(58.0f)), true);

    ImGui::SetCursorPos(ImVec2(S(14.0f), S(10.0f)));
    ImGui::TextUnformatted("Touch test");

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - S(112.0f));

    if (ImGui::Button(touchTest ? "TOUCH: OK" : "TEST TOUCH", ImVec2(S(96.0f), S(30.0f))))
        touchTest = !touchTest;

    ImGui::SetCursorPosX(S(14.0f));
    ImGui::TextDisabled("Overlay touch input test");

    ImGui::EndChild();
}

static void DrawSection() {
    if (activeSection == 0) {
        DrawToggle("ESP", "Enable test entity visualization", &esp);
        DrawToggle("Player Box", "Display boxes around test entities", &playerBox);
        DrawToggle("Player Names", "Show test entity names", &playerNames);
        DrawToggle("Distance", "Display test entity distances", &distance);

        static const char* colors[] = {
            "Default",
            "Red",
            "Green",
            "Blue"
        };

        DrawCombo("ESP Color", "Choose visual highlight color", &espColor, colors, IM_ARRAYSIZE(colors));
    }

    if (activeSection == 1) {
        DrawToggle("Freecam", "Enable free camera mode", &freecam);
        DrawToggle("No Clip", "Move through objects", &noClip);
        DrawToggle("Speed Hack", "Modify movement speed", &speedHack);
        DrawSlider("Speed", "Movement speed multiplier", &speed);
    }

    if (activeSection == 2) {
        DrawToggle("Enable Aimbot", "Toggle aim assistance", &aimbot);
        DrawToggle("Draw FOV", "Display aim radius", &drawFov);
        DrawSlider("FOV", "Aim radius size", &fov);
        DrawSlider("Smooth", "Aim movement smoothing", &smooth);
    }

    if (activeSection == 3) {
        DrawSlider("Speed", "Player movement speed", &playerSpeed);
        DrawSlider("Jump Height", "Player jump height", &jumpHeight);
        DrawToggle("Fly", "Enable flight mode", &fly);
        DrawToggle("God Mode", "Enable damage protection", &godMode);
    }

    if (activeSection == 4) {
        static const char* keys[] = {
            "Default",
            "Volume Up",
            "Volume Down"
        };

        static const char* themes[] = {
            "Dark",
            "Light",
            "Blue"
        };

        DrawCombo("Menu Key", "Choose menu toggle key", &menuKey, keys, IM_ARRAYSIZE(keys));
        DrawToggle("Show FPS", "Display frames per second", &showFps);
        DrawCombo("Theme", "Interface color theme", &theme, themes, IM_ARRAYSIZE(themes));
        DrawTouchTest();
    }
}

void DrawMenu() {
    ImGuiIO& io = ImGui::GetIO();

    ImVec2 screenSize = io.DisplaySize;

    DrawTestESP(screenSize);

    ImVec2 menuSize(
        screenSize.x * 0.60f,
        screenSize.y * 0.60f
    );

    float scaleX = menuSize.x / 650.0f;
    float scaleY = menuSize.y / 410.0f;

    uiScale = scaleX < scaleY ? scaleX : scaleY;

    float sidebarWidth = menuSize.x * 0.27f;

    ImGui::PushFont(nullptr, S(16.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, S(10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, S(7.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, S(5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(8.0f), S(7.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(S(4.0f), S(3.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, S(10.0f));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.094f, 0.102f, 0.129f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.082f, 0.090f, 0.114f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.160f, 0.173f, 0.212f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.890f, 0.902f, 0.929f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.470f, 0.490f, 0.545f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.125f, 0.137f, 0.176f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.165f, 0.184f, 0.239f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.180f, 0.220f, 0.310f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.160f, 0.173f, 0.212f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.200f, 0.220f, 0.275f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.230f, 0.270f, 0.360f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.340f, 0.550f, 0.920f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.340f, 0.550f, 0.920f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.430f, 0.640f, 1.000f, 1.0f));

    ImGui::SetNextWindowSize(menuSize, ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2(
            (screenSize.x - menuSize.x) * 0.5f,
            (screenSize.y - menuSize.y) * 0.5f
        ),
        ImGuiCond_FirstUseEver
    );

    ImGui::Begin(
        "##zene",
        nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings
    );

    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();

    UpdateMenuBounds(pos.x, pos.y, size.x, size.y);

    ImGui::BeginChild("##sidebar", ImVec2(sidebarWidth, 0.0f), false);

    ImGui::SetCursorPos(ImVec2(S(16.0f), S(20.0f)));
    ImGui::SetWindowFontScale(1.35f);
    ImGui::TextUnformatted("zene");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::SetCursorPos(ImVec2(S(12.0f), S(70.0f)));

    for (int i = 0; i < IM_ARRAYSIZE(sectionNames); i++) {
        bool selected = activeSection == i;
        ImGui::SetCursorPosX(S(12.0f));

        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.145f, 0.173f, 0.235f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.420f, 0.610f, 1.000f, 1.0f));
        }

        if (ImGui::Button(sectionNames[i], ImVec2(sidebarWidth - S(24.0f), S(36.0f))))
            activeSection = i;

        if (selected)
            ImGui::PopStyleColor(2);

        ImGui::Dummy(ImVec2(0.0f, S(2.0f)));
    }

    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);

    ImGui::BeginChild("##content", ImVec2(0.0f, 0.0f), false);

    ImGui::SetCursorPos(ImVec2(S(27.0f), S(23.0f)));
    ImGui::Text("%s", sectionNames[activeSection]);

    ImGui::SetCursorPos(ImVec2(S(27.0f), S(55.0f)));
    ImGui::Separator();

    ImGui::SetCursorPos(ImVec2(S(27.0f), S(72.0f)));

    ImGui::BeginChild("##section_settings", ImVec2(-S(27.0f), -S(20.0f)), false);
    DrawSection();
    ImGui::EndChild();

    ImGui::EndChild();

    ImGui::End();

    ImGui::PopStyleColor(14);
    ImGui::PopStyleVar(7);
    ImGui::PopFont();
}
