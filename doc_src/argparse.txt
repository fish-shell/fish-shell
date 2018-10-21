\section argparse argparse - parse options passed to a fish script or function

\subsection argparse-synopsis Synopsis
\fish{synopsis}
argparse [OPTIONS] OPTION_SPEC... -- [ARG...]
\endfish

\subsection argparse-description Description

This command makes it easy for fish scripts and functions to handle arguments in a manner 100% identical to how fish builtin commands handle their arguments. You pass a sequence of arguments that define the options recognized, followed by a literal `--`, then the arguments to be parsed (which might also include a literal `--`). More on this in the <a href="#argparse-usage">usage</a> section below.

Each OPTION_SPEC can be written in the domain specific language <a href="#argparse-option-specs">described below</a> or created using the companion <a href="#fish-opt">`fish_opt`</a> command. All OPTION_SPECs must appear after any argparse flags and before the `--` that separates them from the arguments to be parsed.

Each option that is seen in the ARG list will result in a var name of the form `_flag_X`, where `X` is the short flag letter and the long flag name. The OPTION_SPEC always requires a short flag even if it can't be used. So there will always be `_flag_X` var set using the short flag letter if the corresponding short or long flag is seen. The long flag name var (e.g., `_flag_help`) will only be defined, obviously, if the OPTION_SPEC includes a long flag name.

For example `_flag_h` and `_flag_help` if `-h` or `--help` is seen. The var will be set with local scope (i.e., as if the script had done `set -l _flag_X`). If the flag is a boolean (that is, does not have an associated value) the values are the short and long flags seen. If the option is not a boolean flag the values will be zero or more values corresponding to the values collected when the ARG list is processed. If the flag was not seen the flag var will not be set.

\subsection argparse-options Options

The following `argparse` options are available. They must appear before all OPTION_SPECs:

- `-n` or `--name` is the command name to insert into any error messages. If you don't provide this value `argparse` will be used.

- `-x` or `--exclusive` should be followed by a comma separated list of short of long options that are mutually exclusive. You can use this option more than once to define multiple sets of mutually exclusive options.

- `-N` or `--min-args` is followed by an integer that defines the minimum number of acceptable non-option arguments. The default is zero.

- `-X` or `--max-args` is followed by an integer that defines the maximum number of acceptable non-option arguments. The default is infinity.

- `-s` or `--stop-nonopt` causes scanning the arguments to stop as soon as the first non-option argument is seen. Using this arg is equivalent to calling the C function `getopt_long()` with the short options starting with a `+` symbol. This is sometimes known as "POSIXLY CORRECT". If this flag is not used then arguments are reordered (i.e., permuted) so that all non-option arguments are moved after option arguments. This mode has several uses but the main one is to implement a command that has subcommands.

- `-h` or `--help` displays help about using this command.

\subsection argparse-usage Usage

Using this command involves passing two sets of arguments separated by `--`. The first set consists of one or more option specifications (`OPTION_SPEC` above) and options that modify the behavior of `argparse`. These must be listed before the `--` argument. The second set are the arguments to be parsed in accordance with the option specifications. They occur after the `--` argument and can be empty. More about this below but here is a simple example that might be used in a function named `my_function`:

\fish
argparse --name=my_function 'h/help' 'n/name=' -- $argv
or return
\endfish

If `$argv` is empty then there is nothing to parse and `argparse` returns zero to indicate success. If `$argv` is not empty then it is checked for flags `-h`, `--help`, `-n` and `--name`. If they are found they are removed from the arguments and local variables (more on this <a href="argparse-local-variables">below</a>) are set so the script can determine which options were seen. Assuming `$argv` doesn't have any errors, such as a missing mandatory value for an option, then `argparse` exits with status zero. Otherwise it writes appropriate error messages to stderr and exits with a status of one.

The `--` argument is required. You do not have to include any arguments after the `--` but you must include the `--`. For example, this is acceptable:

\fish
set -l argv
argparse 'h/help' 'n/name' -- $argv
\endfish

But this is not:

\fish
set -l argv
argparse 'h/help' 'n/name' $argv
\endfish

The first `--` seen is what allows the `argparse` command to reliably separate the option specifications from the command arguments.

\subsection argparse-option-specs Option Specifications

Each option specification is a string composed of

