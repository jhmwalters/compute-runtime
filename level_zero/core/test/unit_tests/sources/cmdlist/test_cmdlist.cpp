/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"

#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "test.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

namespace L0 {
namespace ult {

using CommandListCreate = Test<DeviceFixture>;

TEST(zeCommandListCreateImmediate, redirectsToObject) {
    Mock<Device> device;
    ze_command_queue_desc_t desc = {};
    ze_command_list_handle_t commandList = {};

    EXPECT_CALL(device, createCommandListImmediate(&desc, &commandList))
        .Times(1)
        .WillRepeatedly(::testing::Return(ZE_RESULT_SUCCESS));

    auto result = zeCommandListCreateImmediate(device.toHandle(), &desc, &commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(CommandListCreate, whenCommandListIsCreatedThenItIsInitialized) {
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    ASSERT_GT(commandList->commandContainer.getCmdBufferAllocations().size(), 0u);

    auto numAllocations = 0u;
    auto allocation = whitebox_cast(commandList->commandContainer.getCmdBufferAllocations()[0]);
    ASSERT_NE(allocation, nullptr);

    ++numAllocations;

    ASSERT_NE(nullptr, commandList->commandContainer.getCommandStream());
    for (uint32_t i = 0; i < NEO::HeapType::NUM_TYPES; i++) {
        ASSERT_NE(commandList->commandContainer.getIndirectHeap(static_cast<NEO::HeapType>(i)), nullptr);
        ++numAllocations;
        ASSERT_NE(commandList->commandContainer.getIndirectHeapAllocation(static_cast<NEO::HeapType>(i)), nullptr);
    }

    EXPECT_LT(0u, commandList->commandContainer.getCommandStream()->getAvailableSpace());
    ASSERT_EQ(commandList->commandContainer.getResidencyContainer().size(), numAllocations);
    EXPECT_EQ(commandList->commandContainer.getResidencyContainer().front(), allocation);
}

TEST_F(CommandListCreate, givenRegularCommandListThenDefaultNumIddPerBlockIsUsed) {
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
    ASSERT_NE(nullptr, commandList);

    const uint32_t defaultNumIdds = CommandList::defaultNumIddsPerBlock;
    EXPECT_EQ(defaultNumIdds, commandList->commandContainer.getNumIddPerBlock());
}

TEST_F(CommandListCreate, givenNonExistingPtrThenAppendMemAdviseReturnsError) {
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
    ASSERT_NE(nullptr, commandList);

    auto res = commandList->appendMemAdvise(device, nullptr, 0, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, res);
}

TEST_F(CommandListCreate, givenNonExistingPtrThenAppendMemoryPrefetchReturnsError) {
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
    ASSERT_NE(nullptr, commandList);

    auto res = commandList->appendMemoryPrefetch(nullptr, 0);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, res);
}

TEST_F(CommandListCreate, givenValidPtrThenAppendMemAdviseReturnsSuccess) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    auto res = driverHandle->allocDeviceMem(device->toHandle(),
                                            ZE_DEVICE_MEM_ALLOC_FLAG_DEFAULT,
                                            size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = driverHandle->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenValidPtrThenAppendMemoryPrefetchReturnsSuccess) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    auto res = driverHandle->allocDeviceMem(device->toHandle(),
                                            ZE_DEVICE_MEM_ALLOC_FLAG_DEFAULT,
                                            size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = driverHandle->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenImmediateCommandListThenCustomNumIddPerBlockUsed) {
    const ze_command_queue_desc_t desc = {
        ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT,
        ZE_COMMAND_QUEUE_FLAG_NONE,
        ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
        0};
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, false));
    ASSERT_NE(nullptr, commandList);

    const uint32_t cmdListImmediateIdds = CommandList::commandListimmediateIddsPerBlock;
    EXPECT_EQ(cmdListImmediateIdds, commandList->commandContainer.getNumIddPerBlock());
}

TEST_F(CommandListCreate, whenCreatingImmediateCommandListThenItHasImmediateCommandQueueCreated) {
    const ze_command_queue_desc_t desc = {
        ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT,
        ZE_COMMAND_QUEUE_FLAG_NONE,
        ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
        0};
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, false));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);
}

TEST_F(CommandListCreate, givenInvalidProductFamilyThenReturnsNullPointer) {
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(IGFX_UNKNOWN, device, false));
    EXPECT_EQ(nullptr, commandList);
}

