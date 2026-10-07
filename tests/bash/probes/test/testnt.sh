touch -t 200001010000 old.txt; [ a.txt -nt old.txt ] && echo newer; [ old.txt -ot a.txt ] && echo older; [ a.txt -ef a.txt ] && echo same
