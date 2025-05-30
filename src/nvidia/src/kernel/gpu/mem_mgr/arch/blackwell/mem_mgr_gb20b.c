/*

 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: MIT
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "core/core.h"
#include "gpu/gpu.h"
#include "gpu/mem_mgr/mem_mgr.h"
#include "gpu/mem_sys/kern_mem_sys.h"
#include "nvRmReg.h"

#include "published/blackwell/gb20b/kind_macros.h"
#include "published/blackwell/gb20b/dev_mmu.h"


/*!
 * @brief Determine the kind of uncompressed PTE for a given allocation.
 * GB20X only supported GMK or GMK compression kinds. If bOverrideToGMK is true, only return GMK otherwise
 * call the _TU102 function.
 *
 * @param[in] pFbAllocPageFormat FB_ALLOC_PAGE_FORMAT pointer
 *
 * @returns   PTE kind.
 */

NvU32
memmgrChooseKindZ_GB20B
(
    OBJGPU                 *pGpu,
    MemoryManager          *pMemoryManager,
    FB_ALLOC_PAGE_FORMAT   *pFbAllocPageFormat
)
{
    NvU32 kind = memmgrChooseKindZ_TU102(pGpu, pMemoryManager, pFbAllocPageFormat);

    if (kind == NV_MMU_PTE_KIND_S8)
    {
        return NV_MMU_PTE_KIND_S8;
    }

    if (kind == NV_MMU_PTE_KIND_Z16)
    {
        return NV_MMU_PTE_KIND_Z16;
    }

    return NV_MMU_PTE_KIND_GENERIC_MEMORY;
}



/*!
 * @brief Determine the kind of compressed PTE with PLC disabled for a given allocation.
 *
 * @param[in] pFbAllocPageFormat FB_ALLOC_PAGE_FORMAT pointer
 *
 * @returns   PTE kind.
 */
NvU32
memmgrChooseKindCompressZ_GB20B
(
    OBJGPU               *pGpu,
    MemoryManager        *pMemoryManager,
    FB_ALLOC_PAGE_FORMAT *pFbAllocPageFormat
)
{
    NvU32 kind = memmgrChooseKindCompressZ_TU102(pGpu, pMemoryManager, pFbAllocPageFormat);
    
    if (kind == NV_MMU_PTE_KIND_S8_COMPRESSIBLE_DISABLE_PLC)
    {
        return NV_MMU_PTE_KIND_S8_COMPRESSIBLE_DISABLE_PLC;
    }

    if (kind == NV_MMU_PTE_KIND_Z16_COMPRESSIBLE_DISABLE_PLC)
    {
        return NV_MMU_PTE_KIND_Z16_COMPRESSIBLE_DISABLE_PLC;
    }
  
    return NV_MMU_PTE_KIND_GENERIC_MEMORY_COMPRESSIBLE;
}



/*
 * @brief Return an uncompressible kind for the given kind.  
 * GB20X only supports GMK or compressible-GMK. Return GMK if bOverrideToGMK
 * is set or if mods want to bypass uncompression, otherwise call the _TU102 function.
 * 
 * @param[in] NvU32   - PTE kind
 * @param[in] NvBool  - ReleaseReacquire / full-fledge mode
 *
 * @returns NvU32 - Uncompressed kind for the compressed PTE kind type, or it will Assert
 */
NvU32
memmgrGetUncompressedKind_GB20B
(
    OBJGPU        *pGpu,
    MemoryManager *pMemoryManager,
    NvU32          kind,
    NvBool         bReleaseReacquire
)
{
    kind = memmgrGetUncompressedKind_TU102(pGpu, pMemoryManager, kind, bReleaseReacquire);
  
    if (kind == NV_MMU_PTE_KIND_S8)
    {
        return NV_MMU_PTE_KIND_S8;
    }

    if (kind == NV_MMU_PTE_KIND_Z16)
    {
        return NV_MMU_PTE_KIND_Z16;
    }

    if (kind == NV_MMU_PTE_KIND_PITCH)
    {
        return NV_MMU_PTE_KIND_PITCH;
    }

    return NV_MMU_PTE_KIND_GENERIC_MEMORY;
}

NvU32
memmgrGetCompressedKind_GB20B
(
    MemoryManager *pMemoryManager,
    NvU32          kind,
    NvBool         bDisablePlc
)
{
    switch (kind)
    {
        case NV_MMU_PTE_KIND_GENERIC_MEMORY:
        case NV_MMU_PTE_KIND_GENERIC_MEMORY_COMPRESSIBLE:
        case NV_MMU_PTE_KIND_GENERIC_MEMORY_COMPRESSIBLE_DISABLE_PLC:
            return bDisablePlc ? NV_MMU_PTE_KIND_GENERIC_MEMORY_COMPRESSIBLE_DISABLE_PLC
                               : NV_MMU_PTE_KIND_GENERIC_MEMORY_COMPRESSIBLE;
        case NV_MMU_PTE_KIND_S8:
        case NV_MMU_PTE_KIND_S8_COMPRESSIBLE_DISABLE_PLC:
            return NV_MMU_PTE_KIND_S8_COMPRESSIBLE_DISABLE_PLC;
        case NV_MMU_PTE_KIND_Z16:
        case NV_MMU_PTE_KIND_Z16_COMPRESSIBLE_DISABLE_PLC:
            return NV_MMU_PTE_KIND_Z16_COMPRESSIBLE_DISABLE_PLC;
        default:
        {
            NV_PRINTF(LEVEL_ERROR, "Unknown kind 0x%x.\n", kind);
            return NV_MMU_PTE_KIND_INVALID;
        }
    }
}

/*
 * @brief Check if memory is IO-coherent. In some SOCs, the display ISO
 * allocations are non IO-coherent and can't snoop CPU or GPU caches.
 *
 * @param[in] pAllocData - memory allocation data
 *
 * @returns NV_TRUE if memory is IO-coherent
 */
NvBool memmgrIsMemoryIoCoherent_GB20B
(
    OBJGPU                          *pGpu,
    MemoryManager                   *pMemoryManager,
    NV_MEMORY_ALLOCATION_PARAMS     *pAllocData
)
{
    return !FLD_TEST_DRF(OS32, _ATTR2, _ISO, _YES, pAllocData->attr2);
}
