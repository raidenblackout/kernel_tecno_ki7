/*
 * Copyright 2022 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include "amdgpu.h"
#include "amdgpu_atombios.h"
#include "nbio_v7_9.h"
#include "amdgpu_ras.h"

#include "nbio/nbio_7_9_0_offset.h"
#include "nbio/nbio_7_9_0_sh_mask.h"
#include "ivsrcid/nbio/irqsrcs_nbif_7_4.h"
#include <uapi/linux/kfd_ioctl.h>

static void nbio_v7_9_remap_hdp_registers(struct amdgpu_device *adev)
{
	WREG32_SOC15(NBIO, 0, regBIF_BX0_REMAP_HDP_MEM_FLUSH_CNTL,
		adev->rmmio_remap.reg_offset + KFD_MMIO_REMAP_HDP_MEM_FLUSH_CNTL);
	WREG32_SOC15(NBIO, 0, regBIF_BX0_REMAP_HDP_REG_FLUSH_CNTL,
		adev->rmmio_remap.reg_offset + KFD_MMIO_REMAP_HDP_REG_FLUSH_CNTL);
}

static u32 nbio_v7_9_get_rev_id(struct amdgpu_device *adev)
{
	u32 tmp;

	tmp = RREG32_SOC15(NBIO, 0, regRCC_STRAP0_RCC_DEV0_EPF0_STRAP0);
	tmp = REG_GET_FIELD(tmp, RCC_STRAP0_RCC_DEV0_EPF0_STRAP0, STRAP_ATI_REV_ID_DEV0_F0);

	return tmp;
}

static void nbio_v7_9_mc_access_enable(struct amdgpu_device *adev, bool enable)
{
	if (enable)
		WREG32_SOC15(NBIO, 0, regBIF_BX0_BIF_FB_EN,
			BIF_BX0_BIF_FB_EN__FB_READ_EN_MASK | BIF_BX0_BIF_FB_EN__FB_WRITE_EN_MASK);
	else
		WREG32_SOC15(NBIO, 0, regBIF_BX0_BIF_FB_EN, 0);
}

static u32 nbio_v7_9_get_memsize(struct amdgpu_device *adev)
{
	return RREG32_SOC15(NBIO, 0, regRCC_DEV0_EPF0_RCC_CONFIG_MEMSIZE);
}

static void nbio_v7_9_sdma_doorbell_range(struct amdgpu_device *adev, int instance,
			bool use_doorbell, int doorbell_index, int doorbell_size)
{
	u32 doorbell_range = 0, doorbell_ctrl = 0;

	doorbell_range =
		REG_SET_FIELD(doorbell_range, DOORBELL0_CTRL_ENTRY_0,
			BIF_DOORBELL0_RANGE_OFFSET_ENTRY, doorbell_index);
	doorbell_range =
		REG_SET_FIELD(doorbell_range, DOORBELL0_CTRL_ENTRY_0,
			BIF_DOORBELL0_RANGE_SIZE_ENTRY, doorbell_size);
	doorbell_ctrl =
		REG_SET_FIELD(doorbell_ctrl, S2A_DOORBELL_ENTRY_1_CTRL,
			S2A_DOORBELL_PORT1_ENABLE, 1);
	doorbell_ctrl =
		REG_SET_FIELD(doorbell_ctrl, S2A_DOORBELL_ENTRY_1_CTRL,
			S2A_DOORBELL_PORT1_RANGE_SIZE, doorbell_size);

	switch (instance) {
	case 0:
		WREG32_SOC15(NBIO, 0, regDOORBELL0_CTRL_ENTRY_1, doorbell_range);

		doorbell_ctrl = REG_SET_FIELD(doorbell_ctrl,
					S2A_DOORBELL_ENTRY_1_CTRL,
					S2A_DOORBELL_PORT1_AWID, 0xe);
		doorbell_ctrl = REG_SET_FIELD(doorbell_ctrl,
					S2A_DOORBELL_ENTRY_1_CTRL,
					S2A_DOORBELL_PORT1_RANGE_OFFSET, 0xe);
		doorbell_ctrl = REG_SET_FIELD(doorbell_ctrl,
					S2A_DOORBELL_ENTRY_1_CTRL,
					S2A_DOORBELL_PORT1_AWADDR_31_28_VALUE,
					0x1);
		WREG32_SOC15(NBIO, 0, regS2A_DOORBELL_ENTRY_1_CTRL, doorbell_ctrl);
		break;
	case 1:
		WREG32_SOC15(NBIO, 0, regDOORBELL0_CTRL_ENTRY_2, doorbell_range);

		doorbell_ctrl = REG_SET_FIELD(doorbell_ctrl,
					S2A_DOORBELL_ENTRY_1_CTRL,
					S2A_DOORBELL_PORT1_AWID, 0x8);
		doorbell_ctrl = REG_SET_FIELD(doorbell_ctrl,
					S2A_DOORBELL_ENTRY_1_CTRL,
					S2A_DOORBELL_PORT1_RANGE_OFFSET, 0x8);
		doorbell_ctrl = REG_SET_FIELD(doorbell_ctrl,
					S2A_DOORBELL_ENTRY_1_CTRL,
					S2A_DOORBELL_PORT1_AWADDR_31_28_VALUE,
					0x2);
		WREG32_SOC15(NBIO, 0, regS2A_DOORBELL_ENTRY_2_CTRL, doorbell_ctrl);
		break;
	case 2:
		WREG32_SOC15(NBIO, 0, regDOORBELL0_CTRL_ENTRY_3, doorbell_range);

		doorbell_ctrl = REG_SET_FIELD(doorbell_ctrl,
					S2A_DOORBELL_ENTRY_1_CTRL,
					S2A_DOORBELL_PORT1_AWID, 0x9);
		doorbell_ctrl = REG_SET_FIELD(doorbell_ctrl,
					S2A_DOORBELL_ENTRY_1_CTRL,
					S2A_DOORBELL_PORT1_RANGE_OFFSET, 0x9);
		doorbell_ctrl = REG_SET_FIELD(doorbell_ctrl,
					S2A_DOORBELL_ENTRY_1_CTRL,
					S2A_DOORBELL_PORT1_AWADDR_31_28_VALUE,
					0x8);
		WREG32_SOC15(NBIO, 0, regS2A_DOORBELL_ENTRY_5_CTRL, doorbell_ctrl);
		break;
	case 3:
		WREG32_SOC15(NBIO, 0, regDOORBELL0_CTRL_ENTRY_4, doorbell_range);

		doorbell_ctrl = REG_SET_FIELD(doorbell_ctrl,
					S2A_DOORBELL_ENTRY_1_CTRL,
					S2A_DOORBELL_PORT1_AWID, 0xa);
		doorbell_ctrl = REG_SET_FIELD(doorbell_ctrl,
					S2A_DOORBELL_ENTRY_1_CTRL,
					S2A_DOORBELL_PORT1_RANGE_OFFSET, 0xa);
		doorbell_ctrl = REG_SET_FIELD(doorbell_ctrl,
					S2A_DOORBELL_ENTRY_1_CTRL,
					S2A_DOORBELL_PORT1_AWADDR_31_28_VALUE,
					0x9);
		WREG32_SOC15(NBIO, 0, regS2A_DOORBELL_ENTRY_6_CTRL, doorbell_ctrl);
		break;
	default:
		break;
	};

	return;
}

static void nbio_v7_9_vcn_doorbell_range(struct amdgpu_device *adev, bool use_doorbell,
					 int doorbell_index, int instance)
{
	u32 doorbell_range = 0, doorbell_ctrl = 0;

	if (use_doorbell) {
		doorbell_range = REG_SET_FIELD(doorbell_range,
				DOORBELL0_CTRL_ENTRY_0,
				BIF_DOORBELL0_RANGE_OFFSET_ENTRY,
				doorbell_index);
		doorbell_range = REG_SET_FIELD(doorbell_range,
				DOORBELL0_CTRL_ENTRY_0,
				BIF_DOORBELL0_RANGE_SIZE_ENTRY,
				0x8);

		doorbell_ctrl = REG_SET_FIELD(doorbell_ctrl,
				S2A_DOORBELL_ENTRY_1_CTRL,
				S2A_DOORBELL_PORT1_ENABLE, 1);
		doorbell_ctrl = REG_SET_FIELD(doorbell_ctrl,
				S2A_DOORBELL_ENTRY_1_CTRL,
				S2A_DOORBELL_PORT1_AWID, 0x4);
		doorbell_ctrl = REG_SET_FIELD(doorbell_ctrl,
				S2A_DOORBELL_ENTRY_1_CTRL,
				S2A_DOORBELL_PORT1_RANGE_OFFSET, 0x4);
		doorbell_ctrl = REG_SET_FIELD(doorbell_ctrl,
				S2A_DOORBELL_ENTRY_1_CTRL,
				S2A_DOORBELL_PORT1_RANGE_SIZE, 0x8);
		doorbell_ctrl = REG_SET_FIELD(doorbell_ctrl,
				S2A_DOORBELL_ENTRY_1_CTRL,
				S2A_DOORBELL_PORT1_AWADDR_31_28_VALUE, 0x4);
	} else {
		doorbell_range = REG_SET_FIELD(doorbell_range,
				DOORBELL0_CTRL_ENTRY_0,
				BIF_DOORBELL0_RANGE_SIZE_ENTRY, 0);
		doorbell_ctrl = REG_SET_FIELD(doorbell_ctrl,
				S2A_DOORBELL_ENTRY_1_CTRL,
				S2A_DOORBELL_PORT1_RANGE_SIZE, 0);
	}

	WREG32_SOC15(NBIO, 0, regDOORBELL0_CTRL_ENTRY_17, doorbell_range);
	WREG32_SOC15(NBIO, 0, regS2A_DOORBELL_ENTRY_4_CTRL, doorbell_ctrl);
}

static void nbio_v7_9_enable_doorbell_aperture(struct amdgpu_device *adev,
					       bool enable)
{
	/* Enable to allow doorbell pass thru on pre-silicon bare-metal */
	WREG32_SOC15(NBIO, 0, regBIFC_DOORBELL_ACCESS_EN_PF, 0xfffff);
	WREG32_FIELD15_PREREG(NBIO, 0, RCC_DEV0_EPF0_RCC_DOORBELL_APER_EN,
			BIF_DOORBELL_APER_EN, enable ? 1 : 0);
}

