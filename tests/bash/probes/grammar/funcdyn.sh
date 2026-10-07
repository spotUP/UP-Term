f() { echo $x; }; g() { local x=inner; f; }; x=outer; g; f
