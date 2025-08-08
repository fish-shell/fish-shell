This pull request has many changes, mostly because I wanted to make a wrapper over an existing command that adds options to said command, and makes some changes to arguments.  However the original command has many options, so I didn't want to bother specifying them all manually to argparse, and then individually forward them over. Bassically, the motivation behind #6466.

However I got a bit carried away with various little things I noticed about `argparse` and `fish_opt`.  So I've split everything up into individual commits, in case you want to reject some of my changes but not all of them. After each commit I ran `make fish_run_tests sphinx-docs`, and there weren't any test/doc failures (although some tests where marked as ignored/skipped).

In detail, the commits make the following changes:
1. 281ebc461313ced6dcf7e41be1ba6445b803cd9a: this just cleans up the argparse tests so it's easier to debug test failures.
2. ac268473fb8febcb58ce55aeadde3a9532599e12: this makes argparse save all it's processed options into `$argv_opts` (i.e. what  #6466 was asking for). Specifically, every argument (other than the first `--`, if any) that `argparse` doesn't add to `$argv` is added to `$argv_opts`. So now you can make wrapper commands that modify non-option arguments, and then forwards all arguments to another command:
```fish
function ehead
	# These are all the options documented for 'head'
	set opts c/bytes= n/lines= q/quiet silent v/verbose z/zero-terminated help version
	# Parse the arguments
	argparse $opts -- $argv

	if (count $argv) -eq 0
		# Default to heading stderr
		# (whereas head defaults to stdin)
		set argv /dev/stderr		
	end
	# Call head with the modified $argv
	# But parsing verbatim all the options the user specified
	head $argv_opts -- $argv
end
```

3. ececb31ec774705b11c9e0431965c363eead8ffa: this makes it so you can tell `argparse` to *not* put an option in either `$argv` or `$argv_opts`. This is done by ending the option spec with `&` (or `&! ...` if your using the `!` validation feature). Alternatively, you can use `fish_opt` with it's new `-d` / `--delete` flag. This allows you to easily add new options when wrapping commands, or modify existing ones. For example,:
```fish
function qhead
	# Same as above, except that there is now an & at the end of the bytes=
	set opts c/bytes=& n/lines= q/quiet silent v/verbose z/zero-terminated help version
	# Add a new option
	set -a opts qwords=&
	argparse $opts -- $argv

	if set -q _flag_nibbles
		# --qwords allows specifying the size in multiples of 8 bytes
		set -a argv_opts --bytes=(math $_flag_nibbles \* 8)
	else if set -q _flag_bytes
		# Allows using a 'q' suffix, e.g. --bytes=4q to mean 4*8 bytes.
		if string match -r `q$` -- $_flag_bytes
			set -a argv_opts --bytes=(math (string replace -r 'q$' '*8'))
		else
			# Keep the users settings
			set -a argv_opts --bytes=$_flag_bytes
		end
		
	end

	# Call head with the modified $argv_opts
	head $argv_opts -- $argv
end
```
Without the `&` in the `qwords=&`, the `--qwords` flag would still be in `$argv_opts`, and `head` doesn't recognize that. Similarly it is needed in `c/bytes=&` to prevent `head` from getting a `--bytes` argument with a `q` suffix.

4. f0a397bd9ff998b4c0cd95068d6ccb8ea1787969: This fixes the issue mentioned in the "Limitations" section of `argparse`'s man page: when using `--ignore-unknown`, mixed groups of known and unknown short flags are now split up properly. For example, `argparse --ignore-unknown a -- -ab` now sets `$argv` to `-b`, and `$argv_opts` to `-a`. Previously, it would set `$argv` to `-ab`, and `$argv_opts` to empty. (However, in both the new and old behaviour, `_flag_a` would be set to `-a`). This allows your wrapped command to only specify a subset of options, e.g:
```fish
function uhead
	# This time, I don't bother specifying the options for head that require arguments
	set opts q/quiet silent v/verbose z/zero-terminated help version
	# Add a new option
	set -a opts u/uppercase
	argparse --ignore-unknown $opts -- $argv

	if set -q _flag_uppercase
		# Uppercase everything (we can't use `--` before `$argv` as it may contain unknown options).
		head $argv_opts $argv | tr [:lower:] [:upper:]
	else
		head $argv_opts $argv
	end
end
```

So `uhead -un 3` will correctly call `head -n 3 | tr [:lower:] [:upper:]`, whereas without my change, it would call `head -un 3 | tr [:lower:] [:upper:]`, but `head` doesn't take a `u` option.

5. 99c16fae2f84fc25842ba68dc898438a1549363b: This adds a new `-m`/`-move-unknown` option to `argparse`, this is like `--ignore-unknown`, but unknown options are instead moved from `$argv` and added to `$argv_opts`, just like known ones. This allows using `--` when forwarding unknown options, and treating the `$argv` as if it was a normal argument. For example, you can simplify example 2 above:
```
function ehead
	# No options specified
	argparse --ignore-unknown -- $argv

	if (count $argv) -eq 0
		set argv /dev/stderr		
	end
	head $argv_opts -- $argv
end
```

