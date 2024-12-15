function __fish_complete_list --argument-names div cmd prefix iprefix
    if not set -q cmd[1]
        echo "Usage:
    __fish_complete_list <separator> <function> <prefix> <itemprefix>
where:
  separator - a symbol, separating individual entries
  function - a function which prints a completion list to complete each entry
  prefix - a prefix, which is printed before the list
  itemprefix - a prefix, which is printed before each item" >/dev/stderr
        return 1
    end
    set -q iprefix[1]
    or set -l iprefix ""
    set -q prefix[1]
    or set -l prefix ""
    set -l pat "$(commandline -t)"
    #set -l pat $argv[5]
    switch $pat
        case "*$div*"
            for i in (echo $pat | sed "s/^\(.\+$div\)$iprefix.*\$/\1/")$iprefix(eval $cmd)
                string unescape -- $i
            end
        case '*'
            for i in $prefix$iprefix(eval $cmd)
                string unescape -- $i
            end
    end

end
