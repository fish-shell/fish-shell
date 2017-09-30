# s3cmd

function __s3cmd_is_remote_path
	commandline -pct | string match -q -r -- "^s3://.*"
end

function __s3cmd_is_valid_remote_path
	commandline -pct | string match -q -r -- "^s3://.*?/.*"
end

# Completions to allow for autocomplete of remote paths
complete -c s3cmd -f -n "__s3cmd_is_valid_remote_path" -a "(s3cmd ls (commandline -ct) 2>/dev/null | string match -r -- 's3://.*')"
complete -c s3cmd -f -n "__s3cmd_is_remote_path"

# Supress file completions for initial command
complete -c s3cmd -n "__fish_is_first_token" -f

# Available commands
complete -c s3cmd -n "__fish_is_first_token" -a 'mb' --description 'Make bucket'
complete -c s3cmd -n "__fish_is_first_token" -a 'rb' --description 'Remove bucket'
complete -c s3cmd -n "__fish_is_first_token" -a 'ls' --description 'List objects or buckets'
complete -c s3cmd -n "__fish_is_first_token" -a 'la' --description 'List all object in all buckets'
complete -c s3cmd -n "__fish_is_first_token" -a 'put' --description 'Put file into bucket'
complete -c s3cmd -n "__fish_is_first_token" -a 'get' --description 'Get file from bucket'
complete -c s3cmd -n "__fish_is_first_token" -a 'del' --description 'Delete file from bucket'
complete -c s3cmd -n "__fish_is_first_token" -a 'rm' --description 'Delete file from bucket (alias for del)'
complete -c s3cmd -n "__fish_is_first_token" -a 'restore' --description 'Restore file from Glacier storage'
complete -c s3cmd -n "__fish_is_first_token" -a 'sync' --description 'Synchronize a directory tree to S3'
complete -c s3cmd -n "__fish_is_first_token" -a 'du' --description 'Disk usage by buckets'
complete -c s3cmd -n "__fish_is_first_token" -a 'info' --description 'Get various information about Buckets or Files'
complete -c s3cmd -n "__fish_is_first_token" -a 'cp' --description 'Copy object'
complete -c s3cmd -n "__fish_is_first_token" -a 'modify' --description 'Modify object metadata'
complete -c s3cmd -n "__fish_is_first_token" -a 'mv' --description 'Move object'
complete -c s3cmd -n "__fish_is_first_token" -a 'setacl' --description 'Modify Access control list for Bucket or Files'
complete -c s3cmd -n "__fish_is_first_token" -a 'setpolicy' --description 'Modify Bucket Policy'
complete -c s3cmd -n "__fish_is_first_token" -a 'delpolicy' --description 'Delete Bucket Policy'
complete -c s3cmd -n "__fish_is_first_token" -a 'setcors' --description 'Modify Bucket CORS'
complete -c s3cmd -n "__fish_is_first_token" -a 'delcors' --description 'Delete Bucket CORS'
complete -c s3cmd -n "__fish_is_first_token" -a 'payer' --description 'Modify Bucket Requester Pays policy'
complete -c s3cmd -n "__fish_is_first_token" -a 'multipart' --description 'Show multipart uploads'
complete -c s3cmd -n "__fish_is_first_token" -a 'abortmp' --description 'Abort a multipart upload'
complete -c s3cmd -n "__fish_is_first_token" -a 'listmp' --description 'List parts of a multipart upload'
complete -c s3cmd -n "__fish_is_first_token" -a 'accesslog' --description 'Enable/disable bucket access logging'
complete -c s3cmd -n "__fish_is_first_token" -a 'sign' --description 'Sign arbitrary string using the secret key'
complete -c s3cmd -n "__fish_is_first_token" -a 'signurl' --description 'Sign an S3 URL to provide limited public access with expiry'
complete -c s3cmd -n "__fish_is_first_token" -a 'fixbucket' --description 'Fix invalid file names in a bucket'
complete -c s3cmd -n "__fish_is_first_token" -a 'ws-create' --description 'Create Website from bucket'
complete -c s3cmd -n "__fish_is_first_token" -a 'ws-delete' --description 'Delete Website'
complete -c s3cmd -n "__fish_is_first_token" -a 'ws-info' --description 'Info about Website'
complete -c s3cmd -n "__fish_is_first_token" -a 'expire' --description 'Set or delete expiration rule for the bucket'
complete -c s3cmd -n "__fish_is_first_token" -a 'setlifecycle' --description 'Upload a lifecycle policy for the bucket'
complete -c s3cmd -n "__fish_is_first_token" -a 'dellifecycle' --description 'Remove a lifecycle policy for the bucket'
complete -c s3cmd -n "__fish_is_first_token" -a 'cflist' --description 'List CloudFront distribution points'
complete -c s3cmd -n "__fish_is_first_token" -a 'cfinfo' --description 'Display CloudFront distribution point parameters'
complete -c s3cmd -n "__fish_is_first_token" -a 'cfcreate' --description 'Create CloudFront distribution point'
complete -c s3cmd -n "__fish_is_first_token" -a 'cfdelete' --description 'Delete CloudFront distribution point'
complete -c s3cmd -n "__fish_is_first_token" -a 'cfmodify' --description 'Change CloudFront distribution point parameters'
complete -c s3cmd -n "__fish_is_first_token" -a 'cfinvalinfo' --description 'Display CloudFront invalidation request(s) status'

