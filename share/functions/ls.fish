#
# Make ls use colors if we are on a system that supports this
#

if command ls --version 1>/dev/null 2>/dev/null
	# This is GNU ls
	function ls -d (N_ "List contents of directory")
		command ls --color=auto --indicator-style=classify $argv
	end

	set -l color_document 35
	set -l color_image '01;35'
	set -l color_sound '01;35'
	set -l color_video '01;35'
	set -l color_archive '01;31'
	set -l color_command '01;32'
	set -l color_backup 37

	set -l default no=00 fi=00 'di=01;34' 'ln=01;36' 'pi=40;33' 'so=01;35' 'bd=40;33;01' 'cd=40;33;01' 'or=01;05;37;41' 'mi=01;05;37;41' ex=$color_command 

	for i in .cmd .exe .com .btm .bat .sh .csh .fish
		set default $default "*$i=$color_command"
	end
		
	for i in .tar .tgz .arj .taz .lhz .zip .z .Z .gz .bz2 .bz .tz .rpm .cpio .jar .deb .rar .bin .hqx
		set default $default "*$i=$color_archive"
	end

	for i in .jpg .jpeg .gif .bmp .xbm .xpm .png .tif 
		set default $default "*$i=$color_image"
	end

	for i in .mp3 .au .wav .aiff .ogg .wma 
		set default $default "*$i=$color_sound"
	end

	for i in .avi .mpeg .mpg .divx .mov .qt .wmv .rm
		set default $default "*$i=$color_video"
	end

	for i in .htm .html .rtf .wpd .doc .pdf .ps .xls .swf .txt .tex .sxw .dvi INSTALL README ChangeLog
		set default $default "*$i=$color_document" 
	end
	
	for i in '~' .bak
		set default $default "*$i=$color_backup" 
	end

	set -gx LS_COLORS $default

else
	# BSD, OS X and a few more support colors through the -G switch instead
	if ls / -G 1>/dev/null 2>/dev/null
		function ls -d (N_ "List contents of directory")
			command ls -G $argv
		end
	end
end

