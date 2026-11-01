# kill ESRCH A/B (rig 9)
Cause: NOT ixemul/ports. The pid tested was sh's `$!`, which vsh sets to a job* (shell/sh_exec.c:8452 last_bg=job), not a Task address.
vsh builtin kill maps job* -> process (os_signal vsh.c:1298); ixemul kill() -> pfind() needs a real Task address.
Same pid ($!=269532444): C:ixkill -0 -> "ixkill: 269532444: no such ixemul process" rc1; kill-new -0 -> "No such process" rc1; kill-new -INT same; ixkill -INT same; builtin kill -0 rc0, builtin -INT ends sleep.
Real task pid (ixemul prog RAM:kp2 writes getpid() = 272086048, run via sh `&`):
 RAM:kp (probe, vtcon flags -mcrt=ixemul): kill(272086048,0)=0 errno=0
 C:ixkill -0 272086048 -> rc 0
 kill-new -0 272086048 -> rc 0
 kill-new -INT 272086048 -> rig 9 stopped answering (hang/reboot by watchdog; fs-uae pid changed); ixkill -INT retest not completed (rig reboot slow, host load 19).
nm: kill.o and ixkill both `_kill` = stub `jmp a0@(-558)` into ixemul LVO; identical. No gnulib replacement.

## s21 INT hang check
Target RAM:kp (writes RAM:kp.pid, sleep 300). Host: no vitest/build; other rig (rig4 fs-uae) 113% cpu.
Launched `Run >NIL: RAM:kp`, pid 270444792: ixkill -INT rc0 no text, target ended, wasabi up after 10s.
Same launch: kill-new -INT rc0 no text, target ended, wasabi up. kill-new -TERM: rc0, ended, wasabi up.
Launched via `sh -c "RAM:kp &"`, pid 272281496: kill-new -INT rc0, ixkill -INT rc0, kill-new -TERM rc0: target survived all (signals ignored in sh bg job), wasabi up each time. NO hang in any case.
Earlier hang not reproduced (earlier: host load 19, rig busy); unexplained.

## s21 vsh bang rig check
Rig 9 (060), vsh rebuilt, start refreshed vsh 240068 bytes (no drift WARN). Scripts run as C:vsh RAM:t1.sh (t1/t2 in scratchpad; outputs o1.txt o2.txt).
RAM:kp & ; echo $! -> 270461856; kp getpid (RAM:kp.pid) 270461856: EQUAL
C:ixkill -0 $! rc0; builtin kill -0 $! rc0; jobs -l: "[1]+ 270461856 Running RAM:kp &"
C:ixkill -INT $! -> "Interrupt - RAM:kp" rc0, target gone (kill -0 -> "No such process" rc1)
builtin kill $! -> "Terminated - RAM:kp" rc0, target gone; wait $! rc0
Verdict PASS
