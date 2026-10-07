[[ a+b =~ a\+b ]] && echo plus; [[ aab =~ a+b ]] && echo plus2; [[ a =~ a? ]] && echo opt; [[ ab =~ a{1}b ]] && echo brace
