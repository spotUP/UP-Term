printf -v x '%q' 'a b$c'; echo "$x"; printf -v y '%q' 'tab	x'; echo "$y"; printf -v z '%s' ; echo "[$z]"; printf -v w -- '%d-%d' 1 2; echo $w; printf '%q %q\n' '' 'a"b'
