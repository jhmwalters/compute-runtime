/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/cache_info_impl.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_context.h"

namespace NEO {

void DrmAllocation::bindBOs(OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind) {
    auto bo = this->getBO();
    bindBO(bo, osContext, vmHandleId, bufferObjects, bind);
}

bool DrmAllocation::setCacheRegion(Drm *drm, CacheRegion regionIndex) {
    auto cacheInfo = static_cast<CacheInfoImpl *>(drm->getCacheInfo());
    if (cacheInfo == nullptr) {
        return false;
    }

    return setCacheAdvice(drm, 0, regionIndex);
}

} // namespace NEO
