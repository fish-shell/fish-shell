### Auto-complete for sysbench (cross-platform and multi-threaded benchmark tool) ###

### sub commands specification ###
complete -c sysbench -f -a "run\t'Run the test'"
complete -c sysbench -n "__fish_contains_opt test=fileio" -a "
	prepare\t'Prepare and create test file'
	cleanup\t'Cleanup test files'
	"
complete -c sysbench -n "__fish_contains_opt test=oltp" -a "
	prepare\t'Prepare test table'
	cleanup\t'Cleanup test table'
	"

### generic long options specification ###
complete -c sysbench -x -l num-threads -d 'The total number of worker threads to create (default: 1)'
complete -c sysbench -x -l max-requests -d 'Limit for total number of requests. 0 means unlimited (default: 10000)'
complete -c sysbench -x -l max-time -d 'Limit for total execution time in seconds. 0 means unlimited (default: 0)'
complete -c sysbench -x -l thread-stack-size -d 'Size of stack for each thread (defaut: 32K)'
complete -c sysbench -f -l init-rng -d 'Specifies if random numbers generator should be initialized from timer (defaut: off)' -a 'on off'
complete -c sysbench -x -l test -d 'Name of the test mode to run(required)' -a "
	cpu\t'Benchmark cpu by calculating prime numbers'
	threads\t'Benchmark scheduler performance'
	mutex\t'Benchmark mutex implementation'
	fileio\t'Benchmark various file I/O workloads'
	oltp\t'Benchmark a real database performance'
	"
complete -c sysbench -f -l debug -d 'Print more debug info (default: off)' -a 'on off'
complete -c sysbench -f -l validate -d 'Perform validation of test results where possible (default: off)' -a 'on off'
complete -c sysbench -l help -d 'Print help on general syntax'
complete -c sysbench -l version -d 'Show version of program'
complete -c sysbench -x -l percentile -d 'A percentile rank of query execution times to count (default: 95)'
complete -c sysbench -f -l batch -d 'Dump current results periodically (default: off)' -a 'on off'
complete -c sysbench -x -l batch-delay -d 'Delay between batch dumps in secods (default: 300)'

### options for test=`cpu` mode ###
complete -c sysbench -n "__fish_contains_opt test=cpu" -x -l cpu-max-prime -d 'Calculation of prime numbers up to the specified value'

### options for test=`threads` mode ###
complete -c sysbench -n "__fish_contains_opt test=threads" -x -l thread-yields -d 'Number of lock/yield/unlock loops to execute per each request (default: 1000)'
complete -c sysbench -n "__fish_contains_opt test=threads" -x -l thread-locks -d 'Number of mutexes to create (default: 8)'

### options for test=`mutex` mode ###
complete -c sysbench -n "__fish_contains_opt test=mutex" -x -l mutex-num -d 'Number of mutexes to create (default: 4096)'
complete -c sysbench -n "__fish_contains_opt test=mutex" -x -l memory-scope -d 'Specifies whether each thread uses a global or local allocation (default:global)' -a "
	local\t'Allocate memory locally'
	global\t'Allocate memory globally'
	"
complete -c sysbench -n "__fish_contains_opt test=mutex" -x -l memory-total-size -d 'Total size of data to transfer (default: 100G)'
complete -c sysbench -n "__fish_contains_opt test=mutex" -x -l memory-oper -d 'Type of memory operations' -a 'read write'

### options for test=`fileio` mode ###
complete -c sysbench -n "__fish_contains_opt test=fileio" -x -l file-num -d 'Number of files to create (default: 128)'
complete -c sysbench -n "__fish_contains_opt test=fileio" -x -l file-block-size -d 'Block size to use in all I/O operations (default: 16K)'
complete -c sysbench -n "__fish_contains_opt test=fileio" -x -l file-total-size -d 'Total size of files (default: 2G)'
complete -c sysbench -n "__fish_contains_opt test=fileio" -x -l file-test-mode -d 'Type of workload to produce' -a "
	seqwr\t'Sequential write'
	seqrewr\t'Sequential rewrite'
	seqrd\t'Sequential read'
	rndrd\t'Random read'
	rndwr\t'Random write'
	rndrw\t'Random read/write'
	"
