[[ abc =~ b ]] && echo y1; [[ abc =~ ^b ]] || echo n2; [[ abc =~ c$ ]] && echo y3; [[ abc =~ ^abc$ ]] && echo y4; [[ abc =~ x ]] || echo n5
