set -o errexit; case $- in *e*) echo e;; esac; set +o errexit; case $- in *e*) echo e;; *) echo no-e;; esac
