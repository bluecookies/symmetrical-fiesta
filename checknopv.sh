# not even nop, just check if it runs

if [ $# -eq 0 ]; then
	for f in Scene/*.ss; do
		echo Testing - $f
		if ! bin/parsess $f >/dev/null 2>/dev/null; then
			echo Found - $f
		fi
	done
else
	for f in Scene/*.ss; do
		echo Testing - $f
		if bin/parsess $f| grep -q $1 ; then
			echo Found - $f
		fi
	done
fi