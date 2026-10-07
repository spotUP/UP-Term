shopt -s nullglob dotglob failglob nocasematch lastpipe nocaseglob
shopt -u nullglob failglob
case A in a) :;; esac
echo *.zz
