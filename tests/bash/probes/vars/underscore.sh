echo a b; echo $_
echo x y z >/dev/null; echo $_
true; echo $_
v=1; echo $_
f() { echo inner; }; f; echo $_
echo "$_"; echo one two three; echo ${_}
