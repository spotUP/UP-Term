/* host model for tests/test_win_refresh.c: the AmigaOS types amarchy/win_refresh.c uses */
#ifndef WRH_EXEC_TYPES_H
#define WRH_EXEC_TYPES_H
typedef unsigned long ULONG;
typedef char *STRPTR;
struct Task { int unused; };
struct MsgPort { struct Task *mp_SigTask; };
#endif
