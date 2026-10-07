true | false | true; echo "${PIPESTATUS[0]}${PIPESTATUS[1]}${PIPESTATUS[2]}"; echo ${PIPESTATUS[@]}; false; echo ${PIPESTATUS[0]}
