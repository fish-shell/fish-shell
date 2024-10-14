# RUN: %fish --interactive %s

commandline '123'; commandline --cursor 0; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 124

commandline '123'; commandline --cursor 0; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 122

commandline '123'; commandline --cursor 1; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 124

commandline '123'; commandline --cursor 1; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 122

commandline '123'; commandline --cursor 2; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 124

commandline '123'; commandline --cursor 2; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 122

commandline '123'; commandline --cursor 3; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 123

commandline '123'; commandline --cursor 3; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 123

commandline 'abc123'; commandline --cursor 1; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: abc124

commandline 'abc123'; commandline --cursor 1; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: abc122

commandline 'abc123def'; commandline --cursor 1; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: abc124def

commandline 'abc123def'; commandline --cursor 1; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: abc122def

commandline 'abc123def'; commandline --cursor 5; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: abc124def

commandline 'abc123def'; commandline --cursor 5; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: abc122def

commandline 'abc123def'; commandline --cursor 6; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: abc123def

commandline 'abc123def'; commandline --cursor 6; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: abc123def

commandline 'abc99def'; commandline --cursor 1; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: abc100def

commandline 'abc99def'; commandline --cursor 1; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: abc98def

commandline 'abc-99def'; commandline --cursor 1; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: abc-98def

commandline 'abc-99def'; commandline --cursor 1; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: abc-100def

commandline 'to 2022-04-09'; commandline --cursor 4; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: to 2023-04-09

commandline 'to 2022-04-09'; commandline --cursor 4; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: to 2021-04-09

commandline 'to 2022-04-09'; commandline --cursor 11; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: to 2022-04-10



# Test when the cursor is at the beginning of a number
commandline '007'; commandline --cursor 0; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 8

commandline '007'; commandline --cursor 0; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 6

# Test when a number has leading zeros
commandline '00012'; commandline --cursor 2; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 00013

commandline '00012'; commandline --cursor 2; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 00011

# Test when the number is negative
commandline '-45'; commandline --cursor 1; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: -44

commandline '-45'; commandline --cursor 1; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: -46

# Test in the middle of a mixed string
commandline 'abc-789xyz'; commandline --cursor 5; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: abc-788xyz

commandline 'abc-789xyz'; commandline --cursor 5; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: abc-790xyz

# Test multi-digit numbers with cursor on different positions
commandline '12345'; commandline --cursor 3; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 12346

commandline '12345'; commandline --cursor 3; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 12344

# Test at the boundaries of the string (cursor before and after the number)
commandline '789'; commandline --cursor 3; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 789

commandline '789'; commandline --cursor 3; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 789

commandline 'hello123'; commandline --cursor 8; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: hello124

commandline 'hello123'; commandline --cursor 8; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: hello122

# Test with mixed symbols and numbers
commandline 'abc123!'; commandline --cursor 6; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: abc124!

commandline 'abc123!'; commandline --cursor 6; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: abc122!

# Test multiple numbers in one string
commandline '123abc456'; commandline --cursor 2; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 124abc456

commandline '123abc456'; commandline --cursor 7; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 123abc457

commandline '123abc456'; commandline --cursor 2; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 122abc456

commandline '123abc456'; commandline --cursor 7; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 123abc455

# Test large numbers
commandline '999999'; commandline --cursor 3; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 1000000

commandline '1000000'; commandline --cursor 3; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 999999

# Test number with decimal points
commandline '3.14'; commandline --cursor 1; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 4.14

commandline '3.14'; commandline --cursor 1; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 2.14

# Test number with negative sign and decimal
commandline '-3.14'; commandline --cursor 1; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: -2.14

commandline '-3.14'; commandline --cursor 1; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: -4.14

# Test numbers within parentheses (common in text)
commandline '(123)'; commandline --cursor 2; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: (124)

commandline '(123)'; commandline --cursor 2; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: (122)


# Test with two separate strings on the same line
commandline 'abc123 def456'; commandline --cursor 4; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: abc124 def456

commandline 'abc123 def456'; commandline --cursor 4; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: abc122 def456

commandline 'abc123 def456'; commandline --cursor 10; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: abc123 def457

commandline 'abc123 def456'; commandline --cursor 10; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: abc123 def455

