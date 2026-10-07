PWD=/p/w; OLDPWD=/o/ld
echo ~ ~/x ~+ ~+/y ~- ~-/z ~foo ~foo/bar a~ "~" '~' \~ ~"" x~y
echo ${u:-~} "${u:-~}" ${u:-~/a}
a=~ b=~/x c=x:~:~/y d=~+:~- e="~" f=a~:b g=~foo:~
echo "$a|$b|$c|$d|$e|$f|$g"
declare h=~/d:~/e; export i=~; local_test() { local j=~/l:~; echo "$j"; }; local_test
echo "$h|$i"
unset OLDPWD; echo ~- ~+
