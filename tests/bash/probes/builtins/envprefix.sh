A=1 env | grep ^A=; echo "[${A-unset}]"; f() { echo "in f $A"; }; A=2 f; echo "[${A-unset}]"