static void nbio_v7_9_enable_doorbell_selfring_aperture(struct amdgpu_device *adev,
							bool enable)
{
	u32 tmp = 0;

	if (enable) {
		tmp = REG_SET_FIELD(tmp, BIF_BX_PF0_DOORBELL_SELFRING_GPA_APER_CNTL,
				    DOORBELL_SELFRING_GPA_APER_EN, 1) |
		      REG_SET_FIELD(tmp, BIF_BX_PF0_DOORBELL_SELFRING_GPA_APER_CNTL,
				    DOORBELL_SELFRING_GPA_APER_MODE, 1) |
		      REG_SET_FIELD(tmp, BIF_BX_PF0_DOORBELL_SELFRING_GPA_APER_CNTL,
				    DOORBELL_SELFRING_GPA_APER_SIZE, 0);

		WREG32_SOC15(NBIO, 0, regBIF_BX_PF0_DOORBELL_SELFRING_GPA_APER_BASE_LOW,
			     lower_32_bits(adev->doorbell.base));
		WREG32_SOC15(NBIO, 0, regBIF_BX_PF0_DOORBELL_SELFRING_GPA_APER_BASE_HIGH,
			     upper_32_bits(adev->doorbell.base));
	}

	WREG32_SOC15(NBIO, 0, regBIF_BX_PF0_DOORBELL_SELFRING_GPA_APER_CNTL, tmp);
}

