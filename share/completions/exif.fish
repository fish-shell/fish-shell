function __get_target_files
  set param_args (string split ', ' -- (string trim (string replace -fr "=\w+.*" '' -- (exif --help))))
  set tokens (commandline -po)[2..]
  set current_token 1
  while test "$current_token" -le (count $tokens)
    if contains -- $tokens[$current_token] $param_args
      set -e tokens[$current_token]
      set -e tokens[$current_token]
    else if string match -- '-*' $tokens[$current_token]
      set -e tokens[$current_token]
    else
      set current_token (math $current_token + 1)
    end
  end
  echo $tokens
end

function __get_target_files_tags
  string trim (string split -m1 ' ' (string replace -r '(\s+[-*]){5}' '' (string replace -fr '^(?!EXIF)' '' (exif --list-tags (__get_target_files)))))
end

function __get_potential_targets
  set matching_files (commandline -t)*
  for file in $matching_files
    if test -d "$file"
      echo "$file/"
    else if exif "$file" &> /dev/null
      echo "$file"
    end
  end
end

function __token_begins_with_arg
  not string match -- '-*' (commandline -t)
end

complete -c exif -f -a "(__get_potential_targets)" -n "__token_begins_with_arg"

for line in (exif --help)
  if string match -r '^\s+-[\w\|\?],\s--(\w|-|=)+\s+.*$' "$line"
    set parts (string split ', ' -- (string trim "$line"))
    set short (string replace -r '^-' '' -- "$parts[1]")
    set parts (string split -nm 1 ' ' -- "$parts[2]")
  else if string match -r '^\s+--(\w|-|=)+\s+.*$' "$line"
    set parts (string split -nm 1 ' ' -- (string trim "$line"))
  else
    continue
  end

  set long (string replace -r '^--' '' -- "$parts[1]")
  set description (string trim "$parts[2]")

  if string match -r '[\w-]+=[\w]' "$long"
    set sub_parts (string split -nm 1 '=' "$long")
    set long "$sub_parts[1]"
    switch (string lower "$sub_parts[2]")
      case "tag"
        if test -n "$short"
          complete -c exif -s "$short" -l "$long" -d "$description" -x -a "(__get_target_files_tags)"
        else
          complete -c exif -l "$long" -d "$description" -x -a "(__get_target_files_tags)"
        end
      case "ifd"
        if test -n "$short"
          complete -c exif -s "$short" -l "$long" -d "$description" -x -a "0 1 EXIF GPS Interoperability"
        else
          complete -c exif -l "$long" -d "$description" -x -a "0 1 EXIF GPS Interoperability"
        end
      case "file"
        if test -n "$short"
          complete -c exif -s "$short" -l "$long" -d "$description" -F
        else
          complete -c exif -l "$long" -d "$description" -F
        end
      case "string"
        if test -n "$short"
          complete -c exif -s "$short" -l "$long" -d "$description" -x
        else
          complete -c exif -l "$long" -d "$description" -x
        end
    end
  else
    if test -n "$short"
      complete -c exif -s "$short" -l "$long" -d "$description"
    else
      complete -c exif -l "$long" -d "$description"
    end
  end
end