- A short flag letter (which is mandatory). It must be an alphanumeric or "#". The "#" character is special and means that a flag of the form `-123` is valid. The short flag "#" must be followed by "-" (since the short name isn't otherwise valid since `_flag_#` is not a valid var name) and must be followed by a long flag name with no modifiers.

- A `/` if the short flag can be used by someone invoking your command else `-` if it should not be exposed as a valid short flag. If there is no long flag name these characters should be omitted. You can also specify a '#' to indicate the short and long flag names can be used and the value can be specified as an implicit int; i.e., a flag of the form `-NNN`.

- A long flag name which is optional. If not present then only the short flag letter can be used.

- Nothing if the flag is a boolean that takes no argument or is an implicit int flag, else

- `=` if it requires a value and only the last instance of the flag is saved, else

- `=?` it takes an optional value and only the last instance of the flag is saved, else

- `=+` if it requires a value and each instance of the flag is saved.

- Optionally a `!` followed by fish script to validate the value. Typically this will be a function to run. If the return status is zero the value for the flag is valid. If non-zero the value is invalid. Any error messages should be written to stdout (not stderr). See the section on <a href="#arparse-validation">Flag Value Validation</a> for more information.

See the <a href="#fish-opt">`fish_opt`</a> command for a friendlier but more verbose way to create option specifications.

In the following examples if a flag is not seen when parsing the arguments then the corresponding _flag_X var(s) will not be set.

\subsection argparse-validation Flag Value Validation

It is common to want to validate the the value provided for an option satisfies some criteria. For example, that it is a valid integer within a specific range. You can always do this after `argparse` returns but you can also request that `argparse` perform the validation by executing arbitrary fish script. To do so simply append an `!` (exclamation-mark) then the fish script to be run. When that code is executed three vars will be defined:

- `_argparse_cmd` will be set to the value of the value of the `argparse --name` value.

- `_flag_name` will be set to the short or long flag that being processed.

- `_flag_value` will be set to the value associated with the flag being processed.

If you do this via a function it should be defined with the `--no-scope-shadowing` flag. Otherwise it won't have access to those variables.

The script should write any error messages to stdout, not stderr. It should return a status of zero if the flag value is valid otherwise a non-zero status to indicate it is invalid.

Fish ships with a `_validate_int` function that accepts a `--min` and `--max` flag. Let's say your command accepts a `-m` or `--max` flag and the minimum allowable value is zero and the maximum is 5. You would define the option like this: `m/max=!_validate_int --min 0 --max 5`. The default if you just call `_validate_int` without those flags is to simply check that the value is a valid integer with no limits on the min or max value allowed.

\subsection argparse-optspec-examples Example OPTION_SPECs

Some OPTION_SPEC examples:

- `h/help` means that both `-h` and `--help` are valid. The flag is a boolean and can be used more than once. If either flag is used then `_flag_h` and `_flag_help` will be set to the count of how many times either flag was seen.

- `h-help` means that only `--help` is valid. The flag is a boolean and can be used more than once. If the long flag is used then `_flag_h` and `_flag_help` will be set to the count of how many times the long flag was seen.

- `n/name=` means that both `-n` and `--name` are valid. It requires a value and can be used at most once. If the flag is seen then `_flag_n` and `_flag_name` will be set with the single mandatory value associated with the flag.

- `n/name=?` means that both `-n` and `--name` are valid. It accepts an optional value and can be used at most once. If the flag is seen then `_flag_n` and `_flag_name` will be set with the value associated with the flag if one was provided else it will be set with no values.

- `n-name=+` means that only `--name` is valid. It requires a value and can be used more than once. If the flag is seen then `_flag_n` and `_flag_name` will be set with the values associated with each occurrence of the flag.

- `x` means that only `-x` is valid. It is a boolean can can be used more than once. If it is seen then `_flag_x` will be set to the count of how many times the flag was seen.

- `x=`, `x=?`, and `x=+` are similar to the n/name examples above but there is no long flag alternative to the short flag `-x`.

- `x-` is not valid since there is no long flag name and therefore the short flag, `-x`, has to be usable.

- `#-max` means that flags matching the regex "^--?\d+$" are valid. When seen they are assigned to the variable `_flag_max`. This allows any valid positive or negative integer to be specified by prefixing it with a single "-". Many commands support this idiom. For example `head -3 /a/file` to emit only the first three lines of /a/file.

- `n#max` means that flags matching the regex "^--?\d+$" are valid. When seen they are assigned to the variables `_flag_n` and `_flag_max`. This allows any valid positive or negative integer to be specified by prefixing it with a single "-". Many commands support this idiom. For example `head -3 /a/file` to emit only the first three lines of /a/file. You can also specify the value using either flag: `-n NNN` or `--max NNN` in this example.

After parsing the arguments the `argv` var is set with local scope to any values not already consumed during flag processing. If there are not unbound values the var is set but `count $argv` will be zero.

If an error occurs during argparse processing it will exit with a non-zero status and print error messages to stderr.

\subsection argparse-notes Notes

Prior to the addition of this builtin command in the 2.7.0 release there were two main ways to parse the arguments passed to a fish script or function. One way was to use the OS provided `getopt` command. The problem with that is that the GNU and BSD implementations are not compatible. Which makes using that external command difficult other than in trivial situations. The other way is to iterate over `$argv` and use the fish `switch` statement to decide how to handle the argument. That, however, involves a huge amount of boilerplate code. It is also borderline impossible to implement the same behavior as builtin commands.
