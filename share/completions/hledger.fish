# Completions for hledger, updated for version 1.42+.
# https://hledger.org

# TODO: Handle the -NUM form of --depth=NUM.

set -l debug_opts '
    1\tdefault
    2\t
    3\t
    4\t
    5\t
    6\t
    7\t
    8\t
    9\t
'

set -l takes_general_input_flags 'add commodities files import'
set -l takes_general_reporting_flags 'check close rewrite aregister areg balancesheet bs balancesheetequity bse cashflow cf incomestatement is roi accounts activity balance bal codes descriptions diff notes payees prices print p register reg stats tags'

# General input flags
complete -c hledger -r -s f -l file -d 'Input file' -n "__fish_seen_subcommand_from $takes_general_input_flags $takes_general_reporting_flags"
complete -c hledger -r -l rules -d 'CSV-conversion-rules file' -n "__fish_seen_subcommand_from $takes_general_input_flags $takes_general_reporting_flags"
complete -c hledger -r -l alias -d 'Rename account' -n "__fish_seen_subcommand_from $takes_general_input_flags $takes_general_reporting_flags"
complete -c hledger -f -l anon -d 'Anonymize accounts and payees' -n "__fish_seen_subcommand_from $takes_general_input_flags $takes_general_reporting_flags"
complete -c hledger -x -l pivot -d 'Use some other field/tag for account names' -n "__fish_seen_subcommand_from $takes_general_input_flags $takes_general_reporting_flags"
complete -c hledger -x -s I -l ignore-assertions -d 'Ignore any balance assertions' -n "__fish_seen_subcommand_from $takes_general_input_flags $takes_general_reporting_flags"
complete -c hledger -f -l auto -d 'Apply automated posting rules to modify txns' -n "__fish_seen_subcommand_from $takes_general_input_flags $takes_general_reporting_flags"
complete -c hledger -f -l forecast -d 'Generate future txns from periodic rules' -n "__fish_seen_subcommand_from $takes_general_input_flags $takes_general_reporting_flags"
complete -c hledger -f -s s -l strict -d 'Do extra error checks' -n "__fish_seen_subcommand_from $takes_general_input_flags $takes_general_reporting_flags"
complete -c hledger -f -l infer-costs -d 'Infer conversion equity postings from costs' -n "__fish_seen_subcommand_from $takes_general_input_flags $takes_general_reporting_flags"
complete -c hledger -f -l infer-equity -d 'Infer costs from conversion equity postings' -n "__fish_seen_subcommand_from $takes_general_input_flags $takes_general_reporting_flags"
complete -c hledger -f -l infer-market-prices -d 'Infer market prices from costs' -n "__fish_seen_subcommand_from $takes_general_input_flags $takes_general_reporting_flags"
complete -c hledger -f -l verbose-tags -d 'Add tags indicating generated/modified data' -n "__fish_seen_subcommand_from $takes_general_input_flags $takes_general_reporting_flags"

# General reporting flags
complete -c hledger -x -s b -l begin -d 'Include postings/txns on or after this date' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -x -s e -l end -d 'Include postings/txns before this date' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -f -s D -l daily -d 'Multiperiod/multicolumn report by day' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -f -s W -l weekly -d 'Multiperiod/multicolumn report by week' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -f -s M -l monthly -d 'Multiperiod/multicolumn report by month' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -f -s Q -l quarterly -d 'Multiperiod/multicolumn report by quarter' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -f -s Y -l yearly -d 'Multiperiod/multicolumn report by year' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -x -s p -l period -d 'Set start date, end date, and/or report interval' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -f -l date2 -d 'Match secondary date instead (deprecated)' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -f -s U -l unmarked -d 'Include only unmarked postings/txns' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -f -s P -l pending -d 'Include only pending ("!") postings/txns' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -f -s C -l cleared -d 'Include only cleared ("*") postings/txns' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -f -s R -l real -d 'Include only non-virtual postings' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -x -l depth -d 'Hide accounts/postings deeper than this' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -f -s E -l empty -d 'Show items with zero amount' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -f -s B -l cost -d 'Convert amounts to their cost at txn time' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -f -s V -l market -d 'Convert amounts to market value at period end' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -x -s X -l exchange -d 'Convert amounts to specified commodity' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -x -l value -d 'Show amounts converted to value on specified date' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -x -s c -l commodity-style -d "Override a commodity's display style" -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -f -l pretty -d 'Use box-drawing characters in text output' -n "__fish_seen_subcommand_from $takes_general_reporting_flags"
complete -c hledger -x -l today -d "Override today's date" -n "__fish_seen_subcommand_from $takes_general_reporting_flags"

