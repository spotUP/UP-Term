for x in a b c; do case $x in a|b) echo ab;; *) echo other;; esac; done
