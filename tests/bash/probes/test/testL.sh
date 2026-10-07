ln -s a.txt lnk; [ -L lnk ] && echo link; [ -h lnk ] && echo h; [ -L a.txt ] || echo notlink