# Test with more complex strings and spaces
commandline 'test1 42 abc99 xyz100'; commandline --cursor 5; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: test1 43 abc99 xyz100

commandline 'test1 42 abc99 xyz100'; commandline --cursor 5; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: test1 41 abc99 xyz100

commandline 'test1 42 abc99 xyz100'; commandline --cursor 10; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: test1 42 abc100 xyz100

commandline 'test1 42 abc99 xyz100'; commandline --cursor 10; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: test1 42 abc98 xyz100

commandline 'test1 42 abc99 xyz100'; commandline --cursor 15; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: test1 42 abc99 xyz101

commandline 'test1 42 abc99 xyz100'; commandline --cursor 15; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: test1 42 abc99 xyz99

# Test with numbers separated by special characters
commandline 'num1, num2, num3'; commandline --cursor 3; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: num2, num2, num3

commandline 'num1, num2, num3'; commandline --cursor 3; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: num0, num2, num3

commandline 'num1, num2, num3'; commandline --cursor 9; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: num1, num3, num3

commandline 'num1, num2, num3'; commandline --cursor 9; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: num1, num1, num3

commandline 'num1, num2, num3'; commandline --cursor 15; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: num1, num2, num4

commandline 'num1, num2, num3'; commandline --cursor 15; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: num1, num2, num2

# Test with multiple numbers and spaces between words
commandline 'apple12 orange34 grape56'; commandline --cursor 7; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: apple13 orange34 grape56

commandline 'apple12 orange34 grape56'; commandline --cursor 7; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: apple11 orange34 grape56

commandline 'apple12 orange34 grape56'; commandline --cursor 15; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: apple12 orange35 grape56

commandline 'apple12 orange34 grape56'; commandline --cursor 15; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: apple12 orange33 grape56

commandline 'apple12 orange34 grape56'; commandline --cursor 22; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: apple12 orange34 grape57

commandline 'apple12 orange34 grape56'; commandline --cursor 22; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: apple12 orange34 grape55

# Test with mixed numbers and text with punctuation
commandline 'num1! num2? num3.'; commandline --cursor 3; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: num2! num2? num3.

commandline 'num1! num2? num3.'; commandline --cursor 3; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: num0! num2? num3.

commandline 'num1! num2? num3.'; commandline --cursor 9; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: num1! num3? num3.

commandline 'num1! num2? num3.'; commandline --cursor 9; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: num1! num1? num3.

commandline 'num1! num2? num3.'; commandline --cursor 15; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: num1! num2? num4.

commandline 'num1! num2? num3.'; commandline --cursor 15; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: num1! num2? num2.



# Test with identical numbers in different parts of the string
commandline '123 abc 123 xyz 123'; commandline --cursor 1; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 124 abc 123 xyz 123

commandline '123 abc 123 xyz 123'; commandline --cursor 1; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 122 abc 123 xyz 123

commandline '123 abc 123 xyz 123'; commandline --cursor 9; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 123 abc 124 xyz 123

commandline '123 abc 123 xyz 123'; commandline --cursor 9; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 123 abc 122 xyz 123

commandline '123 abc 123 xyz 123'; commandline --cursor 17; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 123 abc 123 xyz 124

commandline '123 abc 123 xyz 123'; commandline --cursor 17; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 123 abc 123 xyz 122

# Test with identical numbers that include leading zeros
commandline '00123 abc 00123 xyz 00123'; commandline --cursor 3; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 00124 abc 00123 xyz 00123

commandline '00123 abc 00123 xyz 00123'; commandline --cursor 3; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 00122 abc 00123 xyz 00123

commandline '00123 abc 00123 xyz 00123'; commandline --cursor 11; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 00123 abc 00124 xyz 00123

commandline '00123 abc 00123 xyz 00123'; commandline --cursor 11; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 00123 abc 00122 xyz 00123

commandline '00123 abc 00123 xyz 00123'; commandline --cursor 19; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 00123 abc 00123 xyz 00124

commandline '00123 abc 00123 xyz 00123'; commandline --cursor 19; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 00123 abc 00123 xyz 00122

# Test with repeated patterns and spaces
commandline 'abc123 abc123 abc123'; commandline --cursor 4; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: abc124 abc123 abc123

commandline 'abc123 abc123 abc123'; commandline --cursor 4; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: abc122 abc123 abc123

