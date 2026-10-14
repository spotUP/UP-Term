/* Build shim (not part of wasabi): LockBitMapTagList (-168) and UnLockBitMap (-174) for the 0.8 grab of a deep
 * screen, and ReadPixelArray, library vector -198 of
 * cybergraphics.library (devkit FD: ReadPixelArray(destRect,destX,destY,destMod,
 * RastPort,SrcX,SrcY,SizeX,SizeY,DestFormat)(a0,d0,d1,d2,a1,d3,d4,d5,d6,d7)).
 * wasabid.c includes this with __NOLIBBASE__ and calls the _base form. */
#ifndef PROTO_CYBERGRAPHICS_SHIM_H
#define PROTO_CYBERGRAPHICS_SHIM_H
#include <exec/types.h>
#include <exec/libraries.h>
#include <graphics/rastport.h>
#include <graphics/gfx.h>
#include <utility/tagitem.h>

static inline APTR __LockBitMapTagList_base(struct Library *base, struct BitMap *bm, struct TagItem *tags)
{
    register struct Library *_base __asm("a6") = base;
    register struct BitMap  *_a0   __asm("a0") = bm;
    register struct TagItem *_a1   __asm("a1") = tags;
    register ULONG           _d0   __asm("d0");
    __asm volatile ("jsr a6@(-168:W)"
                    : "=r"(_d0), "+r"(_a0), "+r"(_a1)
                    : "r"(_base)
                    : "d1", "cc", "memory");
    return (APTR)_d0;
}

static inline void __UnLockBitMap_base(struct Library *base, APTR handle)
{
    register struct Library *_base __asm("a6") = base;
    register APTR            _a0   __asm("a0") = handle;
    __asm volatile ("jsr a6@(-174:W)"
                    : "+r"(_a0)
                    : "r"(_base)
                    : "d0", "d1", "a1", "cc", "memory");
}

static inline ULONG __ReadPixelArray_base(struct Library *base, APTR destRect, UWORD destX, UWORD destY,
                                          UWORD destMod, struct RastPort *rp, UWORD srcX, UWORD srcY,
                                          UWORD sizeX, UWORD sizeY, UBYTE destFormat)
{
    register struct Library *_base __asm("a6") = base;
    register APTR            _a0   __asm("a0") = destRect;
    register ULONG           _d0   __asm("d0") = destX;
    register ULONG           _d1   __asm("d1") = destY;
    register ULONG           _d2   __asm("d2") = destMod;
    register struct RastPort *_a1  __asm("a1") = rp;
    register ULONG           _d3   __asm("d3") = srcX;
    register ULONG           _d4   __asm("d4") = srcY;
    register ULONG           _d5   __asm("d5") = sizeX;
    register ULONG           _d6   __asm("d6") = sizeY;
    register ULONG           _d7   __asm("d7") = destFormat;
    __asm volatile ("jsr a6@(-198:W)"
                    : "+r"(_d0), "+r"(_d1), "+r"(_a0), "+r"(_a1)
                    : "r"(_base), "r"(_d2), "r"(_d3), "r"(_d4), "r"(_d5), "r"(_d6), "r"(_d7)
                    : "cc", "memory");
    return _d0;
}
#endif