# General help flags
complete -c hledger -f -s h -l help -d 'Show help for this command'
complete -c hledger -x -l debug -a "$debug_opts" -d 'Show debug output'
complete -c hledger -f -l version -d 'Show version information'
complete -c hledger -f -l tldr -d 'Show command examples with tldr'
complete -c hledger -f -l info -d 'Show the manual with info'
complete -c hledger -f -l man -d 'Show the manual with man'
complete -c hledger -x -l color -l colour -a 'y yes n no auto' -d 'Use ANSI color'
complete -c hledger -x -l pager -a 'y yes n no' -d 'Use a pager when needed'
complete -c hledger -f -s n -l no-conf -d 'Ignore any config file'
complete -c hledger -r -l conf -d 'Use extra options from this config file'

# Commands

# Help commands
complete -c hledger -f -n __fish_use_subcommand -a commands -d 'Show the commands list'
complete -c hledger -f -n __fish_use_subcommand -a demo -d 'Show brief demos in the terminal'
complete -c hledger -f -n __fish_use_subcommand -a help -d 'Show the hledger manual'

# User interface commands
complete -c hledger -f -n __fish_use_subcommand -a repl -d 'Run commands from an interactive prompt'
complete -c hledger -f -n __fish_use_subcommand -a run -d 'Run command scripts from files or arguments'

# Data entry commands
complete -c hledger -f -n __fish_use_subcommand -a add -d 'Add transactions using interactive prompts'
complete -c hledger -f -n '__fish_seen_subcommand_from add' -l no-new-accounts -d "Don't allow creating new accounts"

complete -c hledger -n __fish_use_subcommand -a import -d 'Add new transactions from other files'
complete -c hledger -f -n '__fish_seen_subcommand_from import' -l dry-run -d 'Just show the transactions to be imported'

# Basic reports
complete -c hledger -f -n __fish_use_subcommand -a accounts -d 'Show account names'
complete -c hledger -f -n '__fish_seen_subcommand_from accounts' -l declared -d 'Show account names declared with account directives'
complete -c hledger -f -n '__fish_seen_subcommand_from accounts' -l used -d 'Show account names referenced by transactions'
complete -c hledger -f -n '__fish_seen_subcommand_from accounts' -l tree -d 'Show short account names as a tree'
complete -c hledger -f -n '__fish_seen_subcommand_from accounts' -l flat -d 'Show short account names as a list'
complete -c hledger -x -n '__fish_seen_subcommand_from accounts' -l drop -d 'Omit N leading account name parts in flat mode'

complete -c hledger -f -n __fish_use_subcommand -a codes -d 'Show transaction codes'

complete -c hledger -f -n __fish_use_subcommand -a commodities -d 'Show commodity/currency symbols'

complete -c hledger -f -n __fish_use_subcommand -a descriptions -d 'Show transaction descriptions'

complete -c hledger -f -n __fish_use_subcommand -a files -d 'Show data files in use'

complete -c hledger -f -n __fish_use_subcommand -a notes -d 'Show note part of transaction descriptions'

complete -c hledger -f -n __fish_use_subcommand -a payees -d 'Show payee part of transaction descriptions'

complete -c hledger -f -n __fish_use_subcommand -a prices -d 'Show historical market prices'
complete -c hledger -f -n '__fish_seen_subcommand_from prices' -l costs -d 'Print transaction prices from postings'
complete -c hledger -f -n '__fish_seen_subcommand_from prices' -l inverted-costs -d 'Print inverted transaction prices from postings'

complete -c hledger -f -n __fish_use_subcommand -a stats -d 'Show journal statistics'
complete -c hledger -r -n '__fish_seen_subcommand_from stats' -s o -l output-file -d 'Write output to given file'

complete -c hledger -f -n __fish_use_subcommand -a tags -d 'Show tag names'

# Standard reports
complete -c hledger -f -n __fish_use_subcommand -a 'print p' -d 'Show full transaction entries'
complete -c hledger -x -n '__fish_seen_subcommand_from print p' -s m -l match -d 'Show the most-recent transaction most similar to STR'
complete -c hledger -f -n '__fish_seen_subcommand_from print p' -s x -l explicit -d 'Show all amounts explicitly'
complete -c hledger -f -n '__fish_seen_subcommand_from print p' -l new -d 'Show only newer-dated transactions added since last run'
complete -c hledger -x -n '__fish_seen_subcommand_from print p' -s O -l output-format -a 'txt csv html json sql' -d 'Select an output format'
complete -c hledger -r -n '__fish_seen_subcommand_from print p' -s o -l output-file -d 'Write output to given file'