# Created against s3cmd version 2.0
complete -c s3cmd -s h -l help --description 'Show help and exit'
complete -c s3cmd -l configure --description 'Run interactive (re)configuration tool'
complete -c s3cmd -s c -l config --description 'Config file (default: $HOME/.s3cfg)'
complete -c s3cmd -l dump-config --description 'Dump current configuration'
complete -c s3cmd -l access_key --description 'AWS Access Key'
complete -c s3cmd -l secret_key --description 'AWS Secret Key'
complete -c s3cmd -l access_token --description 'AWS Access Token'
complete -c s3cmd -s n -l dry-run --description 'Dry run, test only'
complete -c s3cmd -s s -l ssl --description 'Use HTTPS (default)'
complete -c s3cmd -l no-ssl --description 'Don\'t use HTTPS'
complete -c s3cmd -s e -l encrypt --description 'Encrypt files before uploading'
complete -c s3cmd -l no-encrypt --description 'Don\'t encrypt files'
complete -c s3cmd -s f -l force --description 'Force overwrite'
complete -c s3cmd -l continue --description 'Resume partially downloaded file' -n "__fish_seen_subcommand_from get"
complete -c s3cmd -l continue-put --description 'Resume partially uploaded files'
complete -c s3cmd -l upload-id --description 'Resume multipart upload by UploadId'
complete -c s3cmd -l skip-existing --description 'Skip existing files at destination' -n "__fish_seen_subcommand_from get sync"
complete -c s3cmd -s r -l recursive --description 'Upload/download/delete recursively'
complete -c s3cmd -l check-md5 --description 'Check MD5 sums (default)' -n "__fish_seen_subcommand_from sync"
complete -c s3cmd -l no-check-md5 --description 'Skip MD5 sum check' -n "__fish_seen_subcommand_from sync"
complete -c s3cmd -s P -l acl-public --description 'Store with ACL read for all'
complete -c s3cmd -l acl-private --description 'Store with private ACL'
complete -c s3cmd -l acl-grant --description 'Grant permission to named user'
complete -c s3cmd -l acl-revoke --description 'Revoke permission to named user'
complete -c s3cmd -s D -l restore-days --description 'Days to keep restored file'
complete -c s3cmd -l restore-priority --description 'S3 glacier restore priority'
complete -c s3cmd -l delete-removed --description 'Delete objects not found locally' -n "__fish_seen_subcommand_from sync"
complete -c s3cmd -l no-delete-removed --description 'Don\'t delete dest objects'
complete -c s3cmd -l delete-after --description 'Delete after upload' -n "__fish_seen_subcommand_from sync"
complete -c s3cmd -l max-delete --description 'Delete no more than NUM files' -n "__fish_seen_subcommand_from del sync"
complete -c s3cmd -l limit --description 'Max objects per response' -n "__fish_seen_subcommand_from ls la"
complete -c s3cmd -l add-destination --description 'Additional parallel upload'
complete -c s3cmd -l delete-after-fetch --description 'Delete remotely after fetch' -n "__fish_seen_subcommand_from get sync"
complete -c s3cmd -s p -l preserve --description 'Preserve FS attributes'
complete -c s3cmd -l no-preserve --description 'Don\'t store FS attributes'
complete -c s3cmd -l exclude --description 'Exclude GLOB matches'
complete -c s3cmd -l exclude-from --description '--exclude GLOBs from FILE'
complete -c s3cmd -l rexclude --description 'Exclude REGEXP matches'
complete -c s3cmd -l rexclude-from --description 'Read --rexclude REGEXPs from FILE'
complete -c s3cmd -l include --description 'Include GLOB matches even if previously excluded'
complete -c s3cmd -l include-from --description 'Read --include GLOBs from FILE'
complete -c s3cmd -l rinclude --description 'Include REGEXP matches even if preiously excluded'
complete -c s3cmd -l rinclude-from --description 'Read --rinclude REGEXPs from FILE'
complete -c s3cmd -l files-from --description 'Read source-file names from FILE'
complete -c s3cmd -l region -l bucket-location --description 'Create bucket in region'
complete -c s3cmd -l host --description 'S3 endpoint (default: s3.amazonaws.com)'
complete -c s3cmd -l host-bucket --description 'DNS-style bucket+hostname:port template for bucket'
complete -c s3cmd -l reduced-redundancy -l rr --description 'Store with reduced redundancy' -n "__fish_seen_subcommand_from put cp mv"
complete -c s3cmd -l no-reduced-redundancy -l no-rr --description 'Store without reduced redundancy' -n "__fish_seen_subcommand_from put cp mv"
complete -c s3cmd -l storage-class --description 'Store with STANDARD, STANDARD_IA, or REDUCED_REDUNDANCY'
complete -c s3cmd -l access-logging-target-prefix --description 'Prefix for access logs' -n "__fish_seen_subcommand_from cfmodify accesslog"
complete -c s3cmd -l no-access-logging --description 'Disable access logging' -n "__fish_seen_subcommand_from cfmodify accesslog"
complete -c s3cmd -l default-mime-type --description 'Default MIME-type for objects'
complete -c s3cmd -s M -l guess-mime-type --description 'Guess MIME-type'
complete -c s3cmd -l no-guess-mime-type --description 'Don\'t guess MIME-type, use default'
complete -c s3cmd -l no-mime-magic --description 'Don\'t use mime magic when guessing'
complete -c s3cmd -s m -l mime-type --description 'Force MIME-type'
complete -c s3cmd -l add-header --description 'Add HTTP header'
complete -c s3cmd -l remove-header --description 'Remove HTTP header'
complete -c s3cmd -l server-side-encryption --description 'Use server-side encryption for upload'
complete -c s3cmd -l server-side-encryption-kms-id --description 'Encrypt with specified AWS KMS-managed key'
complete -c s3cmd -l encoding --description 'Use specified encoding'
complete -c s3cmd -l add-encoding-exts --description 'Add encoding to CSV extension list'
complete -c s3cmd -l verbatim --description 'Use S3 name as-is'
complete -c s3cmd -l disable-multipart --description 'No multipart on files larger than --multipart-chunk-size-mb'
complete -c s3cmd -l multipart-chunk-size-mb --description 'Multipart upload chunk size'
complete -c s3cmd -l list-md5 --description 'Include MD5 sums in bucket listings' -n "__fish_seen_subcommand_from ls"
complete -c s3cmd -s H -l human-readable-sizes --description 'Print sizes in human-readable form'
complete -c s3cmd -l ws-index --description 'Name of index-document' -n "__fish_seen_subcommand_from ws-create"
complete -c s3cmd -l ws-error --description 'Name of error-document' -n "__fish_seen_subcommand_from ws-create"
complete -c s3cmd -l expiry-date --description 'When expiration rule takes effect' -n "__fish_seen_subcommand_from expire"
complete -c s3cmd -l expiry-days --description 'Days to expire' -n "__fish_seen_subcommand_from expire"
complete -c s3cmd -l expiry-prefix --description 'Apply expiry to objects matching prefix' -n "__fish_seen_subcommand_from expire"
complete -c s3cmd -l progress --description 'Show progress (default on TTY)'
complete -c s3cmd -l no-progress --description 'Don\'t show progress meter (default if non-TTY)'
complete -c s3cmd -l stats --description 'Show file transfer stats'
complete -c s3cmd -l enable --description 'Enable CloudFront distribution' -n "__fish_seen_subcommand_from cfmodify"
complete -c s3cmd -l disable --description 'Disable CloudFront distribution' -n "__fish_seen_subcommand_from cfmodify"
complete -c s3cmd -l cf-invalidate --description 'Invalidate CloudFront file' -n "__fish_seen_subcommand_from cfmodify"
complete -c s3cmd -l cf-invalidate-default-index --description 'Invalidate default index' -n "__fish_seen_subcommand_from cfmodify"
complete -c s3cmd -l cf-no-invalidate-default-index-root --description 'Don\'t invalidate default index' -n "__fish_seen_subcommand_from cfmodify"
complete -c s3cmd -l cf-add-cname --description 'Add CNAME to CloudFront distribution' -n "__fish_seen_subcommand_from cfcreate cfmodify"
complete -c s3cmd -l cf-remove-cname --description 'Remove CNAME from CloudFront distribution' -n "__fish_seen_subcommand_from cfmodify"
complete -c s3cmd -l cf-comment --description 'Set COMMENT for CloudFront distribution' -n "__fish_seen_subcommand_from cfcreate cfmodify"
complete -c s3cmd -l cf-default-root-object --description 'Set default root object' -n "__fish_seen_subcommand_from cfcreate cfmodify"
complete -c s3cmd -s v -l verbose --description 'Verbose output'
complete -c s3cmd -s d -l debug --description 'Debug output'
complete -c s3cmd -l version --description 'Show version'
complete -c s3cmd -s F -l follow-symlinks --description 'Follow symlinks'
complete -c s3cmd -l cache-file --description 'Cache FILE containing MD5 values'
complete -c s3cmd -s q -l quiet --description 'Silence stdout output'
complete -c s3cmd -l ca-certs --description 'Path to SSL CA certificate FILE'
complete -c s3cmd -l check-certificate --description 'Validate SSL certificate'
complete -c s3cmd -l no-check-certificate --description 'Don\'t validate SSL certificate'
complete -c s3cmd -l check-hostname --description 'Validate SSL hostname'
complete -c s3cmd -l no-check-hostname --description 'Don\'t validate SSL hostname'
complete -c s3cmd -l signature-v2 --description 'Use AWS Signature version 2'
complete -c s3cmd -l limit-rate --description 'Limit upload or download speed (bytes/sec)'
complete -c s3cmd -l requester-pays --description 'Set REQUESTER PAYS for operations'
complete -c s3cmd -s l -l long-listing --description 'Produce long listing' -n "__fish_seen_subcommand_from ls"
complete -c s3cmd -l stop-on-error --description 'Stop on error in transfer'
complete -c s3cmd -l content-disposition --description 'Provide Content-Disposition for signed URLs'
complete -c s3cmd -l content-type --description 'Provide Content-Type for signed URLs'
