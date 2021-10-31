function fish_fletcher16 --argument input -d "compute a fletcher 16 checksum"
    set -l sum1 0
    set -l sum2 0
    for char in (string split '' $input)
        set -l ordinal (printf %2u "'$char")
        math "($sum1 + $ordinal)" % 255 | read sum1
        math "($sum2 + $sum1)" % 255 | read sum2
    end
    math "bitor $sum1,$(math $sum2 '*' pow 2,8)"
end