complete -c hledger -f -n __fish_use_subcommand -a 'aregister areg' -d 'Show transactions & running balance in one account'
complete -c hledger -f -n '__fish_seen_subcommand_from aregister areg' -l cumulative -d 'Show running total from report start date'
complete -c hledger -f -n '__fish_seen_subcommand_from aregister areg' -s H -l historical -d 'Show historical running total/balance'
complete -c hledger -x -n '__fish_seen_subcommand_from aregister areg' -s w -l width -d 'Set output width'
complete -c hledger -x -n '__fish_seen_subcommand_from aregister areg' -s O -l output-format -a 'txt csv html json' -d 'Select an output format'
complete -c hledger -r -n '__fish_seen_subcommand_from aregister areg' -s o -l output-file -d 'Write output to given file'

complete -c hledger -f -n __fish_use_subcommand -a 'register reg' -d 'Show postings & running total in accounts'
complete -c hledger -f -n '__fish_seen_subcommand_from register reg' -l cumulative -d 'Show running total from report start date'
complete -c hledger -f -n '__fish_seen_subcommand_from register reg' -s H -l historical -d 'Show historical running total/balance'
complete -c hledger -f -n '__fish_seen_subcommand_from register reg' -s A -l average -d 'Show running average of posting amounts instead of total'
complete -c hledger -f -n '__fish_seen_subcommand_from register reg' -s r -l related -d "Show postings' siblings instead"
complete -c hledger -f -n '__fish_seen_subcommand_from register reg' -l invert -d 'Display all amounts with reversed sign'
complete -c hledger -x -n '__fish_seen_subcommand_from register reg' -s w -l width -d 'Set output width'
complete -c hledger -x -n '__fish_seen_subcommand_from register reg' -s O -l output-format -a 'txt csv html json' -d 'Select an output format'
complete -c hledger -r -n '__fish_seen_subcommand_from register reg' -s o -l output-file -d 'Write output to given file'

# Financial reports (balancesheet, balancesheetequity, cashflow, incomestatement share flags)
set -l financial_reports_commands 'balancesheet bs balancesheetequity bse cashflow cf incomestatement is'
complete -c hledger -f -n __fish_use_subcommand -a 'balancesheet       bs' -d 'Show assets and liabilities'
complete -c hledger -f -n __fish_use_subcommand -a 'balancesheetequity bse' -d 'Show assets, liabilities, and equity'
complete -c hledger -f -n __fish_use_subcommand -a 'cashflow           cf' -d 'Show changes in liquid assets'
complete -c hledger -f -n __fish_use_subcommand -a 'incomestatement    is' -d 'Show revenues and expenses'
complete -c hledger -f -n "__fish_seen_subcommand_from $financial_reports_commands" -l change -d 'Show balance change in each period'
complete -c hledger -f -n "__fish_seen_subcommand_from $financial_reports_commands" -l cumulative -d 'Show balance change accumulated across periods'
complete -c hledger -f -n "__fish_seen_subcommand_from $financial_reports_commands" -s H -l historical -d 'Show historical ending balance in each period'
complete -c hledger -f -n "__fish_seen_subcommand_from $financial_reports_commands" -l flat -d 'Show accounts as a list'
complete -c hledger -x -n "__fish_seen_subcommand_from $financial_reports_commands" -l drop -d 'Omit N leading account-name parts (in flat mode)'
complete -c hledger -f -n "__fish_seen_subcommand_from $financial_reports_commands" -s N -l no-total -d 'Omit the final total row'
complete -c hledger -f -n "__fish_seen_subcommand_from $financial_reports_commands" -l tree -d 'Show accounts as a tree; amounts include subaccounts'
complete -c hledger -f -n "__fish_seen_subcommand_from $financial_reports_commands" -s A -l average -d 'Show a row average column in multicolumn reports'
complete -c hledger -f -n "__fish_seen_subcommand_from $financial_reports_commands" -s T -l row-total -d 'Show a row total column in multicolumn reports'
complete -c hledger -f -n "__fish_seen_subcommand_from $financial_reports_commands" -l no-elide -d "Don't squash boring parent accounts in tree mode"
complete -c hledger -x -n "__fish_seen_subcommand_from $financial_reports_commands" -l format -d 'Use a custom line format in multicolumn reports'
complete -c hledger -f -n "__fish_seen_subcommand_from $financial_reports_commands" -s S -l sort-amount -d 'Sort by amount instead of account code/name'
complete -c hledger -x -n "__fish_seen_subcommand_from $financial_reports_commands" -s O -l output-format -a 'txt csv html json' -d 'Select an output format'
complete -c hledger -r -n "__fish_seen_subcommand_from $financial_reports_commands" -s o -l output-file -d 'Write output to given file'

