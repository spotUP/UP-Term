mkdir -p gf; cd gf; touch a.txt
shopt -s failglob
echo *.zzz 2>/dev/null; echo "same line"
echo "next line $?"
echo *.txt
