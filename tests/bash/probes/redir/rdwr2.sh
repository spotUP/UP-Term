echo start > rw.txt; exec 0<>rw.txt; read -r a; echo "[$a]"; cat rw.txt
echo new <> rw2.txt; cat rw2.txt; echo more 1<>rw2.txt; cat rw2.txt
cat <> a.txt
