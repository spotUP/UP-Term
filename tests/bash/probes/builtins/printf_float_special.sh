printf '%f %e %g %F %E %G|%+f|%10f|%010f|%-6f|\n' inf -inf nan INF nan -Inf inf inf inf inf
printf '%f|\n'; echo "rc=$?"
printf '%f|\n' ''; echo "rc=$?"
printf '%f|\n' abc 2>/dev/null; echo "rc=$?"
printf '%f|\n' 3abc 2>/dev/null; echo "rc=$?"
printf '%f|\n' 1e309 2>/dev/null; echo "rc=$?"
printf '%f|\n' 1e-400 2>/dev/null; echo "rc=$?"
printf '%f %f|\n' "'a" '"b'
printf '%d|\n' '' 2>/dev/null; echo "rc=$?"
printf '%f %f\n' 1 2 3 4
printf '%*.*f|%-*.*e|\n' 8 2 3.14159 12 3 31415.9
printf '%lf %Lf %.1Lf\n' 1.5 2.5 3.25
