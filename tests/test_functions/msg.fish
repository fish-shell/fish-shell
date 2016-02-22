#    __ _  ___ ___ _
#   /  ' \(_-</ _ `/
#  /_/_/_/___/\_, /
#            /___/ v0.1.0
# NAME
#     msg - technicolor message printer
#
# SYNOPSIS
#     msg [-sn] [@<fg:bg> | style]<text>[style]
#
# OPTIONS
#     -s
#         Do not separate arguments with spaces.
#     -n
#         Do not output a newline at the end of the message.
#
# STYLES
#     _text_                 Bold
#     __text__               Underline
#     ___text___             Bold and Underline
#     `$variable`            Apply @<styles> to $variables
#     /directory/            Directories
#     [url]                  Links
#     \n                     Line Break
#     \t                     Tab Space
#
#     @<fg:bg> fg=bg=RGB|RRGGBB|name|random
#       RGB value can be any three to six hex digit or color name.
#       e.g, @0fb, @tomato, @random, @error, @ok.
#
# NOTES
#     Escape style separators or options prepending a \ to the
#     string if inside quotes `msg "\-s"` or a \\ backslash in
#     order to escape the backslash itself when outside quotes.
#
# AUTHORS
#      Jorge Bucaran <jbucaran@me.com>
#
# v. 0.1.0
#/

# Global `msg` default color and styles.
set --global msg_color_fg   FFFFFF
set --global msg_color_bg   normal
set --global msg_style_url  00FF00 $msg_color_bg -u
set --global msg_style_dir  FFA500 $msg_color_bg -u
set --global msg_color_err  FF0000
set --global msg_color_ok   00FA9A

function msg -d "Technicolor printer."
  # Default " " whitespace between strings, skip with -s.
  set -l ws " "
  # Default \n newline after message, skip with -n.
  set -l ln \n

  switch (count $argv)
    case 0
      # Nothing to print here.
      return 0
    case \*
      switch $argv[1]
        # Options must appear joined and without spaces: -sn or -ns
        case -s\* -n\*
          # To use options at least a second parameter will be required.
          if [ (count $argv) -gt 1 ]
            # Use -s to not add spaces between words.
            if msg.util.str.has "s" $argv[1]
              set ws ""
            end
            # Use -n to not add a newline at the end of the message.
            if msg.util.str.has "n" $argv[1]
              set ln ""
            end
            # We are done with options, get rid of first item.
            set argv $argv[2..-1]
          else
            return 0
          end
      end
  end

  # Print flow is get next style token, set style, get anything else, print
  # and use the reset. set_color normal makes sure to reset both colors and
  # bold / underline styles.
  set -l reset (set_color normal)(set_color $msg_color_fg)

  # Foreground and background color carried from previous @style token.
  # These variables are set and reset via msg.util.set.color
  set -l fg $msg_color_fg
  set -l bg $msg_color_bg

  for token in $argv
    switch $token
      # Parse style tokens:
      # _txt_, __txt__, ___txt___, @color `$var`, /dir/, [url]
      case ___\*___\* __\*__\* _\*_\* \[\*\]\* \/\*\/\* `\*`\*
        set -l left   _ # Begin of style
        set -l right  _ # End of style
        set -l color $fg $bg -o
        switch $token
          case ___\*___\* __\*__\* _\*_\*
            if msg.util.str.has __ $token
              set color[3] -u
              set left __
              if msg.util.str.has ___ $token
                set color[3] -uo
                set left ___
              end
            end
          case \[\*\]\*
            set color $msg_style_url
            set left  [ ]
            set right ]
          case \/\*\/\*
            set color $msg_style_dir
            set left \/
            set right \/
          case `\*`\*
            set color $fg $bg
            set left  `
            set right `
        end

        # Extract text between left and right separators.
        echo -n (msg.util.set.color $color)(msg.util.str.get $left $token)$reset

        # Extract string after separator from the right.
        echo -n (printf $token | sed "s/^.*\\$right\(.*\)/\1/")$ws

      # Parse @fg:bg style tokens.
      case @\*
        set fg (printf $token | cut -c 2-)  # @fg[:bg] → fg[:bg]
        set bg (printf $fg | cut -d: -f 2)  # fg:bg    → fg|bg
        set fg (printf $fg | cut -d: -f 1)  # fg:bg    → fg

        # Do not let bg=fg have the same color unless the user wants to.
        if [ $bg = $fg ]
          if not msg.util.str.has : $token
            set bg $msg_color_bg
          end
        end

        # Make color string into valid RRGGBB hex format code.
        set fg (msg.util.get.color $fg)
        set bg (msg.util.get.color $bg)

      # Everything else, print tokens, whitespace, etc.
      case \*
        set -l blank $ws
        switch $token
          # Do not print whitespace after the following characters.
          case $argv[-1] \n\* \t\* \r
            set blank ""
        end
        switch $token
        # Escape -s -n options and style separators \\[text] \\/text/
          case \\\[\* \\\/\* \\\_\* \\\-s \\\-n
            set token (printf $token | sed "s/^\\\//")
        end
        echo -en (msg.util.set.color)$token$reset$blank
    end
  end
  echo -en $ln
end
