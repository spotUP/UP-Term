/* dopus-ftp.rexx: open the FTP connect requester (module command; HOST, PORT, USER, PASSWORD, DIR, SITE, GUI, NOSCAN and RECON only: v5.91 has no TLS options)
 * No Directory Opus: exit 5, nothing is opened. Gate: the first token of `dopus version` is 5 (the A1200
 * answers `5 91`, v5.104 answers `5 99 104`). Plan 2026-10-25-amarchy-dopus-integration.md, P1. */
OPTIONS RESULTS
IF ~SHOW('P', 'DOPUS.1') THEN EXIT 5
ADDRESS 'DOPUS.1'
'dopus version'
PARSE VAR RESULT major .
IF major ~= 5 THEN EXIT 5
'command FTPConnect'
EXIT RC
