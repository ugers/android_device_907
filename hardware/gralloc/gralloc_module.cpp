/*
 * Copyright (C) 2010 ARM Limited. All rights reserved.
 *
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <pthread.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include "gralloc_priv.h"
#include "alloc_device.h"
#include "framebuffer_device.h"

#if GRALLOC_ARM_UMP_MODULE
#include <ump/ump_ref_drv.h>
static int s_ump_is_open = 0;
#endif

#if GRALLOC_ARM_DMA_BUF_MODULE
#include <linux/ion.h>
#include <ion/ion.h>
#include <sys/mman.h>
#endif

static pthread_mutex_t s_map_lock = PTHREAD_MUTEX_INITIALIZER;

static int gralloc_device_open(const hw_module_t* module, const char* name, hw_device_t** device)
{
	int status = -EINVAL;

	if (!strcmp(name, GRALLOC_HARDWARE_GPU0))
	{
		status = alloc_device_open(module, name, device);
	}
	else if (!strcmp(name, GRALLOC_HARDWARE_FB0))
	{
		status = framebuffer_device_open(module, name, device);
	}

	return status;
}

static int gralloc_register_buffer(gralloc_module_t const* module, buffer_handle_t handle)
{
	if (private_handle_t::validate(handle) < 0)
	{
		AERR("Registering invalid buffer 0x%x, returning error", (int)handle);
		return -EINVAL;
	}

	// if this handle was created in this process, then we keep it as is.
	private_handle_t* hnd = (private_handle_t*)handle;
	//if (hnd->pid == getpid())
	//{
	//	AERR("Unable to register handle 0x%x coming from different process: %d", (unsigned int)hnd, hnd->pid );
	//	return 0;
	//}

	int retval = -EINVAL;

	pthread_mutex_lock(&s_map_lock);

#if GRALLOC_ARM_UMP_MODULE
	if (!s_ump_is_open)
	{
		ump_result res = ump_open(); // TODO: Fix a ump_close() somewhere???
		if (res != UMP_OK)
		{
			pthread_mutex_unlock(&s_map_lock);
			AERR("Failed to open UMP library with res=%d", res);
			return retval;
		}
		s_ump_is_open = 1;
	}
#endif

	if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER) 
	{
		AERR( "Can't register buffer 0x%x as it is a framebuffer", (unsigned int)handle );
	}
	else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP)
	{
#if GRALLOC_ARM_UMP_MODULE
		hnd->ump_mem_handle = (int)ump_handle_create_from_secure_id(hnd->ump_id);
		if (UMP_INVALID_MEMORY_HANDLE != (ump_handle)hnd->ump_mem_handle)
		{
			hnd->base = (int)ump_mapped_pointer_get((ump_handle)hnd->ump_mem_handle);
			if (0 != hnd->base)
			{
				hnd->lockState = private_handle_t::LOCK_STATE_MAPPED;
				hnd->writeOwner = 0;
				hnd->lockState = 0;

				pthread_mutex_unlock(&s_map_lock);
				return 0;
			}
			else
			{
				AERR("Failed to map UMP handle 0x%x", hnd->ump_mem_handle );
			}

			ump_reference_release((ump_handle)hnd->ump_mem_handle);
		}
		else
		{
			AERR("Failed to create UMP handle 0x%x", hnd->ump_mem_handle );
		}
#else
		AERR("Gralloc does not support UMP. Unable to register UMP memory for handle 0x%x", (unsigned int)hnd );
#endif
	}
	else if ( hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION )
	{
#if GRALLOC_ARM_DMA_BUF_MODULE
		int ret;
		unsigned char *mappedAddress;
		size_t size = hnd->size;
	    /* a second user process must obtain a client handle first via ion_open before it can obtain the shared ion buffer*/	
		hnd->ion_client = ion_open();
		
		if ( hnd->ion_client < 0 ) 
		{
			AERR( "Could not open ion device for handle: 0x%x", (unsigned int)hnd );
			retval = -errno;
			goto cleanup;
		}
		
		mappedAddress = (unsigned char*)mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, hnd->share_fd, 0 );
		
		if ( MAP_FAILED == mappedAddress )
		{
			AERR( "mmap( share_fd:%d ) failed with %s",  hnd->share_fd, strerror( errno ) );
			retval = -errno;
			goto cleanup;
		}

		hnd->base = intptr_t(mappedAddress) + hnd->offset;
		pthread_mutex_unlock( &s_map_lock );
		return 0;
#endif
	}
	else
	{
		AERR("registering non-UMP buffer not supported. flags = %d", hnd->flags );
	}

cleanup:
	pthread_mutex_unlock(&s_map_lock);
	return retval;
}

