touch -t 200001010000 old.txt; [ a.txt -nt old.txt ] && echo newer
