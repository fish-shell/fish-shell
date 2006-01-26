# Completions for mplayer (Incomplete, there are too many options and
# I am too lazy. Please send in suggested additions)

#
# List of two letter language codes for dvd audio, etc. Very
# incomplete. Please send in additions.
#

set mplayer_lang "
	de\tGerman 
	dk\tDanish 
	en\tEnglish 
	es\tSpanish
	fi\tFinnish 
	fr\tFrench 
	gr\tGreek
	hu\tHungarian
	it\tItalian
	jp\tJapanese 
	no\tNorwegian 
	hu\tHungarian 
	pl\tPolish 
	pt\tPortugese
	se\Swedish
"

complete -c mplayer -o autoq -d (_ "Dynamically change postprocessing") -x
complete -c mplayer -o autosync -x -d (_ "A/V sync speed")
complete -c mplayer -o framedrop -d (_ "Skip frames to maintain A/V sync")
complete -c mplayer -s h -o help -d (_ "Display help and exit")
complete -c mplayer -o hardframedrop -d (_ "Skip frames to maintain A/V sync")
complete -c mplayer -o loop -x -d (_ "Loop playback") -r -a "0 1 2 3 4 5 6 7 8 9"
complete -c mplayer -o shuffle -d (_ "Play in random order")

complete -c mplayer -s h -o help -d (_ "Display help and exit")
complete -c mplayer -o fs -d (_ "Full screen")
complete -c mplayer -o playlist -d (_ "Set playlist")-r
complete -c mplayer -o alang -d (_ "Audio language") -x -a $mplayer_lang
complete -c mplayer -o audiofile -d (_ "Play audio from file") -r
complete -c mplayer -o cdrom-device -d (_ "Set default CD-ROM drive")
complete -c mplayer -o channels -d (_ "Set number of audio channels") -x -a "2 4 6"
complete -c mplayer -o chapter -d (_ "Set start chapter") -x
complete -c mplayer -o dvd-device -d (_ "Set default DVD-ROM drive")
complete -c mplayer -o dvdangle -d (_ "Set dvd viewing angle") -x -a "1 2 3 4 5 6 7 8"
complete -c mplayer -o forceidx -d (_ "Force rebuilding index")
complete -c mplayer -o fps -d (_ "Override framerate") -x -a "24 25 30"
complete -c mplayer -o idx -d (_ "Build index if unavailable")
complete -c mplayer -o loadidx -d (_ "Load index from file") -r
complete -c mplayer -o ni -d (_ "Force non-interleaved AVI parser")
complete -c mplayer -o saveidx -d (_ "Rebuild index and save to file") -r
complete -c mplayer -o ss -d (_ "Seek to given time position") -x
complete -c mplayer -o tv -d (_ "TV capture mode")
complete -c mplayer -o slang -d (_ "Subtitle language") -x -a $mplayer_lang
complete -c mplayer -o sub -d (_ "Subtitle file") -r
complete -c mplayer -o unicode -d (_ "Handle subtitlefile as unicode")
complete -c mplayer -o utf8 -d (_ "Handle subtitlefile as utf8")

complete -c mplayer -o vo -x -d (_ "Video output") -a "
(
	mplayer -vo help|grep \t.\*\t'\|^ *[a-zA-Z0-9][a-zA-Z0-9]*  '|sed -e 's/[\t ]*\([a-zA-Z0-9]*\)[\t ]*\(.*\)/\1\t\2/'
)
"

complete -c mplayer -o ao -x -d (_ "Audio output") -a "
(
	mplayer -ao help|grep \t.\*\t'\|^ *[a-zA-Z0-9][a-zA-Z0-9]*  '|sed -e 's/[\t ]*\([a-zA-Z0-9]*\)[\t ]*\(.*\)/\1\t\2/'
)
"

complete -c mplayer -o afm -x -d (_ "Audio output") -a "
(
	__fish_append ',' (mplayer -afm help|grep \t.\*\t'\|^ *[a-zA-Z0-9][a-zA-Z0-9]*  '|sed -e 's/[\t ]*\([a-zA-Z0-9]*\)[\t ]*\(.*\)/\1\t\2/')
)
"

complete -c mplayer -o vfm -x -d (_ "Video output") -a "
(
	__fish_append ',' (mplayer -vfm help|grep \t.\*\t'\|^ *[a-zA-Z0-9][a-zA-Z0-9]*  '|sed -e 's/[\t ]*\([a-zA-Z0-9]*\)[\t ]*\(.*\)/\1\t\2/')
)
"

complete -c mplayer -l help -d (_ "Display help and exit")
complete -c mplayer -l version -d (_ "Display version and exit")

set -e mplayer_lang
