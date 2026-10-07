for v in a b c d; do case $v in a) echo A;& b) echo B;;& [ab]) echo AB;; c) echo C;& *) echo any;; esac; done
case x in x) echo 1;;& x) echo 2;;& y) echo no;;& *) echo 3;; esac
case x in y) echo no;;& x) echo yes;; esac
case q in a) echo a;& b) echo b;; *) echo star;; esac
case a in a) ;& b) echo fell;; esac
case a in a) echo 1;; esac; echo rc=$?
case a in a) false;;& *) echo after;; esac