HWTEST_F(CommandListCreate, whenCommandListIsCreatedThenStateBaseAddressCmdIsAddedAndCorrectlyProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
    auto &commandContainer = commandList->commandContainer;
    auto gmmHelper = commandContainer.getDevice()->getGmmHelper();

    ASSERT_NE(nullptr, commandContainer.getCommandStream());
    auto usedSpaceBefore = commandContainer.getCommandStream()->getUsed();

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));
    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);

    auto dsh = commandContainer.getIndirectHeap(NEO::HeapType::DYNAMIC_STATE);
    auto ioh = commandContainer.getIndirectHeap(NEO::HeapType::INDIRECT_OBJECT);
    auto ssh = commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE);

    EXPECT_TRUE(cmdSba->getDynamicStateBaseAddressModifyEnable());
    EXPECT_TRUE(cmdSba->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(dsh->getHeapGpuBase(), cmdSba->getDynamicStateBaseAddress());
    EXPECT_EQ(dsh->getHeapSizeInPages(), cmdSba->getDynamicStateBufferSize());

    EXPECT_TRUE(cmdSba->getIndirectObjectBaseAddressModifyEnable());
    EXPECT_TRUE(cmdSba->getIndirectObjectBufferSizeModifyEnable());
    EXPECT_EQ(ioh->getHeapGpuBase(), cmdSba->getIndirectObjectBaseAddress());
    EXPECT_EQ(ioh->getHeapSizeInPages(), cmdSba->getIndirectObjectBufferSize());

    EXPECT_TRUE(cmdSba->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssh->getHeapGpuBase(), cmdSba->getSurfaceStateBaseAddress());

    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER), cmdSba->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenCreatedThenStateBaseAddressCmdIsNotProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, true));
    auto &commandContainer = commandList->commandContainer;

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());

    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenSetBarrierThenMiFlushDWIsProgrammed) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, true));
    auto &commandContainer = commandList->commandContainer;
    commandList->appendBarrier(nullptr, 0, nullptr);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWhenSetBarrierThenPipeControlIsProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
    auto &commandContainer = commandList->commandContainer;
    commandList->appendBarrier(nullptr, 0, nullptr);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}
template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandList : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    MockCommandList() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>() {}

    AlignedAllocationData getAlignedAllocation(L0::Device *device, const void *buffer, uint64_t bufferSize) override {
        return {0, 0, nullptr, true};
    }
    ze_result_t appendMemoryCopyKernelWithGA(void *dstPtr,
                                             NEO::GraphicsAllocation *dstPtrAlloc,
                                             uint64_t dstOffset,
                                             void *srcPtr,
                                             NEO::GraphicsAllocation *srcPtrAlloc,
                                             uint64_t srcOffset,
                                             uint32_t size,
                                             uint32_t elementSize,
                                             Builtin builtin) override {
        appendMemoryCopyKernelWithGACalledTimes++;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t appendMemoryCopyBlit(uintptr_t dstPtr,
                                     NEO::GraphicsAllocation *dstPtrAlloc,
                                     uint64_t dstOffset, uintptr_t srcPtr,
                                     NEO::GraphicsAllocation *srcPtrAlloc,
                                     uint64_t srcOffset,
                                     uint32_t size,
                                     ze_event_handle_t hSignalEvent) override {
        appendMemoryCopyBlitCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t appendMemoryCopyBlitRegion(NEO::GraphicsAllocation *srcptr,
                                           NEO::GraphicsAllocation *dstptr,
                                           ze_copy_region_t srcRegion,
                                           ze_copy_region_t dstRegion, Vec3<size_t> copySize,
                                           size_t srcRowPitch, size_t srcSlicePitch,
                                           size_t dstRowPitch, size_t dstSlicePitch,
                                           Vec3<uint32_t> srcSize, Vec3<uint32_t> dstSize, ze_event_handle_t hSignalEvent) override {
        appendMemoryCopyBlitRegionCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t appendMemoryCopyKernel2d(NEO::GraphicsAllocation *dstptr, NEO::GraphicsAllocation *srcptr,
                                         Builtin builtin, const ze_copy_region_t *dstRegion,
                                         uint32_t dstPitch, size_t dstOffset,
                                         const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                         size_t srcOffset, ze_event_handle_t hSignalEvent,
                                         uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override {
        appendMemoryCopyKernel2dCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t appendMemoryCopyKernel3d(NEO::GraphicsAllocation *dstptr, NEO::GraphicsAllocation *srcptr,
                                         Builtin builtin, const ze_copy_region_t *dstRegion,
                                         uint32_t dstPitch, uint32_t dstSlicePitch, size_t dstOffset,
                                         const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                         uint32_t srcSlicePitch, size_t srcOffset,
                                         ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                                         ze_event_handle_t *phWaitEvents) override {
        appendMemoryCopyKernel3dCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t appendBlitFill(void *ptr, const void *pattern,
                               size_t patternSize, size_t size,
                               ze_event_handle_t hEvent) override {
        appendBlitFillCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t appendCopyImageBlit(NEO::GraphicsAllocation *src,
                                    NEO::GraphicsAllocation *dst,
                                    Vec3<size_t> srcOffsets, Vec3<size_t> dstOffsets,
                                    size_t srcRowPitch, size_t srcSlicePitch,
                                    size_t dstRowPitch, size_t dstSlicePitch,
                                    size_t bytesPerPixel, Vec3<size_t> copySize,
                                    Vec3<uint32_t> srcSize, Vec3<uint32_t> dstSize, ze_event_handle_t hSignalEvent) override {
        appendCopyImageBlitCalledTimes++;
        appendImageRegionCopySize = copySize;
        appendImageRegionSrcOrigin = srcOffsets;
        appendImageRegionDstOrigin = dstOffsets;
        return ZE_RESULT_SUCCESS;
    }
    uint32_t appendMemoryCopyKernelWithGACalledTimes = 0;
    uint32_t appendMemoryCopyBlitCalledTimes = 0;
    uint32_t appendMemoryCopyBlitRegionCalledTimes = 0;
    uint32_t appendMemoryCopyKernel2dCalledTimes = 0;
    uint32_t appendMemoryCopyKernel3dCalledTimes = 0;
    uint32_t appendBlitFillCalledTimes = 0;
    uint32_t appendCopyImageBlitCalledTimes = 0;
    Vec3<size_t> appendImageRegionCopySize = {0, 0, 0};
    Vec3<size_t> appendImageRegionSrcOrigin = {9, 9, 9};
    Vec3<size_t> appendImageRegionDstOrigin = {9, 9, 9};
};

using Platforms = IsAtLeastProduct<IGFX_SKYLAKE>;

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryCopyCalledThenAppendMemoryCopyWithappendMemoryCopyKernelWithGACalled, Platforms) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, false);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    cmdList.appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.appendMemoryCopyKernelWithGACalledTimes, 0u);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryCopyCalledThenAppendMemoryCopyWithappendMemoryCopyWithBliterCalled, Platforms) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    cmdList.appendMemoryCopy(dstPtr, srcPtr, 0x1001, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalledTimes, 0u);
    EXPECT_GT(cmdList.appendMemoryCopyBlitCalledTimes, 0u);
}

class MockDriverHandle : public L0::DriverHandleImp {
  public:
    bool findAllocationDataForRange(const void *buffer,
                                    size_t size,
                                    NEO::SvmAllocationData **allocData) override {
        mockAllocation.reset(new NEO::MockGraphicsAllocation(rootDeviceIndex, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                             MemoryPool::System4KBPages));
        data.gpuAllocations.addAllocation(mockAllocation.get());
        if (allocData) {
            *allocData = &data;
        }
        return true;
    }
    const uint32_t rootDeviceIndex = 0u;
    std::unique_ptr<NEO::GraphicsAllocation> mockAllocation;
    NEO::SvmAllocationData data{rootDeviceIndex};
};

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryCopyRegionCalledThenAppendMemoryCopyWithappendMemoryCopyWithBliterCalled, Platforms) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {};
    ze_copy_region_t srcRegion = {};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr);
    EXPECT_GT(cmdList.appendMemoryCopyBlitRegionCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCommandListAnd3DWhbufferenMemoryCopyRegionCalledThenCopyKernel3DCalled, Platforms) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, false);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitRegionCalledTimes, 0u);
    EXPECT_GT(cmdList.appendMemoryCopyKernel3dCalledTimes, 0u);
}

