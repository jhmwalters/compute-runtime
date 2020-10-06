/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/os_interface.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/mocks/linux/mock_drm_memory_manager.h"

#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"
#include "test.h"

namespace NEO {

class DrmMemManagerFixture {
  public:
    struct FrontWindowMemManagerMock : public TestedDrmMemoryManager {
        using MemoryManager::allocate32BitGraphicsMemoryImpl;
        FrontWindowMemManagerMock(NEO::ExecutionEnvironment &executionEnvironment) : TestedDrmMemoryManager(executionEnvironment) {}
        void forceLimitedRangeAllocator(uint32_t rootDeviceIndex, uint64_t range) { getGfxPartition(rootDeviceIndex)->init(range, 0, 0, gfxPartitions.size(), true); }
    };

    void SetUp() {
        DebugManagerStateRestore dbgRestorer;
        DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(true);
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
        DeviceFactory::prepareDeviceEnvironments(*executionEnvironment);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->get()->setDrm(new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]));
        memManager = std::unique_ptr<FrontWindowMemManagerMock>(new FrontWindowMemManagerMock(*executionEnvironment));
    }
    void TearDown() {
    }
    std::unique_ptr<FrontWindowMemManagerMock> memManager;
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
};

using DrmFrontWindowPoolAllocatorTests = Test<DrmMemManagerFixture>;

TEST_F(DrmFrontWindowPoolAllocatorTests, givenAllocateInSpecialPoolFlagWhenDrmAllocate32BitGraphicsMemoryThenAllocateAtHeapBegining) {
    AllocationData allocData = {};
    allocData.flags.use32BitExtraPool = true;
    allocData.size = MemoryConstants::kiloByte;
    auto allocation = memManager->allocate32BitGraphicsMemoryImpl(allocData, false);
    EXPECT_EQ(allocation->getGpuBaseAddress(), allocation->getGpuAddress());
    memManager->freeGraphicsMemory(allocation);
}
} // namespace NEO