cat <<-EOF
	tab one
		tab two
  spaces kept
	$HOME_X end
	EOF
cat <<-'EOF'
	$x literal
	EOF
if true; then
	cat <<-END
		inside if
	END
fi
cat <<-A; cat <<-B
	first
	A
	second
	B
