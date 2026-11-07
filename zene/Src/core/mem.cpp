
// ================================================================================
// Powered by @imguisource_bot | Channels: @ImGui_Source_FIXNEY, @il2cpp_src
// ================================================================================

#include "mem.hpp"

#include <mutex>

namespace Mem {

static std::vector<Entity> entities;
static std::mutex entitiesMutex;

void Initialize() {
    std::lock_guard<std::mutex> lock(entitiesMutex);
    entities.clear();
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(entitiesMutex);
    entities.clear();
}

void SetEntities(const std::vector<Entity>& newEntities) {
    std::lock_guard<std::mutex> lock(entitiesMutex);
    entities = newEntities;
}

std::vector<Entity> GetEntities() {
    std::lock_guard<std::mutex> lock(entitiesMutex);
    return entities;
}

}
