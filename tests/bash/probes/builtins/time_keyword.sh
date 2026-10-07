TIMEFORMAT='r=%R u=%U s=%S %lR %2R %0R %1lU %3lS %%x%'
{ time true; } 2>&1 | sed 's/[0-9]/N/g'
echo "st=$?"
{ time false; } 2>&1 | sed "s/[0-9]/N/g"; echo "st=$?"
{ time ! false; } 2>/dev/null; echo "st=$?"
{ time (exit 3); } 2>/dev/null; echo "st=$?"
TIMEFORMAT=
time true
TIMEFORMAT='%Q'
{ time true; } 2>/dev/null; echo "st=$?"
unset TIMEFORMAT
{ time -p true; } 2>&1 | sed 's/[0-9]/N/g'
{ time true; } 2>&1 | sed 's/[0-9]/N/g'
{ time; } 2>&1 | sed 's/[0-9]/N/g'
{ time -p -- echo hi | cat; } 2>&1 | sed 's/[0-9]/N/g'
f(){ time return 4; }; f 2>/dev/null; echo "f=$?"
type time select
{ time echo ok >/dev/null; } 2>/dev/null; echo ok2
for i in 1 2; do time -p echo $i; done 2>&1 | sed 's/[0-9]\.[0-9]*/T/g'
