unset a; echo ${a:-d} ${a-d2}; a=; echo ${a:-d3}[${a-d4}]; echo ${a:=set}; echo $a
