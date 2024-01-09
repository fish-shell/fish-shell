function load-dotenv -d 'Load .env file into environment'
    if [ -z "$argv" ]
        if [ -e .env ]
            # if no file is specified, use .env in current directory
            set argv[1] .env
        else
            echo 'No file specified and .env does not exist in current directory'
            return 1
        end
    end

    for file in $argv
        if [ ! -e $file ]
            echo "File $file does not exist"
            false
            continue
        end

        for line in (cat $file)
            set line (string trim $line)
            if [ -z $line ] || string match -q '#*' $line
                # skip empty lines and comments
                continue
            end
            # expand any variables in the value of the line before exporting
            # this allows for things like var1=foo and var2=$var1.txt
            export (echo "echo $line" | source)
        end
    end
end
