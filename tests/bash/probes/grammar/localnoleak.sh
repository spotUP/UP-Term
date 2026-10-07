f() { local v=in; }; v=out; f; echo $v
f2() { local w=1; }; f2; echo "[${w-unset}]"
