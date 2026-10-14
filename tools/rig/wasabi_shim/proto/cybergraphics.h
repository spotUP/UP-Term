/* Build shim (not part of wasabi): ReadPixelArray, library vector -198 of
 * cybergraphics.library (devkit FD: ReadPixelArray(destRect,destX,destY,destMod,
 * RastPort,SrcX,SrcY,SizeX,SizeY,DestFormat)(a0,d0,d1,d2,a1,d3,d4,d5,d6,d7)).
 * wasabid.c includes this with __NOLIBBASE__ and calls the _base form. */
#ifndef PROTO_CYBERGRAPHICS_SHIM_H
#define PROTO_CYBERGRAPHICS_SHIM_H
#include <exec/types.h>
#include <exec/libraries.h>
#include <graphics/rastport.h>

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
