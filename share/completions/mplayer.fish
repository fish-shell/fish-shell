# Completions for mplayer (Incomplete, there are too many options and
# I am too lazy. Please send in suggested additions)

#
# List of two letter language codes for dvd audio, etc. Very
# incomplete. Please send in additions.
#

set -l mplayer_lang "
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
	se\tSwedish
"

complete -c mplayer -o autoq -d "Dynamically change postprocessing" -x
complete -c mplayer -o autosync -x -d "A/V sync speed"
complete -c mplayer -o framedrop -d "Skip frames to maintain A/V sync"
complete -c mplayer -s h -o help -d "Display help and exit"
complete -c mplayer -o hardframedrop -d "Skip frames to maintain A/V sync"
complete -c mplayer -o loop -x -d "Loop playback" -r -a "0 1 2 3 4 5 6 7 8 9"
complete -c mplayer -o shuffle -d "Play in random order"

complete -c mplayer -s h -o help -d "Display help and exit"
complete -c mplayer -o fs -d "Full screen"
complete -c mplayer -o playlist -d "Set playlist"-r
complete -c mplayer -o alang -d "Audio language" -x -a $mplayer_lang
complete -c mplayer -o audiofile -d "Play audio from file" -r
complete -c mplayer -o cdrom-device -d "Set default CD-ROM drive"
complete -c mplayer -o channels -d "Set number of audio channels" -x -a "2 4 6"
complete -c mplayer -o chapter -d "Set start chapter" -x
complete -c mplayer -o dvd-device -d "Set default DVD-ROM drive"
complete -c mplayer -o dvdangle -d "Set dvd viewing angle" -x -a "1 2 3 4 5 6 7 8"
complete -c mplayer -o forceidx -d "Force rebuilding index"
complete -c mplayer -o fps -d "Override framerate" -x -a "24 25 30"
complete -c mplayer -o idx -d "Build index if unavailable"
complete -c mplayer -o loadidx -d "Load index from file" -r
complete -c mplayer -o ni -d "Force non-interleaved AVI parser"
complete -c mplayer -o saveidx -d "Rebuild index and save to file" -r
complete -c mplayer -o ss -d "Seek to given time position" -x
complete -c mplayer -o tv -d "TV capture mode"
complete -c mplayer -o slang -d "Subtitle language" -x -a $mplayer_lang
complete -c mplayer -o sub -d "Subtitle file" -r
complete -c mplayer -o unicode -d "Handle subtitlefile as unicode"
complete -c mplayer -o utf8 -d "Handle subtitlefile as utf8"

complete -c mplayer -o vo -x -d "Video output" -a "
(
	mplayer -vo help | string match -ar '\t.*\t.*|^ *[a-zA-Z0-9]+  ' | sed -e 's/[\t ]*\([a-zA-Z0-9]*\)[\t ]*\(.*\)/\1'\t'\2/'
)
"

complete -c mplayer -o ao -x -d "Audio output" -a "
(
	mplayer -ao help | string match -ar '\t.*\t|^ *[a-zA-Z0-9]+  ' | sed -e 's/[\t ]*\([a-zA-Z0-9]*\)[\t ]*\(.*\)/\1'\t'\2/'
)
"

complete -c mplayer -o afm -x -d "Audio output" -a "
(
	__fish_append ',' (mplayer -afm help | string match -ar '\t.*\t|^ *[a-zA-Z0-9]+  ' | sed -e 's/[\t ]*\([a-zA-Z0-9]*\)[\t ]*\(.*\)/\1'\t'\2/')
)
"

complete -c mplayer -o vfm -x -d "Video output" -a "
(
	__fish_append ',' (mplayer -vfm help | string match -ar '\t.*\t|^ *[a-zA-Z0-9]+  ' | sed -e 's/[\t ]*\([a-zA-Z0-9]*\)[\t ]*\(.*\)/\1'\t'\2/')
)
"

complete -c mplayer -l help -d "Display help and exit"
complete -c mplayer -l version -d "Display version and exit"
