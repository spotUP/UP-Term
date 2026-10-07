set -e; case $- in *e*) echo errexit;; esac; set +e; case $- in *e*) echo errexit;; *) echo plain;; esac
