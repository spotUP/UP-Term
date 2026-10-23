/* The portable decision of leaf.h: which way a leaf's kind runs. */
#include "leaf.h"

lf_how lf_plan(int kind)
{
    switch (kind) {
    case KA_RUN:
        return LF_WINDOW;
    case KA_REXX:
        return LF_REXX;
    case KA_OPEN:
        return LF_OPEN;
    default:
        return LF_REQUEST;
    }
}
