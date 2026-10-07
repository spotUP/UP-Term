mkdir -p q/x/y q/z; touch q/f q/x/g q/x/y/h
cd q
echo */
echo x/*/
echo */nonexist
echo x/*/h
echo */g
echo */*/h
echo /nonexist*/x
shopt -s globstar
echo x/**/h x/**/*
echo **/h
echo **/y/
