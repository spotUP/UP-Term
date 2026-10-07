exec 3>a.out; echo x >&3; exec 3>&-; exec 3<a.out; read z <&3; echo "$z"; exec 3<&-
