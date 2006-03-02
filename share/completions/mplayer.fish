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

complete -c mplayer -o autoq -d (N_ "Dynamically change postprocessing") -x
complete -c mplayer -o autosync -x -d (N_ "A/V sync speed")
complete -c mplayer -o framedrop -d (N_ "Skip frames to maintain A/V sync")
complete -c mplayer -s h -o help -d (N_ "Display help and exit")
complete -c mplayer -o hardframedrop -d (N_ "Skip frames to maintain A/V sync")
complete -c mplayer -o loop -x -d (N_ "Loop playback") -r -a "0 1 2 3 4 5 6 7 8 9"
complete -c mplayer -o shuffle -d (N_ "Play in random order")

complete -c mplayer -s h -o help -d (N_ "Display help and exit")
complete -c mplayer -o fs -d (N_ "Full screen")
complete -c mplayer -o playlist -d (N_ "Set playlist")-r
complete -c mplayer -o alang -d (N_ "Audio language") -x -a $mplayer_lang
complete -c mplayer -o audiofile -d (N_ "Play audio from file") -r
complete -c mplayer -o cdrom-device -d (N_ "Set default CD-ROM drive")
complete -c mplayer -o channels -d (N_ "Set number of audio channels") -x -a "2 4 6"
complete -c mplayer -o chapter -d (N_ "Set start chapter") -x
complete -c mplayer -o dvd-device -d (N_ "Set default DVD-ROM drive")
complete -c mplayer -o dvdangle -d (N_ "Set dvd viewing angle") -x -a "1 2 3 4 5 6 7 8"
complete -c mplayer -o forceidx -d (N_ "Force rebuilding index")
complete -c mplayer -o fps -d (N_ "Override framerate") -x -a "24 25 30"
complete -c mplayer -o idx -d (N_ "Build index if unavailable")
complete -c mplayer -o loadidx -d (N_ "Load index from file") -r
complete -c mplayer -o ni -d (N_ "Force non-interleaved AVI parser")
complete -c mplayer -o saveidx -d (N_ "Rebuild index and save to file") -r
complete -c mplayer -o ss -d (N_ "Seek to given time position") -x
complete -c mplayer -o tv -d (N_ "TV capture mode")
complete -c mplayer -o slang -d (N_ "Subtitle language") -x -a $mplayer_lang
complete -c mplayer -o sub -d (N_ "Subtitle file") -r
complete -c mplayer -o unicode -d (N_ "Handle subtitlefile as unicode")
complete -c mplayer -o utf8 -d (N_ "Handle subtitlefile as utf8")

complete -c mplayer -o vo -x -d (N_ "Video output") -a "
(
	mplayer -vo help|grep \t.\*\t'\|^ *[a-zA-Z0-9][a-zA-Z0-9]*  '|sed -e 's/[\t ]*\([a-zA-Z0-9]*\)[\t ]*\(.*\)/\1'\t'\2/'
)
"

complete -c mplayer -o ao -x -d (N_ "Audio output") -a "
(
	mplayer -ao help|grep \t.\*\t'\|^ *[a-zA-Z0-9][a-zA-Z0-9]*  '|sed -e 's/[\t ]*\([a-zA-Z0-9]*\)[\t ]*\(.*\)/\1'\t'\2/'
)
"

complete -c mplayer -o afm -x -d (N_ "Audio output") -a "
(
	__fish_append ',' (mplayer -afm help|grep \t.\*\t'\|^ *[a-zA-Z0-9][a-zA-Z0-9]*  '|sed -e 's/[\t ]*\([a-zA-Z0-9]*\)[\t ]*\(.*\)/\1'\t'\2/')
)
"

complete -c mplayer -o vfm -x -d (N_ "Video output") -a "
(
	__fish_append ',' (mplayer -vfm help|grep \t.\*\t'\|^ *[a-zA-Z0-9][a-zA-Z0-9]*  '|sed -e 's/[\t ]*\([a-zA-Z0-9]*\)[\t ]*\(.*\)/\1'\t'\2/')
)
"

complete -c mplayer -l help -d (N_ "Display help and exit")
complete -c mplayer -l version -d (N_ "Display version and exit")

set -e mplayer_lang
