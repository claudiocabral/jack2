/*
Copyright (C) 2001 Paul Davis
Copyright (C) 2004-2008 Grame

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "JackError.h"
#include "JackShmMem.h"
#include <stdio.h>

namespace Jack
{

static unsigned int fSegmentNum = 0;
static jack_shm_info_t gInfo;
size_t JackMem::gSize = 0;

JackShmMem::JackShmMem()
{
    fInfo.index = gInfo.index;
    fInfo.attached_at = gInfo.attached_at;
    fInfo.size = gInfo.size;
}
    
void* JackShmMem::operator new(size_t size, void* memory)
{
    jack_log("JackShmMem::new placement size = %ld", size);
    return memory;
}

void* JackShmMem::operator new(size_t size)
{
    jack_shm_info_t info;
    JackShmMem* obj;
    char name[64];

    snprintf(name, sizeof(name), "/jack_shared%d", fSegmentNum++);

    if (jack_shmalloc(name, size, &info)) {
        jack_error("cannot create shared memory segment of size = %d", size, strerror(errno));
        goto error;
    }

    if (jack_attach_shm(&info)) {
        jack_error("cannot attach shared memory segment name = %s err = %s", name, strerror(errno));
        jack_destroy_shm(&info);
        goto error;
    }

    obj = (JackShmMem*)jack_shm_addr(&info);
    // It is unsafe to set object fields directly (may be overwritten during object initialization),
    // so use an intermediate global data
    gInfo.index = info.index;
    gInfo.size = size;
    gInfo.attached_at = info.attached_at;

    jack_log("JackShmMem::new index = %ld attached = %x size = %ld ", info.index, info.attached_at, size);
    return obj;

error:
    jack_error("JackShmMem::new bad alloc", size);
    throw std::bad_alloc();
}

void JackShmMem::operator delete(void* p, size_t size)
{
    jack_shm_info_t info;
    JackShmMem* obj = (JackShmMem*)p;
    info.index = obj->fInfo.index;
    info.attached_at = obj->fInfo.attached_at;

    jack_log("JackShmMem::delete size = %ld index = %ld", size, info.index);

    jack_release_shm(&info);
    jack_destroy_shm(&info);
}

void JackShmMem::operator delete(void* p)
{		
	JackShmMem::operator delete(p, 0);
}

void LockMemoryImp(void* ptr, size_t size)
{
    if (CHECK_MLOCK(ptr, size)) {
        jack_log("Succeeded in locking %u byte memory area", size);
    } else {
        jack_error("Cannot lock down memory area (%s)", strerror(errno));
    }
}

void UnlockMemoryImp(void* ptr, size_t size)
{
    if (CHECK_MUNLOCK(ptr, size)) {
        jack_log("Succeeded in unlocking %u byte memory area", size);
    } else {
        jack_error("Cannot unlock down memory area (%s)", strerror(errno));
    }
}

void LockAllMemory()
{
    if (CHECK_MLOCKALL()) {
        jack_log("Succeeded in locking all memory");
    } else {
        jack_error("Cannot lock all memory (%s)", strerror(errno));
    }
}

void UnlockAllMemory()
{
    if (CHECK_MUNLOCKALL()) {
        jack_log("Succeeded in unlocking all memory");
    } else {
        jack_error("Cannot unlock all memory (%s)", strerror(errno));
    }
}


} // end of namespace

