trap "echo ERR" ERR
set -E
echo [$(false)]
echo --
x=$(false)
echo --
echo $(false) $(true)
