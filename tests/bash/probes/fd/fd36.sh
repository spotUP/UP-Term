cat <&3 3<<<"here string"; exec 4<<<"w4"; read -u 4 q; echo "$q"; exec 4<&-
