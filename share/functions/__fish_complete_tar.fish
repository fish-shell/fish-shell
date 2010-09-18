
function __fish_complete_tar -d "Peek inside of archives and list all files"

	set -l cmd (commandline -poc)
	set -e cmd[1]
	for i in $cmd
		switch $i
			case '-*'
				continue

			case '*.tar.bz' '*.tar.bz2' '*.tbz' '*.tbz2'
				if test -f $i
					set -l file_list (tar -jt <$i)
					printf (_ "%s\tArchived file\n") $file_list
				end
				return

			case '*.tar.gz'  '*.tgz'
				if test -f $i
					set -l file_list (tar -it <$i)
					printf (_ "%s\tArchived file\n") $file_list
				end
				return

			case '*.tar'
				if test -f $i
					set -l file_list (tar -t <$i)
					printf (_ "%s\tArchived file\n") $file_list
				end
				return
		end
	end
end


