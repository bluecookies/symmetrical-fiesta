# not even nop, just check if it runs

if [ $# -eq 0 ]; then
	for f in Scene/*.ss; do
		printf $f
		if ! bin/decompiless $f >/dev/null 2>/dev/null; then
			printf “- error”
		fi
		echo
	done
else
	for f in Scene/*.ss; do
		if bin/decompiless $f| grep -q $1 ; then
			echo $f
		fi
	done
fi