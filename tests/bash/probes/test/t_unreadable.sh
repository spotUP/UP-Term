echo x > ur; chmod 000 ur; test -r ur; echo $?; test -w ur; echo $?; test -e ur; echo $?; chmod 644 ur
