# INSTALL-AMISSL-STALL evidence

Cause:
- dist/Install.installer P_AMISSL (old lines 62-80) and dist/install.dos libs part (old lines 1138-1155) started a nested `Installer SCRIPT Install-AmiSSL ... MINUSER AVERAGE DEFUSER AVERAGE NOLOG`.
- vendor/amissl-5.27/AmiSSL/Install-AmiSSL asks on every page regardless of MINUSER/DEFUSER: welcome (line 11), AmiSSL: assign/askdir (255-330), CPU askchoice (411-418), certificates askbool (519), Path askbool (599), startup (609), exit (621).
- So a full install stops on the first AmiSSL page.

Fix (commit a4928c9d on worktree-agent-ae25ae66e89295084): new dist/install-amissl.dos copies the same files with no prompts and writes the S:User-Startup AmiSSL block; both callers Execute it. Shipped via Makefile and dist/parts.txt. installer_rig fresh-all checks Version and the AmiSSL:Libs listings.

Rig output: see below.

## Rig proof (s31 main session)

fresh-all on rig 58 (OS 3.2) with a4928c9d: passed 44 of 44, including ok 31-34 (amisslmaster.library v5+, AmiSSL:Libs/amisslmaster.library, AmiSSL:Libs/AmiSSL/amissl_v362.library, AmiSSL:Certs). No stall.