static void nbio_v7_9_ih_doorbell_range(struct amdgpu_device *adev,
					bool use_doorbell, int doorbell_index)
{
	u32 ih_doorbell_range = 0, ih_doorbell_ctrl = 0;

	if (use_doorbell) {
		ih_doorbell_range = REG_SET_FIELD(ih_doorbell_range,
				DOORBELL0_CTRL_ENTRY_0,
				BIF_DOORBELL0_RANGE_OFFSET_ENTRY,
				doorbell_index);
		ih_doorbell_range = REG_SET_FIELD(ih_doorbell_range,
				DOORBELL0_CTRL_ENTRY_0,
				BIF_DOORBELL0_RANGE_SIZE_ENTRY,
				0x4);

		ih_doorbell_ctrl = REG_SET_FIELD(ih_doorbell_ctrl,
				S2A_DOORBELL_ENTRY_1_CTRL,
				S2A_DOORBELL_PORT1_ENABLE, 1);
		ih_doorbell_ctrl = REG_SET_FIELD(ih_doorbell_ctrl,
				S2A_DOORBELL_ENTRY_1_CTRL,
				S2A_DOORBELL_PORT1_AWID, 0);
		ih_doorbell_ctrl = REG_SET_FIELD(ih_doorbell_ctrl,
				S2A_DOORBELL_ENTRY_1_CTRL,
				S2A_DOORBELL_PORT1_RANGE_OFFSET, 0);
		ih_doorbell_ctrl = REG_SET_FIELD(ih_doorbell_ctrl,
				S2A_DOORBELL_ENTRY_1_CTRL,
				S2A_DOORBELL_PORT1_RANGE_SIZE, 0x4);
		ih_doorbell_ctrl = REG_SET_FIELD(ih_doorbell_ctrl,
				S2A_DOORBELL_ENTRY_1_CTRL,
				S2A_DOORBELL_PORT1_AWADDR_31_28_VALUE, 0);
	} else {
		ih_doorbell_range = REG_SET_FIELD(ih_doorbell_range,
				DOORBELL0_CTRL_ENTRY_0,
				BIF_DOORBELL0_RANGE_SIZE_ENTRY, 0);
		ih_doorbell_ctrl = REG_SET_FIELD(ih_doorbell_ctrl,
				S2A_DOORBELL_ENTRY_1_CTRL,
				S2A_DOORBELL_PORT1_RANGE_SIZE, 0);
	}

	WREG32_SOC15(NBIO, 0, regDOORBELL0_CTRL_ENTRY_0, ih_doorbell_range);
	WREG32_SOC15(NBIO, 0, regS2A_DOORBELL_ENTRY_3_CTRL, ih_doorbell_ctrl);
}