using AppendMemoryCopy = CommandListCreate;

template <GFXCORE_FAMILY gfxCoreFamily>
class MockAppendMemoryCopy : public MockCommandList<gfxCoreFamily> {
  public:
    AlignedAllocationData getAlignedAllocation(L0::Device *device, const void *buffer, uint64_t bufferSize) override {
        return L0::CommandListCoreFamily<gfxCoreFamily>::getAlignedAllocation(device, buffer, bufferSize);
    }
};

HWTEST2_F(AppendMemoryCopy, givenCommandListAndHostPointersWhenMemoryCopyRegionCalledThenTwoNewAllocationAreAddedToHostMapPtr, Platforms) {
    MockAppendMemoryCopy<gfxCoreFamily> cmdList;
    cmdList.initialize(device, false);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr);
    EXPECT_EQ(cmdList.hostPtrMap.size(), 2u);
}

HWTEST2_F(AppendMemoryCopy, givenCommandListAndHostPointersWhenMemoryCopyRegionCalledThenPipeControlWithDcFlushAdded, Platforms) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockAppendMemoryCopy<gfxCoreFamily> cmdList;
    cmdList.initialize(device, false);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr);

    auto &commandContainer = cmdList.commandContainer;
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        genCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);
    PIPE_CONTROL *cmd = nullptr;
    while (itor != genCmdList.end()) {
        cmd = genCmdCast<PIPE_CONTROL *>(*itor);
        itor = find<PIPE_CONTROL *>(++itor, genCmdList.end());
    }
    EXPECT_TRUE(cmd->getDcFlushEnable());
}

HWTEST2_F(AppendMemoryCopy, givenCommandListAndHostPointersWhenMemoryCopyCalledThenPipeControlWithDcFlushAdded, Platforms) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockAppendMemoryCopy<gfxCoreFamily> cmdList;
    cmdList.initialize(device, false);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    cmdList.appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr);

    auto &commandContainer = cmdList.commandContainer;
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        genCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);
    PIPE_CONTROL *cmd = nullptr;
    while (itor != genCmdList.end()) {
        cmd = genCmdCast<PIPE_CONTROL *>(*itor);
        itor = find<PIPE_CONTROL *>(++itor, genCmdList.end());
    }
    EXPECT_TRUE(cmd->getDcFlushEnable());
}

HWTEST2_F(CommandListCreate, givenCommandListAnd2DWhbufferenMemoryCopyRegionCalledThenCopyKernel2DCalled, Platforms) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, false);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 1};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 1};
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr);
    EXPECT_EQ(cmdList.appendMemoryCopyBlitRegionCalledTimes, 0u);
    EXPECT_GT(cmdList.appendMemoryCopyKernel2dCalledTimes, 0u);
}

HWTEST2_F(AppendMemoryCopy, givenCopyOnlyCommandListAndHostPointersWhenMemoryCopyCalledThenPipeControlWithDcFlushAddedIsNotAddedAfterBlitCopy, Platforms) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr);

    auto &commandContainer = commandList->commandContainer;
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        genCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);

    itor = find<PIPE_CONTROL *>(++itor, genCmdList.end());

    EXPECT_EQ(genCmdList.end(), itor);
}

