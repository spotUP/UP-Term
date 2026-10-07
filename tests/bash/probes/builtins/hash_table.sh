main(){
hash; echo "e=$?"
hash -l; echo "e=$?"
ls >/dev/null; ls >/dev/null; cat /dev/null
hash; echo "e=$?"
hash -l
hash -t ls cat; echo "e=$?"
hash -t nosuch; echo "e=$?"
hash nosuch; echo "e=$?"
hash -p /bin/echo myecho; hash -t myecho; hash -l
myecho hi
hash
hash -d ls; hash -t ls; echo "e=$?"
hash -d zzz; echo "e=$?"
hash -r; hash
hash -z; echo "e=$?"
hash ls sort; hash -l
hash -lt ls
hash -p; echo "e=$?"
hash -p /nonexist/x foo; hash -t foo; echo "e=$?"
hash /bin/ls; echo "e=$?"; hash -t /bin/ls; echo "e=$?"
hash -r; type ls >/dev/null; hash
set -h; set +h; ls >/dev/null; hash; echo $-
}
main 2>/dev/null | sed "s|/usr/bin/|P|;s|usr:bin/|P|;s|/bin/|P|;s|bin:|P|;s|/nonexist/x|NX|;s|nonexist:x|NX|"
