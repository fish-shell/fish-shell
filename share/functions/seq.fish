# If seq is not installed, then define a function that invokes __fish_fallback_seq
# We can't call type here because that also calls seq

if not command -s seq >/dev/null
	# No seq command
	function seq --description "Print sequences of numbers"
		__fish_fallback_seq $argv
	end

	function __fish_fallback_seq --description "Fallback implementation of the seq command"
	
		set -l from 1
		set -l step 1
		set -l to 1
		
		switch (count $argv)
			case 1
				set to $argv[1]
		
			case 2
				set from $argv[1]
				set to $argv[2]
		
			case 3
				set from $argv[1]
				set step $argv[2]
				set to $argv[3]
		
			case '*'
				printf (_ "%s: Expected 1, 2 or 3 arguments, got %d\n") seq (count $argv)
				return 1
		
		end
		
		for i in $from $step $to
			if not echo $i | grep -E '^-?[0-9]*([0-9]*|\.[0-9]+)$' >/dev/null
				printf (_ "%s: '%s' is not a number\n") seq $i
				return 1
			end
		end
		
		if [ $step -ge 0 ]
			echo "for( i=$from; i<=$to ; i+=$step ) i;" | bc
		else
			echo "for( i=$from; i>=$to ; i+=$step ) i;" | bc
		end
	end
end
