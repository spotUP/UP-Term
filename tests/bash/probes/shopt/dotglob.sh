mkdir -p gd2; cd gd2; touch a.txt .h.txt
echo *; shopt -s dotglob; echo *; echo .*txt; shopt -u dotglob; echo *
