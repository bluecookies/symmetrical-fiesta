# not even nop, just check if it runs

if [ $# -eq 0 ]; then
	for f in Scene/*.ss; do
		if ! bin/parsess $f >/dev/null 2>/dev/null; then
			echo $f
		fi
	done
else
	for f in Scene/*.ss; do
		if bin/parsess $f| grep -q "NOP" ; then
			echo $f
		fi
	done
fi