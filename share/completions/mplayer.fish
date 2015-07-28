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

complete -c mplayer -o autoq --description "Dynamically change postprocessing" -x
complete -c mplayer -o autosync -x --description "A/V sync speed"
complete -c mplayer -o framedrop --description "Skip frames to maintain A/V sync"
complete -c mplayer -s h -o help --description "Display help and exit"
complete -c mplayer -o hardframedrop --description "Skip frames to maintain A/V sync"
complete -c mplayer -o loop -x --description "Loop playback" -r -a "0 1 2 3 4 5 6 7 8 9"
complete -c mplayer -o shuffle --description "Play in random order"

complete -c mplayer -s h -o help --description "Display help and exit"
complete -c mplayer -o fs --description "Full screen"
complete -c mplayer -o playlist --description "Set playlist"-r
complete -c mplayer -o alang --description "Audio language" -x -a $mplayer_lang
complete -c mplayer -o audiofile --description "Play audio from file" -r
complete -c mplayer -o cdrom-device --description "Set default CD-ROM drive"
complete -c mplayer -o channels --description "Set number of audio channels" -x -a "2 4 6"
complete -c mplayer -o chapter --description "Set start chapter" -x
complete -c mplayer -o dvd-device --description "Set default DVD-ROM drive"
complete -c mplayer -o dvdangle --description "Set dvd viewing angle" -x -a "1 2 3 4 5 6 7 8"
complete -c mplayer -o forceidx --description "Force rebuilding index"
complete -c mplayer -o fps --description "Override framerate" -x -a "24 25 30"
complete -c mplayer -o idx --description "Build index if unavailable"
complete -c mplayer -o loadidx --description "Load index from file" -r
complete -c mplayer -o ni --description "Force non-interleaved AVI parser"
complete -c mplayer -o saveidx --description "Rebuild index and save to file" -r
complete -c mplayer -o ss --description "Seek to given time position" -x
complete -c mplayer -o tv --description "TV capture mode"
complete -c mplayer -o slang --description "Subtitle language" -x -a $mplayer_lang
complete -c mplayer -o sub --description "Subtitle file" -r
complete -c mplayer -o unicode --description "Handle subtitlefile as unicode"
complete -c mplayer -o utf8 --description "Handle subtitlefile as utf8"

complete -c mplayer -o vo -x --description "Video output" -a "
(
	mplayer -vo help| __fish_sgrep \t.\*\t'\|^ *[a-zA-Z0-9][a-zA-Z0-9]*  '|sed -e 's/[\t ]*\([a-zA-Z0-9]*\)[\t ]*\(.*\)/\1'\t'\2/'
)
"

complete -c mplayer -o ao -x --description "Audio output" -a "
(
	mplayer -ao help| __fish_sgrep \t.\*\t'\|^ *[a-zA-Z0-9][a-zA-Z0-9]*  '|sed -e 's/[\t ]*\([a-zA-Z0-9]*\)[\t ]*\(.*\)/\1'\t'\2/'
)
"

complete -c mplayer -o afm -x --description "Audio output" -a "
(
	__fish_append ',' (mplayer -afm help| __fish_sgrep \t.\*\t'\|^ *[a-zA-Z0-9][a-zA-Z0-9]*  '|sed -e 's/[\t ]*\([a-zA-Z0-9]*\)[\t ]*\(.*\)/\1'\t'\2/')
)
"

complete -c mplayer -o vfm -x --description "Video output" -a "
(
	__fish_append ',' (mplayer -vfm help| __fish_sgrep \t.\*\t'\|^ *[a-zA-Z0-9][a-zA-Z0-9]*  '|sed -e 's/[\t ]*\([a-zA-Z0-9]*\)[\t ]*\(.*\)/\1'\t'\2/')
)
"

complete -c mplayer -l help --description "Display help and exit"
complete -c mplayer -l version --description "Display version and exit"

complete -c mplayer -u

set -e mplayer_lang
