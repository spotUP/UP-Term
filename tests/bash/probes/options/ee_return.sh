set -e; f() { return 3; }; f || echo rc=$?; f; echo no
