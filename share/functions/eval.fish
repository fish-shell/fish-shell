
function eval -S -d "Evaluate parameters as a command"

	# If we are in an interactive shell, eval should enable full
	# job control since it should behave like the real code was
	# executed.  If we don't do this, commands that expect to be
	# used interactively, like less, wont work using eval.

	set -l mode
	if status --is-interactive-job-control
		set mode interactive
	else
		if status --is-full-job-control
			set mode full
		else
			set mode none
		end
	end
	if status --is-interactive
		status --job-control full
	end

	echo "begin; $argv ;end eval2_inner <&3 3<&-" | . 3<&0
	set -l res $status

	status --job-control $mode
	return $res
end