HWTEST2_F(AppendMemoryCopy, givenCopyOnlyCommandListAndHostPointersWhenMemoryCopyRegionCalledThenPipeControlWithDcFlushAddedIsNotAddedAfterBlitCopy, Platforms) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 1};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 1};
    commandList->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr);

    auto &commandContainer = commandList->commandContainer;
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        genCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);

    itor = find<PIPE_CONTROL *>(++itor, genCmdList.end());

    EXPECT_EQ(genCmdList.end(), itor);
}

HWTEST2_F(CommandListCreate, givenCopyOnlyCommandListWhenAppendMemoryFillCalledThenAppendBlitFillCalled, Platforms) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *dstPtr = reinterpret_cast<void *>(0x1234);
    int pattern = 1;
    cmdList.appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0, nullptr);
    EXPECT_GT(cmdList.appendBlitFillCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenAppendMemoryFillCalledThenAppendBlitFillNotCalled, Platforms) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, false);
    void *dstPtr = reinterpret_cast<void *>(0x1234);
    int pattern = 1;
    cmdList.appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0, nullptr);
    EXPECT_EQ(cmdList.appendBlitFillCalledTimes, 0u);
}

class MockEvent : public Mock<Event> {
  public:
    MockEvent() {
        mockAllocation.reset(new NEO::MockGraphicsAllocation(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                             MemoryPool::System4KBPages));
        gpuAddress = mockAllocation->getGpuAddress();
    }
    NEO::GraphicsAllocation &getAllocation() override {
        return *mockAllocation.get();
    }
    std::unique_ptr<NEO::GraphicsAllocation> mockAllocation;
};

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenAppendSignalEventThenMiFlushDWIsProgrammed) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, true));
    auto &commandContainer = commandList->commandContainer;
    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_NONE;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_NONE;
    commandList->appendSignalEvent(event.toHandle());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListyWhenAppendSignalEventThePipeControlIsProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
    auto &commandContainer = commandList->commandContainer;
    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_NONE;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_NONE;
    commandList->appendSignalEvent(event.toHandle());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenAppendWaitEventsWithDcFlushThenMiFlushDWIsProgrammed) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, true));
    auto &commandContainer = commandList->commandContainer;
    MockEvent event;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_NONE;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    auto eventHandle = event.toHandle();
    commandList->appendWaitOnEvents(1, &eventHandle);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListyWhenAppendWaitEventsWithDcFlushThePipeControlIsProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
    auto &commandContainer = commandList->commandContainer;
    MockEvent event;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_NONE;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    auto eventHandle = event.toHandle();
    commandList->appendWaitOnEvents(1, &eventHandle);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenTimestampPassedToMemoryCopyThenAppendProfilingCalledOnceBeforeAndAfterCommand, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockAppendMemoryCopy<gfxCoreFamily> commandList;
    commandList.initialize(device, false);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    ze_event_pool_desc_t eventPoolDesc = {
        ZE_EVENT_POOL_DESC_VERSION_CURRENT,
        ZE_EVENT_POOL_FLAG_TIMESTAMP,
        1};
    ze_event_desc_t eventDesc = {
        ZE_EVENT_DESC_VERSION_CURRENT,
        0,
        ZE_EVENT_SCOPE_FLAG_NONE,
        ZE_EVENT_SCOPE_FLAG_NONE};
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));

    commandList.appendMemoryCopy(dstPtr, srcPtr, 0x100, event->toHandle(), 0, nullptr);
    EXPECT_GT(commandList.appendMemoryCopyKernelWithGACalledTimes, 0u);
    EXPECT_EQ(commandList.appendMemoryCopyBlitCalledTimes, 0u);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList.commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);

    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
}

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListForMemFill : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    MockCommandListForMemFill() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>() {}

    AlignedAllocationData getAlignedAllocation(L0::Device *device, const void *buffer, uint64_t bufferSize) override {
        return {0, 0, nullptr, true};
    }
    ze_result_t appendMemoryCopyBlit(uintptr_t dstPtr,
                                     NEO::GraphicsAllocation *dstPtrAlloc,
                                     uint64_t dstOffset, uintptr_t srcPtr,
                                     NEO::GraphicsAllocation *srcPtrAlloc,
                                     uint64_t srcOffset,
                                     uint32_t size,
                                     ze_event_handle_t hSignalEvent) override {
        appendMemoryCopyBlitCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }
    uint32_t appendMemoryCopyBlitCalledTimes = 0;
};

