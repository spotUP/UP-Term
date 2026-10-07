mkdir -p gn; cd gn; touch a.txt
echo [*.zzz]; shopt -s nullglob; echo [*.zzz]; set -- *.zzz; echo $#; shopt -u nullglob; echo [*.zzz]
