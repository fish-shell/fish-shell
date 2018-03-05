
# magic completion safety check (do not remove this comment)
if not type -q pdflatex
    exit
end
complete -c tex -w pdflatex