# Advanced reports
complete -c hledger -f -n __fish_use_subcommand -a 'balance bal' -d 'Show balance changes, end balances, gains, budgets'
complete -c hledger -f -n '__fish_seen_subcommand_from balance bal' -l change -d 'Show balance change in each period'
complete -c hledger -f -n '__fish_seen_subcommand_from balance bal' -l cumulative -d 'Show balance change accumulated across periods'
complete -c hledger -f -n '__fish_seen_subcommand_from balance bal' -s H -l historical -d 'Show historical ending balance in each period'
complete -c hledger -f -n '__fish_seen_subcommand_from balance bal' -l tree -d 'Show accounts as a tree'
complete -c hledger -f -n '__fish_seen_subcommand_from balance bal' -l flat -d 'Show accounts as a list'
complete -c hledger -f -n '__fish_seen_subcommand_from balance bal' -s A -l average -d 'Show a row average column in multicolumn reports'
complete -c hledger -f -n '__fish_seen_subcommand_from balance bal' -s T -l row-total -d 'Show a row total column in multicolumn reports'
complete -c hledger -f -n '__fish_seen_subcommand_from balance bal' -s N -l no-total -d 'Omit the final row'
complete -c hledger -x -n '__fish_seen_subcommand_from balance bal' -l drop -d 'Omit N leading account name parts in flat mode'
complete -c hledger -f -n '__fish_seen_subcommand_from balance bal' -l no-elide -d "Don't squash boring parent accounts in tree mode"
complete -c hledger -x -n '__fish_seen_subcommand_from balance bal' -l format -d 'Use a custom line format in multicolumn reports'
complete -c hledger -f -n '__fish_seen_subcommand_from balance bal' -s S -l sort-amount -d 'Sort by amount instead of account code/name'
complete -c hledger -f -n '__fish_seen_subcommand_from balance bal' -l budget -d 'Show performance compared to budget goals'
complete -c hledger -f -n '__fish_seen_subcommand_from balance bal' -l invert -d 'Display all amounts with reversed sign'
complete -c hledger -f -n '__fish_seen_subcommand_from balance bal' -l transpose -d 'Transpose rows and columns'
complete -c hledger -x -n '__fish_seen_subcommand_from balance bal' -s O -l output-format -a 'txt csv html json' -d 'Select an output format'
complete -c hledger -r -n '__fish_seen_subcommand_from balance bal' -s o -l output-file -d 'Write output to given file'

complete -c hledger -f -n __fish_use_subcommand -a roi -d 'Show return on investments'
complete -c hledger -f -n '__fish_seen_subcommand_from roi' -l cashflow -d 'Show all amounts that were used to compute returns'
complete -c hledger -x -n '__fish_seen_subcommand_from roi' -l investment -d 'Query to select investment transactions'
complete -c hledger -x -n '__fish_seen_subcommand_from roi' -l profit-loss -l pnl -d 'Query to select profit-and-loss transactions'

# Chart commands
complete -c hledger -f -n __fish_use_subcommand -a activity -d 'Show posting counts as a bar chart'

# Data generation commands
complete -c hledger -f -n __fish_use_subcommand -a close -d 'Generate transactions to zero/restore/assert balances'
complete -c hledger -f -n '__fish_seen_subcommand_from close' -l open -d 'Show just opening transaction'
complete -c hledger -f -n '__fish_seen_subcommand_from close' -l close -d 'Show just closing transaction'
complete -c hledger -f -n '__fish_seen_subcommand_from close' -l assert -d 'Show just balance-asserting transaction'
complete -c hledger -f -n '__fish_seen_subcommand_from close' -l assign -d 'Show just balance-assigning transaction'
complete -c hledger -f -n '__fish_seen_subcommand_from close' -l retain -d 'Show just equity-retaining transaction'
complete -c hledger -f -n '__fish_seen_subcommand_from close' -l migrate -d 'Show close and open transactions (default)'
complete -c hledger -x -n '__fish_seen_subcommand_from close' -l close-acct -d 'Set the closing account'
complete -c hledger -x -n '__fish_seen_subcommand_from close' -l open-acct -d 'Set the opening account'

complete -c hledger -f -n __fish_use_subcommand -a rewrite -d 'Add postings to transactions, like print --auto'
complete -c hledger -f -n '__fish_seen_subcommand_from rewrite' -l add-posting -d 'Add a posting to account'
complete -c hledger -f -n '__fish_seen_subcommand_from rewrite' -l diff -d 'Generate diff suitable for patch(1)'

# Maintenance commands
complete -c hledger -f -n __fish_use_subcommand -a check -d "Run hledger's built-in correctness checks"
complete -c hledger -f -n __fish_use_subcommand -a diff -d "Compare an account's transactions in two journals"
complete -c hledger -f -n __fish_use_subcommand -a setup -d 'Check the status of the hledger installation'
complete -c hledger -f -n __fish_use_subcommand -a test -d 'Run self-tests'