HWTEST2_F(CommandListCreate, givenCopyOnlyCommandListWhenAppenBlitFillCalledWithLargePatternSizeThenMemCopyWasCalled, Platforms) {
    MockCommandListForMemFill<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    uint64_t pattern[4] = {1, 2, 3, 4};
    void *ptr = reinterpret_cast<void *>(0x1234);
    cmdList.appendMemoryFill(ptr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0x1000, nullptr);
    EXPECT_GT(cmdList.appendMemoryCopyBlitCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCopyOnlyCommandListWhenAppenBlitFillToNotDeviceMemThenInvalidArgumentReturned, Platforms) {
    MockCommandListForMemFill<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    uint8_t pattern = 1;
    void *ptr = reinterpret_cast<void *>(0x1234);
    auto ret = cmdList.appendMemoryFill(ptr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0x1000, nullptr);
    EXPECT_EQ(ret, ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

HWTEST2_F(CommandListCreate, givenCopyOnlyCommandListWhenAppenBlitFillThenCopyBltIsProgrammed, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;
    MockCommandListForMemFill<gfxCoreFamily> commandList;
    MockDriverHandle driverHandleMock;
    device->setDriverHandle(&driverHandleMock);
    commandList.initialize(device, true);
    uint16_t pattern = 1;
    void *ptr = reinterpret_cast<void *>(0x1234);
    commandList.appendMemoryFill(ptr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0x1000, nullptr);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0), commandList.commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COLOR_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    device->setDriverHandle(driverHandle.get());
}

using ImageSupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenCopyFromMemoryToImageThenBlitImageCopyCalled, ImageSupport) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    ze_image_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, &dstRegion, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.appendCopyImageBlitCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhenImageCopyFromMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.height = 1;
    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhenImageCopyToMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.height = 1;
    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen1DImageCopyFromMemoryWithInvalidHeightAndDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_1D;
    zeDesc.height = 9;
    zeDesc.depth = 9;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.height = 1;
    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen1DImageCopyToMemoryWithInvalidHeightAndDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_1D;
    zeDesc.height = 9;
    zeDesc.depth = 9;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.height = 1;
    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen1DArrayImageCopyFromMemoryWithInvalidHeightAndDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_1DARRAY;
    zeDesc.height = 9;
    zeDesc.depth = 9;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.height = 1;
    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen1DArrayImageCopyToMemoryWithInvalidHeightAndDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_1DARRAY;
    zeDesc.height = 9;
    zeDesc.depth = 9;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.height = 1;
    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen2DImageCopyToMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.height = 2;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen2DImageCopyFromMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.height = 2;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen2DImageCopyToMemoryWithInvalidDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.height = 2;
    zeDesc.depth = 9;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen2DImageCopyFromMemoryWithInvalidDepthThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.height = 2;
    zeDesc.depth = 9;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    zeDesc.depth = 1;

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen3DImageCopyToMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.height = 2;
    zeDesc.depth = 2;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionSrcOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListAndNullDestinationRegionWhen3DImageCopyFromMemoryThenBlitImageCopyCalledWithCorrectImageSize, ImageSupport) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.height = 2;
    zeDesc.depth = 2;
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    cmdList.appendImageCopyFromMemory(imageHW->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendImageRegionCopySize, expectedRegionCopySize);
    EXPECT_EQ(cmdList.appendImageRegionDstOrigin, expectedRegionOrigin);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenCopyFromImageToMemoryThenBlitImageCopyCalled, ImageSupport) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHW->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendImageCopyToMemory(dstPtr, imageHW->toHandle(), &srcRegion, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.appendCopyImageBlitCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenCopyFromImageToImageThenBlitImageCopyCalled, ImageSupport) {
    MockCommandList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    ze_image_desc_t zeDesc = {};
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &zeDesc);
    imageHWDst->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    ze_image_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    cmdList.appendImageCopyRegion(imageHWDst->toHandle(), imageHWSrc->toHandle(), &dstRegion, &srcRegion, nullptr, 0, nullptr);
    EXPECT_GT(cmdList.appendCopyImageBlitCalledTimes, 0u);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenCopyFromImagBlitThenCommandAddedToStream, ImageSupport) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, true));
    ze_image_desc_t zeDesc = {};
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &zeDesc);
    imageHWDst->initialize(device, &zeDesc);

    commandList->appendImageCopyRegion(imageHWDst->toHandle(), imageHWSrc->toHandle(), nullptr, nullptr, nullptr, 0, nullptr);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenTimestampPassedToMemoryCopyThenAppendProfilingCalledOnceBeforeAndAfterCommand, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using MI_FLUSH_DW = typename GfxFamily::MI_FLUSH_DW;

    MockAppendMemoryCopy<gfxCoreFamily> commandList;
    commandList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_EVENT_POOL_DESC_VERSION_CURRENT,
        ZE_EVENT_POOL_FLAG_TIMESTAMP,
        1};
    ze_event_desc_t eventDesc = {
        ZE_EVENT_DESC_VERSION_CURRENT,
        0,
        ZE_EVENT_SCOPE_FLAG_NONE,
        ZE_EVENT_SCOPE_FLAG_NONE};
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));

    commandList.appendMemoryCopy(dstPtr, srcPtr, 0x100, event->toHandle(), 0, nullptr);
    EXPECT_GT(commandList.appendMemoryCopyBlitCalledTimes, 1u);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList.commandContainer.getCommandStream()->getCpuBase(), 0), commandList.commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);

    itor = find<MI_FLUSH_DW *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    itor++;
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenTimestampPassedToMemoryCopyRegionBlitThenTimeStampRegistersAreAdded, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_EVENT_POOL_DESC_VERSION_CURRENT,
        ZE_EVENT_POOL_FLAG_TIMESTAMP,
        1};
    ze_event_desc_t eventDesc = {
        ZE_EVENT_DESC_VERSION_CURRENT,
        0,
        ZE_EVENT_SCOPE_FLAG_NONE,
        ZE_EVENT_SCOPE_FLAG_NONE};
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));

    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    NEO::MockGraphicsAllocation mockAllocationSrc(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    NEO::MockGraphicsAllocation mockAllocationDst(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);

    commandList->appendMemoryCopyBlitRegion(&mockAllocationDst, &mockAllocationSrc, srcRegion, dstRegion, {0, 0, 0}, 0, 0, 0, 0, 0, 0, event->toHandle());
    GenCmdList cmdList;

    auto baseAddr = event->getGpuAddress();
    auto contextStartOffset = offsetof(KernelTimestampEvent, contextStart);
    auto globalStartOffset = offsetof(KernelTimestampEvent, globalStart);
    auto contextEndOffset = offsetof(KernelTimestampEvent, contextEnd);
    auto globalEndOffset = offsetof(KernelTimestampEvent, globalEnd);

    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, globalStartOffset));
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, contextStartOffset));
    itor++;
    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, globalEndOffset));
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, contextEndOffset));
    itor++;
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenTimestampPassedToImageCopyBlitThenTimeStampRegistersAreAdded, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_EVENT_POOL_DESC_VERSION_CURRENT,
        ZE_EVENT_POOL_FLAG_TIMESTAMP,
        1};
    ze_event_desc_t eventDesc = {
        ZE_EVENT_DESC_VERSION_CURRENT,
        0,
        ZE_EVENT_SCOPE_FLAG_NONE,
        ZE_EVENT_SCOPE_FLAG_NONE};
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));

    NEO::MockGraphicsAllocation mockAllocationSrc(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    NEO::MockGraphicsAllocation mockAllocationDst(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);

    commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 0, 0, 0, 0, 1, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, event->toHandle());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenProfilingBeforeCommandForCopyOnlyThenCommandsHaveCorrectEventOffsets, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_EVENT_POOL_DESC_VERSION_CURRENT,
        ZE_EVENT_POOL_FLAG_TIMESTAMP,
        1};
    ze_event_desc_t eventDesc = {
        ZE_EVENT_DESC_VERSION_CURRENT,
        0,
        ZE_EVENT_SCOPE_FLAG_NONE,
        ZE_EVENT_SCOPE_FLAG_NONE};
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));

    commandList->appendEventForProfilingCopyCommand(event->toHandle(), true);

    auto contextOffset = offsetof(KernelTimestampEvent, contextStart);
    auto globalOffset = offsetof(KernelTimestampEvent, globalStart);
    auto baseAddr = event->getGpuAddress();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, globalOffset));
    EXPECT_NE(cmdList.end(), ++itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, contextOffset));
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenProfilingAfterCommandForCopyOnlyThenCommandsHaveCorrectEventOffsets, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_EVENT_POOL_DESC_VERSION_CURRENT,
        ZE_EVENT_POOL_FLAG_TIMESTAMP,
        1};
    ze_event_desc_t eventDesc = {
        ZE_EVENT_DESC_VERSION_CURRENT,
        0,
        ZE_EVENT_SCOPE_FLAG_NONE,
        ZE_EVENT_SCOPE_FLAG_NONE};
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));

    commandList->appendEventForProfilingCopyCommand(event->toHandle(), false);

    auto contextOffset = offsetof(KernelTimestampEvent, contextEnd);
    auto globalOffset = offsetof(KernelTimestampEvent, globalEnd);
    auto baseAddr = event->getGpuAddress();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, globalOffset));
    EXPECT_NE(cmdList.end(), ++itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, contextOffset));
}

