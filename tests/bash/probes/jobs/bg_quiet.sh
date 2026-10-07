# a non-interactive shell does not announce a background job
sleep 0 &
true &
wait
echo done
( sleep 0 & wait )
echo end
