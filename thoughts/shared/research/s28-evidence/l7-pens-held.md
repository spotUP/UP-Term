# L7 pens-held rig check (rig 10, AmigaOS 3.2, 68060, build of rebased 5b02d3d8 on 24d72aec)

Host suites: iconpen 2008/0, flat_core 675/0, flat_on_hang OK, desk 1828/0, iconfile 125/0.
Regression: with the `h->screen = NULL; h->depth = 0;` lines removed from ip_holder_release, iconpen fails at tests/test_iconpen.c:155 (want 1, got 0).

Rig (STATUS of amarchy-flat; Amarchy process alive and AMARCHY port answered after every step):
1. boot with flat ON:            pens-held 16  PASS
2. flat OFF (theme pick):        pens-held 0   PASS
3. flat ON again (theme pick):   pens-held 14  FAIL (plan wants 16)
4. fresh start flat ON (later):  pens-held 16

Open: step 3 obtains only 14 of 16 after off->on on the same screen; the first obtain at boot gets 16. Likely 2 pens taken by another
user of the Workbench palette between release and re-obtain (the theme pick re-applies palette/pens), or the re-obtain path
asks pens that the pick itself holds. Not investigated further (budget). Flat ON hang fix stays effective: no hang in 3 steps.
Rig tick stays OPEN. Not pushed.
