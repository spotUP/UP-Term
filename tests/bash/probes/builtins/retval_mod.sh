f() { return 300; }; f; echo $?; g() { return 256; }; g; echo $?; (exit 257); echo $?; (exit 1000); echo $?
