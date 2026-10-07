x="a  b"; cat <<< "$x"; cat <<< $x; cat <<< 'lit $x'; read -r v <<< "hello world"; echo "[$v]"
tr a-z A-Z <<< "abc"; wc -c <<< ""; cat <<< "multi
line"; while read -r l; do echo "<$l>"; done <<< "one"
y=$(cat <<< "sub"); echo "$y"; cat <<< "$(echo nested)" <<< "second"
