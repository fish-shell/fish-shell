function env
	set -l i 0;
	while echo $argv[(math $i + 1)] | string match -qr -- ".*=.*"
		set i (math $i + 1)
	end
	begin
		# The following would suffice, except "set -l" makes the export
		# last only until the end of the for loop :-(
		#		for j in (seq 1 $i)
		#			set -l kv (echo $argv[$j] | string split -m1 '=')
		#			set -lx $kv[0] $kv[1]
		#			echo set -lx $k[0] $k[1]
		#		end
		# additionally, `eval` cannot be used to `set -x` since that scope
		# expires at the end of the `eval`'d command. Bug?
		# Instead, we have to do this awkward song and dance

		set -l export_cmd ""
		for j in (seq 1 $i)
			set -l kv (echo $argv[$j] | string split -m1 '=')
			set export_cmd $export_cmd(echo set -lx $kv[1] $kv[2] ";")
		end

		set -l k (math $i + 1)

		# if no command follows declarations, run literal `env`
		# this matches the behavior of `env` on Linux
		if echo (count $argv) | string match -q $i
			set export_cmd $export_cmd "command" env
		else if echo $argv[$k] | string match -q "env"
			set export_cmd $export_cmd "command" $argv[$k..-1]
		else
			set export_cmd $export_cmd $argv[$k..-1]
		end

		eval $export_cmd
	end
end

