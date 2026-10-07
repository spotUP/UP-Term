mkdir -p gc; cd gc; touch a.txt b.TXT
shopt -u nocaseglob; echo *.txt
shopt -s nocaseglob; echo *.txt
shopt nocaseglob