complete -c sysbench -n "__fish_contains_opt test=fileio" -x -l file-io-mode -d 'I/O mode (default: sync)' -a 'sync async fastmmap slowmmap'
complete -c sysbench -n "__fish_contains_opt test=fileio" -x -l file-async-backlog -d 'Number of asynchronous operations to queue per thread (default: 128)'
complete -c sysbench -n "__fish_contains_opt test=fileio" -x -l file-extra-flags -d 'Additional flags to use with open(2)'
complete -c sysbench -n "__fish_contains_opt test=fileio" -x -l file-fsync-freq -d 'Do fsync() after this number of requests (default: 0)'
complete -c sysbench -n "__fish_contains_opt test=fileio" -f -l file-fsync-all -d 'Do fsync() after each write operation (default: no)' -a 'yes no'
complete -c sysbench -n "__fish_contains_opt test=fileio" -f -l file-fsync-end -d 'Do fsync() at the end of the test (default: yes)' -a 'yes no'
complete -c sysbench -n "__fish_contains_opt test=fileio" -x -l file-fsync-mode -d 'Method used for synchronization: fsync, fdatasync (default: fsync)' -a 'fsync fdatasync'
complete -c sysbench -n "__fish_contains_opt test=fileio" -x -l file-merged-requests -d 'Upper limit of I/O requests merge (default: 0)'
complete -c sysbench -n "__fish_contains_opt test=fileio" -x -l file-rw-ratio -d 'reads/writes ratio for combined random read/write test (default: 1.5)'

### options for test=`oltp` mode ###
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l oltp-test-mode -d 'Execution mode: simple, complex and nontrx(non-transactional)(default: complex)' -a "
	simple\t'Simple'
	complex\t'Advanced transactional'
	nontrx\t'Non-transactional'
	"
complete -c sysbench -n "__fish_contains_opt test=oltp" -f -l oltp-read-only -d 'Read-only mode. No UPDATE, DELETE or INSERT queries will be performed. (default: off)' -a 'on off'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l oltp-range-size -d 'Range size for range queries (default: 100)'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l oltp-point-selects -d 'Number of point select queries in a single transaction (default: 10)'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l oltp-simple-ranges -d 'Number of simple range queries in a single transaction (default: 1)'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l oltp-sum-ranges -d 'Number of SUM range queries in a single transaction (default: 1)'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l oltp-order-ranges -d 'Number of ORDER range queries in a single transaction (default: 1)'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l oltp-distinct-ranges -d 'Number of DISTINCT range queries in a single transaction (default: 1)'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l oltp-index-updates -d 'Number of index UPDATE queries in a single transaction (default: 1)'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l oltp-non-index-updates -d 'Number of non-index UPDATE queries in a single transaction (default: 1)'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l oltp-nontrx-mode -d 'Type of queries for non-transactional execution mode (default: select)' -a 'select update_key update_nokey insert delete'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l oltp-connect-delay -d 'Time to sleep(in microseconds) after each connection to database (default: 10000)'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l oltp-user-delay-min -d 'Minimum time to sleep(in microseconds) after each request (default: 0)'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l oltp-user-delay-max -d 'Maximum time to sleep(in microseconds) after each request (default: 0)'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l oltp-table-name -d 'Name of the test table (default: sbtest)'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l oltp-table-size -d 'Number of rows in the test table (default: 10000)'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l oltp-dist-type -d 'Distribution type of random numbers (default: special)' -a "
	uniform\t'Uniform distribution'
	gauss\t'Gaussian distribution'
	special\t'Specified percent of numbers is generated in a specified percent of cases'
	"
complete -c sysbench -n "__fish_contains_opt oltp-dist-type=special" -x -l oltp-dist-pct -d 'Percentage of values to be treated as \'special\'(default: 1)'
complete -c sysbench -n "__fish_contains_opt oltp-dist-type=special" -x -l oltp-dist-res -d 'Percentage of cases when \'special\' values are generated (default: 75)'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l db-ps-mode -d 'Use "Prepared Statements" API if supported, otherwise - use clientside statements: disable, auto (default: auto)' -a 'disable auto'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l mysql-host -d 'MySQL server host (default: localhost)' -a "(__fish_print_hostnames)"
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l mysql-port -d 'MySQL server port (default: 3306)'
complete -c sysbench -n "__fish_contains_opt test=oltp" -l mysql-socket -d 'Unix socket file to communicate with the MySQL server'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l mysql-user -d 'MySQL user (default: sbtest)'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l mysql-password -d 'MySQL password'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l mysql-db -d 'MySQL database name (default: sbtest)'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l mysql-table-engine -d 'Type of the test table to use' -a 'myisam innodb heap ndbcluster bdb maria falcon pbxt'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l mysql-ssl -d 'Use SSL connections. (default: no)' -a 'yes no'
complete -c sysbench -n "__fish_contains_opt test=oltp" -x -l myisam-max-rows -d 'MAX_ROWS option for MyISAM tables (required for big tables) (default: 1000000)'
complete -c sysbench -n "__fish_contains_opt test=oltp" -l mysql-create-options -d 'Additional options passed to CREATE TABLE.'
