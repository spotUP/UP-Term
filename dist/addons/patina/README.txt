up-patina: Patina, the UP-Term Workbench desktop layer, for AmigaOS 3

Contents: Patina (the Left Amiga menu and key actions), patina-menu and patina-gui (the drawn menu), patina-index (the
app index), patina-flat (the flat look of windows and icons), patina-icons, amnotify (notifier), AmTile, the colour
themes, the Phosphor and Lucide icon sets, bar.conf and the WBStartup icons.

Needs up-term.lha 1.2 or newer installed first. Install copies the programs to UP-Term:bin, the themes and icons to
UP-Term:Patina, the WBStartup icons to SYS:WBStartup and adds a Patina-Flat block to S:Startup-Sequence (guarded: it
runs only while UP-Term:bin/patina-flat exists). Without this package the base boots and works as before.

Uninstall (beside Install) quits Patina, removes the programs, the block and the icons. Your settings
(ENVARC:Patina/bar.conf and your own themes) stay.
