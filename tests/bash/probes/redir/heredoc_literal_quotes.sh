# an unquoted here-document: quotes are themselves, \ quotes only $ ` \ and a newline (configure --help)
x=1
cat <<_ACEOF
'configure' configures "less" 'it'"s" $x '$x' "$x"
a\
b \$x \a \" \' \\ `echo bq`
${x:+"set"} ${y:-'none'}
_ACEOF
cat <<-E
	tab 'q' "d"
	E
