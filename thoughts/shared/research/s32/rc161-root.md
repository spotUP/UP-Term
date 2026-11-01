rc161: ixemul programs under vsh (runner RunCommand) die before main; no DOS call after OpenLibrary(ixemul) (snoop). true/false/echo/mkdir all 161, nothing created, stderr empty.
Old lib (buildgcc16, 7 Oct, pre-2525442, 259012 B) also gives 161 under vsh -> 161 not caused by 2525442; commit only exposes a pre-existing signal-death (before: rc 0).
Not found: which path yields p_xstat low7=33. Needs DEBUG_VERSION (KPRINTF) build or wasabi debugger on the vsh runner.
