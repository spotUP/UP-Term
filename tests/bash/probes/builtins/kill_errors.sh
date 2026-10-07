kill -0 99999999 2>/dev/null; echo $?; kill 2>/dev/null; echo $?; kill -Z 123 2>/dev/null; echo $?
