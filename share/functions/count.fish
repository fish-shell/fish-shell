
function count --description "Count the number of elements of an array"
	set -l lines ''
	set result 1
	for i in $argv
		set lines $lines\n
		set result 0
	end
	echo -n $lines|wc -l
	return $result
end
