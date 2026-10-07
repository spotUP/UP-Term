mkdir -p gs/a/b/c gs/d gs/.h/x gs/e
cd gs
touch t.c a/a1.c a/b/b1.c a/b/c/c1.c d/d1.h e/e1.c .h/x/hid.c a/.dot.c
ln -s a lnk
ln -s ../a e/up
echo "off: **/*.c"; echo **/*.c
shopt -s globstar
echo "1:" **/*.c
echo "2:" **
echo "3:" **/
echo "4:" a/**
echo "5:" a/**/
echo "6:" a/**/*.c
echo "7:" **/c/*
echo "8:" **/b
echo "9:" **/*1.*
echo "10:" ./**/*.c
echo "11:" a/**/b/**/*.c
echo "12:" **/nomatch
echo "13:" a**/*.c
echo "14:" **x
echo "15:" "**"/*.c
shopt -s dotglob
echo "16:" **/*.c
shopt -u dotglob
shopt -s nullglob
echo "17:" **/zz
shopt -u nullglob
echo "18:" lnk/**/*.c
echo "19:" **/up
echo "20:" e/**/*.c
