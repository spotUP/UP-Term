/* dopus-listerhere.rexx: open a lister at the folder of the active lister (the first lister when none is active; a new
 * lister showing the device list when there is none or it has no folder)
 * No Directory Opus: exit 5, nothing is opened. Gate: the first token of `dopus version` is 5 (the A1200
 * answers `5 91`, v5.104 answers `5 99 104`). Plan 2026-10-25-amarchy-dopus-integration.md, P1. */
OPTIONS RESULTS
IF ~SHOW('P', 'DOPUS.1') THEN EXIT 5
ADDRESS 'DOPUS.1'
'dopus version'
PARSE VAR RESULT major .
IF major ~= 5 THEN EXIT 5
'lister query active'
IF RC ~= 0 THEN 'lister query all'
IF RC ~= 0 THEN DO
  'command DeviceList NEW'
  EXIT RC
END
PARSE VAR RESULT handle .
'lister query' handle 'path'
IF RC ~= 0 THEN EXIT RC
IF RESULT = '' THEN 'command DeviceList NEW'
ELSE 'lister new' RESULT
EXIT RC
