
// ================================================================================
// Bot: @imguisource_bot | TG: @ImGui_Source_FIXNEY & @il2cpp_src
// ================================================================================

#pragma once

#include <cstdint>
#include <vector>

namespace Mem {

struct Vec3 {
    float x;
    float y;
    float z;
};

enum class EntityType : uint8_t {
    Player,
    NPC
};

struct Entity {
    int id;
    EntityType type;
    Vec3 position;
    int team;
    float health;
    bool alive;
    bool visible;
};

void Initialize();
void Shutdown();

void SetEntities(const std::vector<Entity>& entities);
std::vector<Entity> GetEntities();

}
