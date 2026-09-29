# 256-colour bars: the xterm cube and the grey ramp, through screen
printf '\033(B'   # the ASCII set in G0 again (a mangled test line switched it)
i=16
while [ $i -lt 232 ]; do
    printf '\033[48;5;%dm ' $i
    i=$((i+1))
    if [ $(( (i - 16) % 36 )) -eq 0 ]; then printf '\033[0m\n'; fi
done
i=232
while [ $i -lt 256 ]; do printf '\033[48;5;%dm  ' $i; i=$((i+1)); done
printf '\033[0m\n'
printf '\033[38;5;196mred \033[38;5;46mgreen \033[38;5;21mblue \033[38;2;255;128;0mtruecolour orange\033[0m\n'
