ls /dev/null >/dev/null; ls /dev/null >/dev/null
hash -t ls | sed "s|.*bin[:/]||"
hash -r; hash -t ls 2>/dev/null; echo r=$?
