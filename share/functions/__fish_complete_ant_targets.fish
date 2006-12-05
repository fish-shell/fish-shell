
function __fish_complete_ant_targets -d "Print list of targets from build.xml"
	if test -f build.xml
		sed -n "s/ *<target name=[\"']\([^\"']*\)[\"'].*/\1/p" < build.xml
	end
end

