# not even nop, just check if it runs

if [ $# -ne 0 ]; then
	totalLHS=$(grep -o "08|" $1 | wc -l)
	total06=$(grep -o "06|" $1 | wc -l)
	total05=$(grep -o "05|" $1 | wc -l)
	total20=$(grep -o "20|" $1 | wc -l)
	total30=$(grep -o "30|" $1 | wc -l)
	total21=$(grep -o "21|" $1 | wc -l)
	totalRHS=$(($total05 + $total20 + $total30))
	echo $(($totalLHS - $totalRHS))
	echo 08 $(($totalLHS))
	echo 06 $(($total06))
	echo 05 $(($total05))
	echo 20 $(($total20))
	echo 30 $(($total30))

	echo 21 $(($total21))
fi