# Lists PHPUnit test suites
function __fish_phpunit_list_suites
    __fish_phpunit_list --list-suites
end

# Lists PHPUnit test groups
function __fish_phpunit_list_groups
    __fish_phpunit_list --list-groups
end

# Lists PHPUnit objects corresponding to the given option
function __fish_phpunit_list --argument-names option
    # Use the same PHPUnit binary as in the command being completed
    set -l phpunit (commandline -xpc)[1]
    test -x $phpunit
    or return

    $phpunit $option | tail -n +4 | string trim -c ' -'
end

# Code Coverage Options:
complete -x -c phpunit -l coverage-clover -d 'Generate code coverage report in Clover XML format'
complete -x -c phpunit -l coverage-crap4j -d 'Generate code coverage report in Crap4J XML format'
complete -x -c phpunit -l coverage-html -d 'Generate code coverage report in HTML format'
complete -x -c phpunit -l coverage-php -d 'Export PHP_CodeCoverage object to file'
complete -x -c phpunit -l coverage-text -d 'Generate code coverage report in text format'
complete -x -c phpunit -l coverage-xml -d 'Generate code coverage report in PHPUnit XML format'
complete -x -c phpunit -l whitelist -d 'Whitelist <dir> for code coverage analysis'
complete -f -c phpunit -l disable-coverage-ignore -d 'Disable annotations for ignoring code coverage'
complete -f -c phpunit -l no-coverage -d 'Ignore code coverage configuration'
complete -x -c phpunit -l dump-xdebug-filter -d 'Generate script to set Xdebug code coverage filter'

# Logging Options:
complete -x -c phpunit -l log-junit -d 'Log test execution in JUnit XML format to file'
complete -x -c phpunit -l log-teamcity -d 'Log test execution in TeamCity format to file'
complete -x -c phpunit -l testdox-html -d 'Write agile documentation in HTML format to file'
complete -x -c phpunit -l testdox-text -d 'Write agile documentation in Text format to file'
complete -x -c phpunit -l testdox-xml -d 'Write agile documentation in XML format to file'
complete -f -c phpunit -l reverse-list -d 'Print defects in reverse order'

# Test Selection Options:
complete -x -c phpunit -l filter -d 'Filter which tests to run'
complete -x -c phpunit -l testsuite -a '(__fish_phpunit_list_suites)' -d 'Filter which testsuite to run'
complete -x -c phpunit -l group -a '(__fish_phpunit_list_groups)' -d 'Only runs tests from the specified group(s)'
complete -f -c phpunit -l exclude-group -d 'Exclude tests from the specified group(s)'
complete -f -c phpunit -l list-groups -d 'List available test groups'
complete -f -c phpunit -l list-suites -d 'List available test suites'
complete -f -c phpunit -l list-tests -d 'List available tests'
complete -x -c phpunit -l list-tests-xml -d 'List available tests in XML format'
complete -x -c phpunit -l test-suffix -d 'Only search for test in files with specified suffix(es). Default: Test.php,.phpt'

# Test Execution Options:
complete -f -c phpunit -l dont-report-useless-tests -d 'Do not report tests that do not test anything'
complete -f -c phpunit -l strict-coverage -d 'Be strict about @covers annotation usage'
complete -f -c phpunit -l strict-global-state -d 'Be strict about changes to global state'
complete -f -c phpunit -l disallow-test-output -d 'Be strict about output during tests'
complete -f -c phpunit -l disallow-resource-usage -d 'Be strict about resource usage during small tests'
complete -f -c phpunit -l enforce-time-limit -d 'Enforce time limit based on test size'
complete -f -c phpunit -l default-time-limit -d 'Timeout in seconds for tests without @small, @medium or @large'
complete -f -c phpunit -l disallow-todo-tests -d 'Disallow @todo-annotated tests'

complete -f -c phpunit -l process-isolation -d 'Run each test in a separate PHP process'
complete -f -c phpunit -l globals-backup -d 'Backup and restore $GLOBALS for each test'
complete -f -c phpunit -l static-backup -d 'Backup and restore static attributes for each test'

complete -f -c phpunit -l colors -a 'never auto always' -d 'Use colors in output'
complete -x -c phpunit -l columns -a max -d 'Number of columns to use for progress output'
complete -f -c phpunit -l stderr -d 'Write to STDERR instead of STDOUT'
complete -f -c phpunit -l stop-on-defect -d 'Stop execution upon first not-passed test'
complete -f -c phpunit -l stop-on-error -d 'Stop execution upon first error'
complete -f -c phpunit -l stop-on-failure -d 'Stop execution upon first error or failure'
complete -f -c phpunit -l stop-on-warning -d 'Stop execution upon first warning'
complete -f -c phpunit -l stop-on-risky -d 'Stop execution upon first risky test'
complete -f -c phpunit -l stop-on-skipped -d 'Stop execution upon first skipped test'
complete -f -c phpunit -l stop-on-incomplete -d 'Stop execution upon first incomplete test'
complete -f -c phpunit -l fail-on-warning -d 'Treat tests with warnings as failures'
complete -f -c phpunit -l fail-on-risky -d 'Treat risky tests as failures'
complete -f -c phpunit -s v -l verbose -d 'Output more verbose information'
complete -f -c phpunit -l debug -d 'Display debugging information'

complete -x -c phpunit -l loader -d 'TestSuiteLoader implementation to use'
complete -x -c phpunit -l repeat -d 'Runs the test(s) repeatedly'
complete -f -c phpunit -l teamcity -d 'Report test execution progress in TeamCity format'
complete -f -c phpunit -l testdox -d 'Report test execution progress in TestDox format'
complete -f -c phpunit -l testdox-group -d 'Only include tests from the specified group(s)'
complete -f -c phpunit -l testdox-exclude-group -d 'Exclude tests from the specified group(s)'
complete -x -c phpunit -l no-interaction -d 'Disable TestDox progress animation'
complete -x -c phpunit -l printer -d 'TestListener implementation to use'

complete -x -c phpunit -l order-by -a 'default defects duration no-depends random reverse size' -d 'Run tests in order'
complete -x -c phpunit -l random-order-seed -d 'Use a specific random seed for random order'
complete -f -c phpunit -l cache-result -d 'Write test results to cache file'
complete -f -c phpunit -l do-not-cache-result -d 'Do not write test results to cache file'

# Configuration Options:
complete -x -c phpunit -l prepend -d 'A PHP script that is included as early as possible'
complete -x -c phpunit -l bootstrap -d 'A PHP script that is included before the tests run'
complete -x -c phpunit -s c -l configuration -k -a '(__fish_complete_suffix .xml .xml.dist)' -d 'Read configuration from XML file'
complete -f -c phpunit -l no-configuration -d 'Ignore default configuration file (phpunit.xml)'
complete -f -c phpunit -l no-logging -d 'Ignore logging configuration'
complete -f -c phpunit -l no-extensions -d 'Do not load PHPUnit extensions'
complete -f -c phpunit -l include-path -d 'Prepend PHP\'s include_path with given path(s)'
complete -x -c phpunit -s d -d 'Sets a php.ini value in the format key[=value]'
complete -f -c phpunit -l generate-configuration -d 'Generate configuration file with suggested settings'
complete -x -c phpunit -l cache-result-file -d 'Specify result cache path and filename'

# Miscellaneous Options:
complete -f -c phpunit -s h -l help -d 'Prints usage information'
complete -f -c phpunit -l version -d 'Prints the version and exits'
complete -x -c phpunit -l atleast-version -d 'Checks that version is greater than min and exits'
complete -f -c phpunit -l check-version -d 'Check whether PHPUnit is the latest version'
