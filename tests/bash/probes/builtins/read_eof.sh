printf abc | { read x; echo "$? $x"; }; printf '' | { read x; echo "$? [$x]"; }