static void nbio_v7_9_update_medium_grain_clock_gating(struct amdgpu_device *adev,
						       bool enable)
{
}

static void nbio_v7_9_update_medium_grain_light_sleep(struct amdgpu_device *adev,
						      bool enable)
{
}

static void nbio_v7_9_get_clockgating_state(struct amdgpu_device *adev,
					    u64 *flags)
{
}

static void nbio_v7_9_ih_control(struct amdgpu_device *adev)
{
	u32 interrupt_cntl;

	/* setup interrupt control */
	WREG32_SOC15(NBIO, 0, regBIF_BX0_INTERRUPT_CNTL2, adev->dummy_page_addr >> 8);
	interrupt_cntl = RREG32_SOC15(NBIO, 0, regBIF_BX0_INTERRUPT_CNTL);
	/* INTERRUPT_CNTL__IH_DUMMY_RD_OVERRIDE_MASK=0 - dummy read disabled with msi, enabled without msi
	 * INTERRUPT_CNTL__IH_DUMMY_RD_OVERRIDE_MASK=1 - dummy read controlled by IH_DUMMY_RD_EN
	 */
	interrupt_cntl =
		REG_SET_FIELD(interrupt_cntl, BIF_BX0_INTERRUPT_CNTL, IH_DUMMY_RD_OVERRIDE, 0);
	/* INTERRUPT_CNTL__IH_REQ_NONSNOOP_EN_MASK=1 if ring is in non-cacheable memory, e.g., vram */
	interrupt_cntl =
		REG_SET_FIELD(interrupt_cntl, BIF_BX0_INTERRUPT_CNTL, IH_REQ_NONSNOOP_EN, 0);
	WREG32_SOC15(NBIO, 0, regBIF_BX0_INTERRUPT_CNTL, interrupt_cntl);
}

static u32 nbio_v7_9_get_hdp_flush_req_offset(struct amdgpu_device *adev)
{
	return SOC15_REG_OFFSET(NBIO, 0, regBIF_BX_PF0_GPU_HDP_FLUSH_REQ);
}

static u32 nbio_v7_9_get_hdp_flush_done_offset(struct amdgpu_device *adev)
{
	return SOC15_REG_OFFSET(NBIO, 0, regBIF_BX_PF0_GPU_HDP_FLUSH_DONE);
}

static u32 nbio_v7_9_get_pcie_index_offset(struct amdgpu_device *adev)
{
	return SOC15_REG_OFFSET(NBIO, 0, regBIF_BX0_PCIE_INDEX2);
}

static u32 nbio_v7_9_get_pcie_data_offset(struct amdgpu_device *adev)
{
	return SOC15_REG_OFFSET(NBIO, 0, regBIF_BX0_PCIE_DATA2);
}