commandline 'abc123 abc123 abc123'; commandline --cursor 11; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: abc123 abc124 abc123

commandline 'abc123 abc123 abc123'; commandline --cursor 11; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: abc123 abc122 abc123

commandline 'abc123 abc123 abc123'; commandline --cursor 18; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: abc123 abc123 abc124

commandline 'abc123 abc123 abc123'; commandline --cursor 18; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: abc123 abc123 abc122

# Test with repeated numbers with special characters between
commandline '123-123-123'; commandline --cursor 1; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 124-123-123

commandline '123-123-123'; commandline --cursor 1; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 122-123-123

commandline '123-123-123'; commandline --cursor 5; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 123-124-123

commandline '123-123-123'; commandline --cursor 5; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 123-122-123

commandline '123-123-123'; commandline --cursor 9; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 123-123-124

commandline '123-123-123'; commandline --cursor 9; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 123-123-122

# Test with repeated complex patterns
commandline 'test-abc99 test-abc99 test-abc99'; commandline --cursor 9; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: test-abc100 test-abc99 test-abc99

commandline 'test-abc99 test-abc99 test-abc99'; commandline --cursor 9; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: test-abc98 test-abc99 test-abc99

commandline 'test-abc99 test-abc99 test-abc99'; commandline --cursor 19; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: test-abc99 test-abc100 test-abc99

commandline 'test-abc99 test-abc99 test-abc99'; commandline --cursor 19; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: test-abc99 test-abc98 test-abc99

commandline 'test-abc99 test-abc99 test-abc99'; commandline --cursor 29; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: test-abc99 test-abc99 test-abc100

commandline 'test-abc99 test-abc99 test-abc99'; commandline --cursor 29; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: test-abc99 test-abc99 test-abc98

# Test with multiple identical substrings including different numbers
commandline 'abc123 abc456 abc123'; commandline --cursor 4; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: abc124 abc456 abc123

commandline 'abc123 abc456 abc123'; commandline --cursor 4; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: abc122 abc456 abc123

commandline 'abc123 abc456 abc123'; commandline --cursor 12; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: abc123 abc457 abc123

commandline 'abc123 abc456 abc123'; commandline --cursor 12; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: abc123 abc455 abc123

commandline 'abc123 abc456 abc123'; commandline --cursor 18; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: abc123 abc456 abc124

commandline 'abc123 abc456 abc123'; commandline --cursor 18; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: abc123 abc456 abc122


# Test with a single '0' on the command line
commandline '0'; commandline --cursor 0; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 1

commandline '0'; commandline --cursor 0; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: -1

# Test with leading zeros
commandline '000'; commandline --cursor 0; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 001

commandline '000'; commandline --cursor 0; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: -001

# Test with zero within text
commandline 'abc0'; commandline --cursor 3; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: abc1

commandline 'abc0'; commandline --cursor 3; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: abc-1

# Test with zero at different positions in a string
commandline '0abc0'; commandline --cursor 0; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 1abc0

commandline '0abc0'; commandline --cursor 0; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: -1abc0

commandline '0abc0'; commandline --cursor 4; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 0abc1

commandline '0abc0'; commandline --cursor 4; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 0abc-1

# Test with '0' at multiple positions with spaces
commandline '0 0 0'; commandline --cursor 0; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 1 0 0

commandline '0 0 0'; commandline --cursor 0; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: -1 0 0

commandline '0 0 0'; commandline --cursor 2; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 0 1 0

commandline '0 0 0'; commandline --cursor 2; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 0 -1 0

commandline '0 0 0'; commandline --cursor 4; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 0 0 1

commandline '0 0 0'; commandline --cursor 4; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 0 0 -1

# Test with multiple zeros and leading zeros
commandline '000 0 000'; commandline --cursor 0; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 001 0 000

commandline '000 0 000'; commandline --cursor 0; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: -001 0 000

commandline '000 0 000'; commandline --cursor 5; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 000 1 000

commandline '000 0 000'; commandline --cursor 5; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 000 -1 000

commandline '000 0 000'; commandline --cursor 8; __fish_inc_dec_number_under_cursor increase
commandline --current-buffer
# CHECK: 000 0 001

commandline '000 0 000'; commandline --cursor 8; __fish_inc_dec_number_under_cursor decrease
commandline --current-buffer
# CHECK: 000 0 -001
