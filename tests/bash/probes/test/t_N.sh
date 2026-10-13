# -N: modified since last read = mtime later than atime. The times are set, never left to the clock:
# a write and a read within one second, or a background reader (Spotlight, a virus scanner) moving the
# access time of a fresh file, made the old `echo a > nn; test -N nn` come out either way under load.
# The modification time is in 2099 where it must win: a reader can only move atime to now.
echo a > nn
touch -a -t 202001010000 nn; touch -m -t 209901010000 nn; test -N nn; echo $?
touch -a -t 209901020000 nn; touch -m -t 202001010000 nn; test -N nn; echo $?
touch -a -m -t 202001010000 nn; test -N nn; echo $?
test -N nn_missing; echo $?
