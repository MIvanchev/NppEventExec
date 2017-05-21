/*
This file is part of NppEventExec
Copyright (C) 2016 Mihail Ivanchev

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#include "base.h"
#include "mem.h"

void* reallocMem(void *mem, size_t numBytes)
{
    void *res;
#ifdef DEBUG
    DWORD prevSize;
#endif

    if (numBytes > (SIZE_T) -1)
    {
        /* TODO error */
        return NULL;
    }

    if (mem)
    {
#ifdef DEBUG
        prevSize = HeapSize(GetProcessHeap(), 0, mem);
#endif

        res = HeapReAlloc(GetProcessHeap(), 0, mem, numBytes);
    }
    else
    {
#ifdef DEBUG
        prevSize = 0;
#endif

        res = HeapAlloc(GetProcessHeap(), 0, numBytes);
    }

    if (!res)
    {
        /* TODO error */
        return NULL;
    }

#ifdef DEBUG
    allocatedBytes -= prevSize;
    allocatedBytes += numBytes;
#endif

    return res;
}

void* allocMem(size_t numBytes)
{
    return reallocMem(NULL, numBytes);
}

void freeMem(void *mem)
{
    if (!mem)
        return;

#ifdef DEBUG
    allocatedBytes -= HeapSize(GetProcessHeap(), 0, mem);
#endif

    HeapFree(GetProcessHeap(), 0, mem);
}
