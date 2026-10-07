exec 2>&1
echo before
set -v
echo one
if true; then
  echo two
fi
# comment

x=$((1+1))
set +v
echo after