static int gralloc_unregister_buffer(gralloc_module_t const* module, buffer_handle_t handle)
{
	if (private_handle_t::validate(handle) < 0)
	{
		AERR("unregistering invalid buffer 0x%x, returning error", (int)handle);
		return -EINVAL;
	}

	private_handle_t* hnd = (private_handle_t*)handle;

	AERR_IF(hnd->lockState & private_handle_t::LOCK_STATE_READ_MASK, "[unregister] handle %p still locked (state=%08x)", hnd, hnd->lockState);

	if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER)
	{
		AERR( "Can't unregister buffer 0x%x as it is a framebuffer", (unsigned int)handle );
	}
	else if (hnd->pid != getpid()) // never unmap buffers that were not created in this process
	{
		pthread_mutex_lock(&s_map_lock);

		if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP)
		{
#if GRALLOC_ARM_UMP_MODULE
			ump_mapped_pointer_release((ump_handle)hnd->ump_mem_handle);
			hnd->base = 0;
			ump_reference_release((ump_handle)hnd->ump_mem_handle);
			hnd->ump_mem_handle = (int)UMP_INVALID_MEMORY_HANDLE;
#else
			AERR( "Can't unregister UMP buffer for handle 0x%x. Not supported", (unsigned int)handle );
#endif
		}
		else if ( hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION )
		{
#if GRALLOC_ARM_DMA_BUF_MODULE
			void* base = (void*)hnd->base;
			size_t size = hnd->size;

			if ( munmap( base,size ) < 0 )
			{
				AERR("Could not munmap base:0x%x size:%d '%s'", (unsigned int)base, size, strerror(errno));
			}

#else
			AERR( "Can't unregister DMA_BUF buffer for hnd %p. Not supported", hnd );
#endif

		}
		else
		{
			AERR("Unregistering unknown buffer is not supported. Flags = %d", hnd->flags );
		}

		hnd->base = 0;
		hnd->lockState  = 0;
		hnd->writeOwner = 0;

		pthread_mutex_unlock(&s_map_lock);
	}
	else
	{
		AERR( "Trying to unregister buffer 0x%x from process %d that was not created in current process: %d", (unsigned int)hnd, hnd->pid, getpid());
	}

	return 0;
}

static int gralloc_lock(gralloc_module_t const* module, buffer_handle_t handle, int usage, int l, int t, int w, int h, void** vaddr)
{
	if (private_handle_t::validate(handle) < 0)
	{
		AERR("Locking invalid buffer 0x%x, returning error", (int)handle );
		return -EINVAL;
	}

	private_handle_t* hnd = (private_handle_t*)handle;
	if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP || hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION)
	{
		hnd->writeOwner = usage & GRALLOC_USAGE_SW_WRITE_MASK;
	}
	if (usage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK))
	{
		*vaddr = (void*)hnd->base;
	}
	return 0;
}

static int gralloc_unlock(gralloc_module_t const* module, buffer_handle_t handle)
{
	if (private_handle_t::validate(handle) < 0)
	{
		AERR( "Unlocking invalid buffer 0x%x, returning error", (int)handle );
		return -EINVAL;
	}

	private_handle_t* hnd = (private_handle_t*)handle;
	int32_t current_value;
	int32_t new_value;
	int retry;

	if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP && hnd->writeOwner)
	{
#if GRALLOC_ARM_UMP_MODULE
		ump_cpu_msync_now((ump_handle)hnd->ump_mem_handle, UMP_MSYNC_CLEAN_AND_INVALIDATE, (void*)hnd->base, hnd->size);
#else
		AERR( "Buffer 0x%x is UMP type but it is not supported", (unsigned int)hnd );
#endif
	}
	else if ( hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION && hnd->writeOwner)
	{
#if GRALLOC_ARM_DMA_BUF_MODULE
		ion_sync_fd(hnd->ion_client, hnd->share_fd);
#endif
	}
	return 0;
}

// There is one global instance of the module

static struct hw_module_methods_t gralloc_module_methods =
{
	.open = gralloc_device_open
};

private_module_t::private_module_t()
{
#define INIT_ZERO(obj) (memset(&(obj),0,sizeof((obj))))

	base.common.tag = HARDWARE_MODULE_TAG;
	base.common.version_major = 1;
	base.common.version_minor = 0;
	base.common.id = GRALLOC_HARDWARE_MODULE_ID;
	base.common.name = "Graphics Memory Allocator Module";
	base.common.author = "ARM Ltd.";
	base.common.methods = &gralloc_module_methods;
	base.common.dso = NULL;
	INIT_ZERO(base.common.reserved);

	base.registerBuffer = gralloc_register_buffer;
	base.unregisterBuffer = gralloc_unregister_buffer;
	base.lock = gralloc_lock;
	base.unlock = gralloc_unlock;
	base.perform = NULL;
	INIT_ZERO(base.reserved_proc);

	framebuffer = NULL;
	flags = 0;
	numBuffers = 0;
	bufferMask = 0;
	pthread_mutex_init(&(lock), NULL);
	currentBuffer = NULL;
	INIT_ZERO(info);
	INIT_ZERO(finfo);
	xdpi = 0.0f; 
	ydpi = 0.0f; 
	fps = 0.0f; 

#undef INIT_ZERO
};

/*
 * HAL_MODULE_INFO_SYM will be initialized using the default constructor
 * implemented above
 */ 
struct private_module_t HAL_MODULE_INFO_SYM;