HWTEST2_F(CommandListCreate, givenNullEventWhenAppendEventAfterWalkerThenNothingAddedToStream, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);

    auto usedBefore = commandList->commandContainer.getCommandStream()->getUsed();

    commandList->appendSignalEventPostWalker(nullptr);

    EXPECT_EQ(commandList->commandContainer.getCommandStream()->getUsed(), usedBefore);
}
HWTEST2_F(CommandListCreate, givenCopyOnlyCommandListWhenAppendBlitFillCalledWithLargePatternSizeThenInternalAllocHasPattern, Platforms) {
    MockCommandListForMemFill<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    uint64_t pattern[4] = {1, 2, 3, 4};
    void *ptr = reinterpret_cast<void *>(0x1234);
    uint32_t fillElements = 0x101;
    auto size = fillElements * sizeof(uint64_t);
    cmdList.appendMemoryFill(ptr, reinterpret_cast<void *>(&pattern), sizeof(pattern), size, nullptr);
    auto internalAlloc = cmdList.commandContainer.getDeallocationContainer()[0];
    for (uint32_t i = 0; i < fillElements; i++) {
        auto allocValue = reinterpret_cast<uint64_t *>(internalAlloc->getUnderlyingBuffer())[i];
        EXPECT_EQ(allocValue, pattern[i % 4]);
    }
}

HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenGettingAllocInRangeThenAllocFromMapReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));

    auto newBufferPtr = ptrOffset(cpuPtr, 0x10);
    auto newBufferSize = allocSize - 0x20;
    auto newAlloc = commandList->getAllocationFromHostPtrMap(newBufferPtr, newBufferSize);
    EXPECT_NE(newAlloc, nullptr);
    commandList->hostPtrMap.clear();
}

HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenSizeIsOutOfRangeThenNullPtrReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));

    auto newBufferPtr = ptrOffset(cpuPtr, 0x10);
    auto newBufferSize = allocSize + 0x20;
    auto newAlloc = commandList->getAllocationFromHostPtrMap(newBufferPtr, newBufferSize);
    EXPECT_EQ(newAlloc, nullptr);
    commandList->hostPtrMap.clear();
}

HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenPtrIsOutOfRangeThenNullPtrReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));

    auto newBufferPtr = reinterpret_cast<const void *>(gpuAddress - 0x100);
    auto newBufferSize = allocSize - 0x200;
    auto newAlloc = commandList->getAllocationFromHostPtrMap(newBufferPtr, newBufferSize);
    EXPECT_EQ(newAlloc, nullptr);
    commandList->hostPtrMap.clear();
}

HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenGetHostPtrAllocCalledThenCorrectOffsetIsSet, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));
    size_t expectedOffset = 0x10;
    auto newBufferPtr = ptrOffset(cpuPtr, expectedOffset);
    auto newBufferSize = allocSize - 0x20;
    size_t offset = 0;
    auto newAlloc = commandList->getHostPtrAlloc(newBufferPtr, newBufferSize, &offset);
    EXPECT_NE(newAlloc, nullptr);
    EXPECT_EQ(offset, expectedOffset);
    commandList->hostPtrMap.clear();
}

HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenPtrIsInMapThenAllocationReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));

    auto newBufferPtr = cpuPtr;
    auto newBufferSize = allocSize - 0x20;
    auto newAlloc = commandList->getAllocationFromHostPtrMap(newBufferPtr, newBufferSize);
    EXPECT_EQ(newAlloc, &alloc);
    commandList->hostPtrMap.clear();
}
HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenPtrIsInMapButWithBiggerSizeThenNullPtrReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));

    auto newBufferPtr = cpuPtr;
    auto newBufferSize = allocSize + 0x20;
    auto newAlloc = commandList->getAllocationFromHostPtrMap(newBufferPtr, newBufferSize);
    EXPECT_EQ(newAlloc, nullptr);
    commandList->hostPtrMap.clear();
}
HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenPtrLowerThanAnyInMapThenNullPtrReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));

    auto newBufferPtr = reinterpret_cast<const void *>(gpuAddress - 0x10);
    auto newBufferSize = allocSize - 0x20;
    auto newAlloc = commandList->getAllocationFromHostPtrMap(newBufferPtr, newBufferSize);
    EXPECT_EQ(newAlloc, nullptr);
    commandList->hostPtrMap.clear();
}
using BlitBlockCopyPlatforms = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;
HWTEST2_F(CommandListCreate, givenCopyCommandListWhenCopyRegionWithinMaxBlitSizeThenOneBlitCommandHasBeenSpown, BlitBlockCopyPlatforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    uint32_t offsetX = 0x10;
    uint32_t offsetY = 0x10;
    Vec3<size_t> copySize = {0x100, 0x10, 1};
    ze_copy_region_t srcRegion = {offsetX, offsetY, 0, static_cast<uint32_t>(copySize.x), static_cast<uint32_t>(copySize.y), static_cast<uint32_t>(copySize.z)};
    ze_copy_region_t dstRegion = srcRegion;
    Vec3<uint32_t> srcSize = {0x1000, 0x100, 1};
    Vec3<uint32_t> dstSize = {0x100, 0x100, 1};
    NEO::MockGraphicsAllocation mockAllocationSrc(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    NEO::MockGraphicsAllocation mockAllocationDst(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    size_t rowPitch = copySize.x;
    size_t slicePitch = copySize.x * copySize.y;
    commandList->appendMemoryCopyBlitRegion(&mockAllocationDst, &mockAllocationSrc, srcRegion, dstRegion, copySize, rowPitch, slicePitch, rowPitch, slicePitch, srcSize, dstSize, nullptr);
    GenCmdList cmdList;

    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenCopyRegionWithinMaxBlitSizeThenOffsetAndSizeAreInPixels, BlitBlockCopyPlatforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    uint32_t offsetX = 0x10;
    uint32_t offsetY = 0x10;
    Vec3<size_t> copySize = {0x100, 0x10, 1};
    ze_copy_region_t srcRegion = {offsetX, offsetY, 0, static_cast<uint32_t>(copySize.x), static_cast<uint32_t>(copySize.y), static_cast<uint32_t>(copySize.z)};
    ze_copy_region_t dstRegion = srcRegion;
    Vec3<uint32_t> srcSize = {0x1000, 0x100, 1};
    Vec3<uint32_t> dstSize = {0x100, 0x100, 1};
    NEO::MockGraphicsAllocation mockAllocationSrc(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    NEO::MockGraphicsAllocation mockAllocationDst(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    size_t rowPitch = copySize.x;
    size_t slicePitch = copySize.x * copySize.y;
    commandList->appendMemoryCopyBlitRegion(&mockAllocationDst, &mockAllocationSrc, srcRegion, dstRegion, copySize, rowPitch, slicePitch, rowPitch, slicePitch, srcSize, dstSize, nullptr);
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<FamilyType>::getAvailableBytesPerPixel(copySize.x, srcRegion.originX, dstRegion.originY, srcSize.x, dstSize.x);
    GenCmdList cmdList;

    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    auto cmd = genCmdCast<XY_COPY_BLT *>(*itor);
    EXPECT_EQ(cmd->getDestinationX1CoordinateLeft(), offsetX / bytesPerPixel);
    EXPECT_EQ(cmd->getTransferWidth(), (offsetX + static_cast<uint32_t>(copySize.x)) / bytesPerPixel);
}
HWTEST2_F(CommandListCreate, givenCopyCommandListWhenCopyRegionGreaterThanMaxBlitSizeThenMoreThanOneBlitCommandHasBeenSpown, BlitBlockCopyPlatforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    uint32_t offsetX = 0x1;
    uint32_t offsetY = 0x1;
    Vec3<size_t> copySize = {BlitterConstants::maxBlitWidth + 0x100, 0x10, 1};
    ze_copy_region_t srcRegion = {offsetX, offsetY, 0, static_cast<uint32_t>(copySize.x), static_cast<uint32_t>(copySize.y), static_cast<uint32_t>(copySize.z)};
    ze_copy_region_t dstRegion = srcRegion;
    Vec3<uint32_t> srcSize = {2 * BlitterConstants::maxBlitWidth, 2 * BlitterConstants::maxBlitHeight, 1};
    Vec3<uint32_t> dstSize = srcSize;
    NEO::MockGraphicsAllocation mockAllocationSrc(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    NEO::MockGraphicsAllocation mockAllocationDst(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    size_t rowPitch = copySize.x;
    size_t slicePitch = copySize.x * copySize.y;
    commandList->appendMemoryCopyBlitRegion(&mockAllocationDst, &mockAllocationSrc, srcRegion, dstRegion, copySize, rowPitch, slicePitch, rowPitch, slicePitch, srcSize, dstSize, nullptr);
    GenCmdList cmdList;

    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    EXPECT_NE(cmdList.end(), itor);
}

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListForRegionSize : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    MockCommandListForRegionSize() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>() {}

    AlignedAllocationData getAlignedAllocation(L0::Device *device, const void *buffer, uint64_t bufferSize) override {
        return {0, 0, nullptr, true};
    }
    ze_result_t appendMemoryCopyBlitRegion(NEO::GraphicsAllocation *srcptr,
                                           NEO::GraphicsAllocation *dstptr,
                                           ze_copy_region_t srcRegion,
                                           ze_copy_region_t dstRegion, Vec3<size_t> copySize,
                                           size_t srcRowPitch, size_t srcSlicePitch,
                                           size_t dstRowPitch, size_t dstSlicePitch,
                                           Vec3<uint32_t> srcSize, Vec3<uint32_t> dstSize, ze_event_handle_t hSignalEvent) override {
        this->srcSize = srcSize;
        this->dstSize = dstSize;
        return ZE_RESULT_SUCCESS;
    }
    Vec3<uint32_t> srcSize = {0, 0, 0};
    Vec3<uint32_t> dstSize = {0, 0, 0};
};
HWTEST2_F(CommandListCreate, givenZeroAsPitchAndSlicePitchWhenMemoryCopyRegionCalledSizesEqualOffsetPlusCopySize, Platforms) {
    MockCommandListForRegionSize<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {0x10, 0x10, 0, 0x100, 0x100, 1};
    ze_copy_region_t srcRegion = dstRegion;
    uint32_t pitch = 0;
    uint32_t slicePitch = 0;
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, pitch, slicePitch, srcPtr, &srcRegion, pitch, slicePitch, nullptr);
    EXPECT_EQ(cmdList.dstSize.x, dstRegion.width + dstRegion.originX);
    EXPECT_EQ(cmdList.dstSize.y, dstRegion.height + dstRegion.originY);
    EXPECT_EQ(cmdList.dstSize.z, dstRegion.depth + dstRegion.originZ);

    EXPECT_EQ(cmdList.srcSize.x, srcRegion.width + srcRegion.originX);
    EXPECT_EQ(cmdList.srcSize.y, srcRegion.height + srcRegion.originY);
    EXPECT_EQ(cmdList.srcSize.z, srcRegion.depth + srcRegion.originZ);
}

HWTEST2_F(CommandListCreate, givenPitchAndSlicePitchWhenMemoryCopyRegionCalledSizesAreBasedOnPitch, Platforms) {
    MockCommandListForRegionSize<gfxCoreFamily> cmdList;
    cmdList.initialize(device, true);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {0x10, 0x10, 0, 0x100, 0x100, 1};
    ze_copy_region_t srcRegion = dstRegion;
    uint32_t pitch = 0x1000;
    uint32_t slicePitch = 0x100000;
    cmdList.appendMemoryCopyRegion(dstPtr, &dstRegion, pitch, slicePitch, srcPtr, &srcRegion, pitch, slicePitch, nullptr);
    EXPECT_EQ(cmdList.dstSize.x, pitch);
    EXPECT_EQ(cmdList.dstSize.y, slicePitch / pitch);

    EXPECT_EQ(cmdList.srcSize.x, pitch);
    EXPECT_EQ(cmdList.srcSize.y, slicePitch / pitch);
}
HWTEST2_F(AppendMemoryCopy, givenCopyOnlyCommandListWhenWithDcFlushAddedIsNotAddedAfterBlitCopy, Platforms) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, true);
    uintptr_t srcPtr = 0x5001;
    uintptr_t dstPtr = 0x7001;
    uint64_t srcOffset = 0x101;
    uint64_t dstOffset = 0x201;
    uint32_t copySize = 0x301;
    NEO::MockGraphicsAllocation mockAllocationSrc(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(srcPtr), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    NEO::MockGraphicsAllocation mockAllocationDst(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(dstPtr), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    commandList->appendMemoryCopyBlit(ptrOffset(dstPtr, dstOffset), &mockAllocationDst, 0, ptrOffset(srcPtr, srcOffset), &mockAllocationSrc, 0, copySize, nullptr);

    auto &commandContainer = commandList->commandContainer;
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        genCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);
    auto cmd = genCmdCast<XY_COPY_BLT *>(*itor);
    EXPECT_EQ(cmd->getDestinationBaseAddress(), ptrOffset(dstPtr, dstOffset));
    EXPECT_EQ(cmd->getSourceBaseAddress(), ptrOffset(srcPtr, srcOffset));
}

} // namespace ult
} // namespace L0
