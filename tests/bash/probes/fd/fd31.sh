exec 3>&1; ls nothere 2>&3 | cat; exec 3>&-
