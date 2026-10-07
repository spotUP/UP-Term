PATH=/nosuchvol/bin:$PATH; type nosuchcmd; echo rc=$?; test -x /nosuchvol/bin/x; echo rc=$?
