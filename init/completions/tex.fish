set -l cmds   -c etex -c tex -c elatex -c latex -c pdflatex -c pdfelatex -c pdftex -c pdfetex -c omega
complete $cmds -o help -d (_ "Display help and exit")
complete $cmds -o version -d (_ "Display version and exit")
complete $cmds -x -a "(
	__fish_complete_suffix (commandline -ct) .tex '(La)TeX file'
)"

