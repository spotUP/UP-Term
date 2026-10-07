echo $LINENO
x=$LINENO
echo $x
for i in 1 2; do
 echo $LINENO
done
cat <<E
l $LINENO
E
echo $LINENO
if true; then
  echo $LINENO
fi
echo a \
 b $LINENO
echo $LINENO
(echo $LINENO)
f() {
  echo in f $LINENO
}
f
echo after $LINENO
case x in
  x) echo case $LINENO ;;
esac
LINENO=5; echo $LINENO
