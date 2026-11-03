# Version date parse (rig 91, Kickstart 40.68, Workbench 40.42, C:Version from the ROM/WB 3.1 set)

Files in RAM hold only `$VER: tN 1.0 (<date>)` (checked with Type HEX: bytes 24564552 3A20 ...).
`Version Version FULL` fails (rc 20, "bad number": a bare `Version` name is read as a version number argument, not a file); `Version C:Dir FULL` = `dir 37.5 (06/04/91)`, control prints right.

| $VER date | Version FULL prints | Right? |
|---|---|---|
| (3.11.26) | (12/09/62) | NO |
| (03.11.26) | (12/09/62) | NO (same as C:UPTerm) |
| (3.11.2026) | (05/29/21) | NO |
| (03.11.2026) | (05/29/21) | NO |
| (1.4.93) | (04/01/93) | yes (D.M.YY) |
| (24.6.93) | (06/24/93) | yes |
| (03-Nov-26) | (11/03/26), then prints "(03-Nov-26)" raw on a second line | date right |
| C:UPTerm (03.11.26) | (12/09/62) | NO |

Finding: Version parses D.M.YY only for years in 78..99 (1978-1999 AmigaDOS epoch); a 2-digit year of 26 falls before the epoch and wraps to garbage. A 4-digit year is not understood at all. dd-Mmm-yy is the only form that prints 2026 correctly (via DateStamp text parse), but it appends the raw string as a second line.

## Raw output
```
## Version Version FULL
rc 20
bad number
Version failed returncode 20
## Version C:Dir FULL
rc 0
dir 37.5 (06/04/91)
## Version C:Dir
rc 0
dir 37.5
## Version RAM:t1 FULL
rc 0
t1 1.0 (12/09/62)
## Version RAM:t1
rc 0
t1 1.0
## Version RAM:t2 FULL
rc 0
t2 1.0 (12/09/62)
## Version RAM:t2
rc 0
t2 1.0
## Version RAM:t3 FULL
rc 0
t3 1.0 (05/29/21)
## Version RAM:t3
rc 0
t3 1.0
## Version RAM:t4 FULL
rc 0
t4 1.0 (05/29/21)
## Version RAM:t4
rc 0
t4 1.0
## Version RAM:t5 FULL
rc 0
t5 1.0 (04/01/93)
## Version RAM:t5
rc 0
t5 1.0
## Version RAM:t6 FULL
rc 0
t6 1.0 (06/24/93)
## Version RAM:t6
rc 0
t6 1.0
## Version RAM:t7 FULL
rc 0
t7 1.0 (11/03/26)
(03-Nov-26)
## Version RAM:t7
rc 0
t7 1.0
## Version C:UPTerm FULL
rc 0
UPTerm 1.0 (12/09/62)
UP-Term
## Version C:UPTerm
rc 0
UPTerm 1.0
## Type C:UPTerm HEX NUMBER
rc 5
Type can't do both HEX and NUMBER
```