Without my change, `euhead -n3` would incorrectly call `head -- -n3`, (trying to read a file called `-n3`), and *not* `head -n3 -- /dev/stderr` (`-n3` is properly passed as an option). Even if you instead removed the `--` from the code above, but still didn't use my commit, then `sehead -- -file` would incorrectly call `head -file` (and there's no `-f` option to `head`). This is unfortunately not perfect, as it requires arguments to be 'stuck' to options (`euhead -n 3` won't work.) The next commit addresses this.

6. d863849296f51cfc1f7072eebf2f8cdeab17afe2: This adds a new `-a`/`--unknown-arguments <KIND>` option to `argparse`, this lets you specify whether unknown options should be assumed to take arguments. `<KIND>` can be either `optional` (the default, and original behaviour), `required`, or `none`. (This option is a no-op if neither `--ignore-unknown` nor  `--move-unknown` is specified). For example, the behaviour of `argparse -ia <KIND> ab -- -u -a -ub` differs as follows:
	
	* With `<KIND>` = `optional`: `_flag_a` is set, but `b` is treated as the argument to `u`, so `_flag_b` is *not* set.

	* With `<KIND>` = `required`: `-a` and `b` are *both* treated as arguments to `-u`, so neither `_flag_a` nor `_flag_b` are set.

	* With `<KIND>` = `none`: neither `-a` nor `b` are treated as arguments to `u`, so both `_flag_a` and `_flag_b` are both set.

With this new option, there are two ways to get an error when specifying unknown options:
	* with `<KIND>` = `required`, if an unknown option is the last argument to `argparse` (thus there was no value given).
	* With `<KIND>` = `none`, if an unknown long option has an `=` (e.g. `--flag=value` is an error).

I wasn't sure if giving errors was the best choice, as you might be using the `--ignore-unknown` option to supress errors.

With this commit, example 2. above can now be correctly abbreviated as:
```
function ehead
	# Only bother specifying the options that take values
	set opts c/bytes= n/lines= 
	argparse -a none $opts --ignore-unknown -- $argv

	# Or equivalently, by only specifying the options that don't take values:
	set opts q/quiet silent v/verbose z/zero-terminated help version
	argparse -a required $opts --ignore-unknown -- $argv

	if (count $argv) -eq 0
		set argv /dev/stderr		
	end
	head $argv_opts -- $argv
end
```

Personally, I think `--unknown-arguments none` would be a more usefull default as in my experience, most options to programs don't take arguments, an they rarely take optional arguments. However this would break backwards compatibility, so I've kept the default as `optional`.

### HASHES change from here
7. e3e0ed520b18174cc746ca00cbd4914e1c6c5f9cL: This adds a new `-L` / `--strict-longopts` flag that disables a very surprising and confusing feature I discovered in the code of wgetopt.rs. Specifically, wgetopt allows abbreviating the names of long flags and using only a single `-` with a long flag, for example `--long` and `--long=value` can be written as:

	* `-long` or `-long=value`, but *only* if there is no short flag `l`.

	* `--lo` and `--lo=value`, but *only* if there is no other long flag that starts with `lo`.

	* `-lo` and `-lo=value` (i.e. combining the above two).

The above does *not* apply to *unkown* options, so when using `--unknown-arguments none`, if there is no `a`, or `ab` option, but there is a `b` option, `-ab` will set `_flag_b`, and not assume that `ab` is some unknown long option.

I suspect this behaviour for fish option parsing was not intended, as when I forcibly disabled the above (i.e. force `--strict-longopts` to be true) for all uses of `wgetopt` (i.e. all of fishes option parsing, not just `argparse`), `make fish_run_tests` still passes.

Personally, I think `--strict-longopts` should be on by default, but that would be a backwards incompatible change (which could break some peoples fish code).

8. dbed849f01f70605d1fd7d630a15e586c84a22b9 Added argparse support for arguments with multiple optional values.
9. 9b7b4bf21019c891b408cac019a2e9dd6b9ad1a8 Added support to fish_opt for defining a long flag with no short flag.
10. 5745191ad31e81d02d70851be926da6dd87b17d0 Modified argparse to support one character long only options.
11. e51806fa027c3579f45d9cae7b3352060ac3cac8 Make argparse reject supplying a validator for a flag that cannot take a value
12. fcb9ea50c3acfb6525c4806c3c7b5ce1c83f2bf7 Added a --validate option to fish_opt

Things I considered doing, but I managed to restrain my self:

Let me know if you think, or would like me to make any other `argparse`/`fish_opt` related changes.