const struct nbio_hdp_flush_reg nbio_v7_9_hdp_flush_reg = {
	.ref_and_mask_cp0 = BIF_BX_PF0_GPU_HDP_FLUSH_DONE__CP0_MASK,
	.ref_and_mask_cp1 = BIF_BX_PF0_GPU_HDP_FLUSH_DONE__CP1_MASK,
	.ref_and_mask_cp2 = BIF_BX_PF0_GPU_HDP_FLUSH_DONE__CP2_MASK,
	.ref_and_mask_cp3 = BIF_BX_PF0_GPU_HDP_FLUSH_DONE__CP3_MASK,
	.ref_and_mask_cp4 = BIF_BX_PF0_GPU_HDP_FLUSH_DONE__CP4_MASK,
	.ref_and_mask_cp5 = BIF_BX_PF0_GPU_HDP_FLUSH_DONE__CP5_MASK,
	.ref_and_mask_cp6 = BIF_BX_PF0_GPU_HDP_FLUSH_DONE__CP6_MASK,
	.ref_and_mask_cp7 = BIF_BX_PF0_GPU_HDP_FLUSH_DONE__CP7_MASK,
	.ref_and_mask_cp8 = BIF_BX_PF0_GPU_HDP_FLUSH_DONE__CP8_MASK,
	.ref_and_mask_cp9 = BIF_BX_PF0_GPU_HDP_FLUSH_DONE__CP9_MASK,
	.ref_and_mask_sdma0 = BIF_BX_PF0_GPU_HDP_FLUSH_DONE__SDMA0_MASK,
	.ref_and_mask_sdma1 = BIF_BX_PF0_GPU_HDP_FLUSH_DONE__SDMA1_MASK,
	.ref_and_mask_sdma2 = BIF_BX_PF0_GPU_HDP_FLUSH_DONE__RSVD_ENG0_MASK,
	.ref_and_mask_sdma3 = BIF_BX_PF0_GPU_HDP_FLUSH_DONE__RSVD_ENG1_MASK,
	.ref_and_mask_sdma4 = BIF_BX_PF0_GPU_HDP_FLUSH_DONE__RSVD_ENG2_MASK,
	.ref_and_mask_sdma5 = BIF_BX_PF0_GPU_HDP_FLUSH_DONE__RSVD_ENG3_MASK,
	.ref_and_mask_sdma6 = BIF_BX_PF0_GPU_HDP_FLUSH_DONE__RSVD_ENG4_MASK,
	.ref_and_mask_sdma7 = BIF_BX_PF0_GPU_HDP_FLUSH_DONE__RSVD_ENG5_MASK,
};

static void nbio_v7_9_enable_doorbell_interrupt(struct amdgpu_device *adev,
						bool enable)
{
	WREG32_FIELD15_PREREG(NBIO, 0, BIF_BX0_BIF_DOORBELL_INT_CNTL,
			      DOORBELL_INTERRUPT_DISABLE, enable ? 0 : 1);
}

const struct amdgpu_nbio_funcs nbio_v7_9_funcs = {
	.get_hdp_flush_req_offset = nbio_v7_9_get_hdp_flush_req_offset,
	.get_hdp_flush_done_offset = nbio_v7_9_get_hdp_flush_done_offset,
	.get_pcie_index_offset = nbio_v7_9_get_pcie_index_offset,
	.get_pcie_data_offset = nbio_v7_9_get_pcie_data_offset,
	.get_rev_id = nbio_v7_9_get_rev_id,
	.mc_access_enable = nbio_v7_9_mc_access_enable,
	.get_memsize = nbio_v7_9_get_memsize,
	.sdma_doorbell_range = nbio_v7_9_sdma_doorbell_range,
	.vcn_doorbell_range = nbio_v7_9_vcn_doorbell_range,
	.enable_doorbell_aperture = nbio_v7_9_enable_doorbell_aperture,
	.enable_doorbell_selfring_aperture = nbio_v7_9_enable_doorbell_selfring_aperture,
	.ih_doorbell_range = nbio_v7_9_ih_doorbell_range,
	.enable_doorbell_interrupt = nbio_v7_9_enable_doorbell_interrupt,
	.update_medium_grain_clock_gating = nbio_v7_9_update_medium_grain_clock_gating,
	.update_medium_grain_light_sleep = nbio_v7_9_update_medium_grain_light_sleep,
	.get_clockgating_state = nbio_v7_9_get_clockgating_state,
	.ih_control = nbio_v7_9_ih_control,
	.remap_hdp_registers = nbio_v7_9_remap_hdp_registers,
};
