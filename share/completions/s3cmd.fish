# s3cmd

function __s3cmd_is_remote_path
    commandline -pct | string match -q -r -- "^s3://.*"
end

function __s3cmd_is_valid_remote_path
    commandline -pct | string match -q -r -- "^s3://.*?/.*"
end

# Completions to allow for autocomplete of remote paths
complete -c s3cmd -f -n __s3cmd_is_valid_remote_path -a "(s3cmd ls (commandline -ct) 2>/dev/null | string match -r -- 's3://.*')"
complete -c s3cmd -f -n __s3cmd_is_remote_path

# Suppress file completions for initial command
complete -c s3cmd -n "__fish_is_nth_token 1" -f

# Available commands
complete -c s3cmd -n "__fish_is_nth_token 1" -a mb -d 'Make bucket'
complete -c s3cmd -n "__fish_is_nth_token 1" -a rb -d 'Remove bucket'
complete -c s3cmd -n "__fish_is_nth_token 1" -a ls -d 'List objects or buckets'
complete -c s3cmd -n "__fish_is_nth_token 1" -a la -d 'List all object in all buckets'
complete -c s3cmd -n "__fish_is_nth_token 1" -a put -d 'Put file into bucket'
complete -c s3cmd -n "__fish_is_nth_token 1" -a get -d 'Get file from bucket'
complete -c s3cmd -n "__fish_is_nth_token 1" -a del -d 'Delete file from bucket'
complete -c s3cmd -n "__fish_is_nth_token 1" -a rm -d 'Delete file from bucket (alias for del)'
complete -c s3cmd -n "__fish_is_nth_token 1" -a restore -d 'Restore file from Glacier storage'
complete -c s3cmd -n "__fish_is_nth_token 1" -a sync -d 'Synchronize a directory tree to S3'
complete -c s3cmd -n "__fish_is_nth_token 1" -a du -d 'Disk usage by buckets'
complete -c s3cmd -n "__fish_is_nth_token 1" -a info -d 'Get various information about Buckets or Files'
complete -c s3cmd -n "__fish_is_nth_token 1" -a cp -d 'Copy object'
complete -c s3cmd -n "__fish_is_nth_token 1" -a modify -d 'Modify object metadata'
complete -c s3cmd -n "__fish_is_nth_token 1" -a mv -d 'Move object'
complete -c s3cmd -n "__fish_is_nth_token 1" -a setacl -d 'Modify Access control list for Bucket or Files'
complete -c s3cmd -n "__fish_is_nth_token 1" -a setpolicy -d 'Modify Bucket Policy'
complete -c s3cmd -n "__fish_is_nth_token 1" -a delpolicy -d 'Delete Bucket Policy'
complete -c s3cmd -n "__fish_is_nth_token 1" -a setcors -d 'Modify Bucket CORS'
complete -c s3cmd -n "__fish_is_nth_token 1" -a delcors -d 'Delete Bucket CORS'
complete -c s3cmd -n "__fish_is_nth_token 1" -a payer -d 'Modify Bucket Requester Pays policy'
complete -c s3cmd -n "__fish_is_nth_token 1" -a multipart -d 'Show multipart uploads'
complete -c s3cmd -n "__fish_is_nth_token 1" -a abortmp -d 'Abort a multipart upload'
complete -c s3cmd -n "__fish_is_nth_token 1" -a listmp -d 'List parts of a multipart upload'
complete -c s3cmd -n "__fish_is_nth_token 1" -a accesslog -d 'Enable/disable bucket access logging'
complete -c s3cmd -n "__fish_is_nth_token 1" -a sign -d 'Sign arbitrary string using the secret key'
complete -c s3cmd -n "__fish_is_nth_token 1" -a signurl -d 'Sign an S3 URL to provide limited public access with expiry'
complete -c s3cmd -n "__fish_is_nth_token 1" -a fixbucket -d 'Fix invalid file names in a bucket'
complete -c s3cmd -n "__fish_is_nth_token 1" -a ws-create -d 'Create Website from bucket'
complete -c s3cmd -n "__fish_is_nth_token 1" -a ws-delete -d 'Delete Website'
complete -c s3cmd -n "__fish_is_nth_token 1" -a ws-info -d 'Info about Website'
complete -c s3cmd -n "__fish_is_nth_token 1" -a expire -d 'Set or delete expiration rule for the bucket'
complete -c s3cmd -n "__fish_is_nth_token 1" -a setlifecycle -d 'Upload a lifecycle policy for the bucket'
complete -c s3cmd -n "__fish_is_nth_token 1" -a dellifecycle -d 'Remove a lifecycle policy for the bucket'
complete -c s3cmd -n "__fish_is_nth_token 1" -a cflist -d 'List CloudFront distribution points'
complete -c s3cmd -n "__fish_is_nth_token 1" -a cfinfo -d 'Display CloudFront distribution point parameters'
complete -c s3cmd -n "__fish_is_nth_token 1" -a cfcreate -d 'Create CloudFront distribution point'
complete -c s3cmd -n "__fish_is_nth_token 1" -a cfdelete -d 'Delete CloudFront distribution point'
complete -c s3cmd -n "__fish_is_nth_token 1" -a cfmodify -d 'Change CloudFront distribution point parameters'
complete -c s3cmd -n "__fish_is_nth_token 1" -a cfinvalinfo -d 'Display CloudFront invalidation request(s) status'

# Created against s3cmd version 2.0
complete -c s3cmd -s h -l help -d 'Show help and exit'
complete -c s3cmd -l configure -d 'Run interactive (re)configuration tool'
complete -c s3cmd -s c -l config -d 'Config file (default: $HOME/.s3cfg)'
complete -c s3cmd -l dump-config -d 'Dump current configuration'
complete -c s3cmd -l access_key -d 'AWS Access Key'
complete -c s3cmd -l secret_key -d 'AWS Secret Key'
complete -c s3cmd -l access_token -d 'AWS Access Token'
complete -c s3cmd -s n -l dry-run -d 'Dry run, test only'
complete -c s3cmd -s s -l ssl -d 'Use HTTPS (default)'
complete -c s3cmd -l no-ssl -d 'Don\'t use HTTPS'
complete -c s3cmd -s e -l encrypt -d 'Encrypt files before uploading'
complete -c s3cmd -l no-encrypt -d 'Don\'t encrypt files'
complete -c s3cmd -s f -l force -d 'Force overwrite'
complete -c s3cmd -l continue -d 'Resume partially downloaded file' -n "__fish_seen_subcommand_from get"
complete -c s3cmd -l continue-put -d 'Resume partially uploaded files'
complete -c s3cmd -l upload-id -d 'Resume multipart upload by UploadId'
complete -c s3cmd -l skip-existing -d 'Skip existing files at destination' -n "__fish_seen_subcommand_from get sync"
complete -c s3cmd -s r -l recursive -d 'Upload/download/delete recursively'
complete -c s3cmd -l check-md5 -d 'Check MD5 sums (default)' -n "__fish_seen_subcommand_from sync"
complete -c s3cmd -l no-check-md5 -d 'Skip MD5 sum check' -n "__fish_seen_subcommand_from sync"
complete -c s3cmd -s P -l acl-public -d 'Store with ACL read for all'
complete -c s3cmd -l acl-private -d 'Store with private ACL'
complete -c s3cmd -l acl-grant -d 'Grant permission to named user'
complete -c s3cmd -l acl-revoke -d 'Revoke permission to named user'
complete -c s3cmd -s D -l restore-days -d 'Days to keep restored file'
complete -c s3cmd -l restore-priority -d 'S3 glacier restore priority'
complete -c s3cmd -l delete-removed -d 'Delete objects not found locally' -n "__fish_seen_subcommand_from sync"
complete -c s3cmd -l no-delete-removed -d 'Don\'t delete dest objects'
complete -c s3cmd -l delete-after -d 'Delete after upload' -n "__fish_seen_subcommand_from sync"
complete -c s3cmd -l max-delete -d 'Delete no more than NUM files' -n "__fish_seen_subcommand_from del sync"
complete -c s3cmd -l limit -d 'Max objects per response' -n "__fish_seen_subcommand_from ls la"
complete -c s3cmd -l add-destination -d 'Additional parallel upload'
complete -c s3cmd -l delete-after-fetch -d 'Delete remotely after fetch' -n "__fish_seen_subcommand_from get sync"
complete -c s3cmd -s p -l preserve -d 'Preserve FS attributes'
complete -c s3cmd -l no-preserve -d 'Don\'t store FS attributes'
complete -c s3cmd -l exclude -d 'Exclude GLOB matches'
complete -c s3cmd -l exclude-from -d '--exclude GLOBs from FILE'
complete -c s3cmd -l rexclude -d 'Exclude REGEXP matches'
complete -c s3cmd -l rexclude-from -d 'Read --rexclude REGEXPs from FILE'
complete -c s3cmd -l include -d 'Include GLOB matches even if previously excluded'
complete -c s3cmd -l include-from -d 'Read --include GLOBs from FILE'
complete -c s3cmd -l rinclude -d 'Include REGEXP matches even if preiously excluded'
complete -c s3cmd -l rinclude-from -d 'Read --rinclude REGEXPs from FILE'
complete -c s3cmd -l files-from -d 'Read source-file names from FILE'
complete -c s3cmd -l region -l bucket-location -d 'Create bucket in region'
complete -c s3cmd -l host -d 'S3 endpoint (default: s3.amazonaws.com)'
complete -c s3cmd -l host-bucket -d 'DNS-style bucket+hostname:port template for bucket'
complete -c s3cmd -l reduced-redundancy -l rr -d 'Store with reduced redundancy' -n "__fish_seen_subcommand_from put cp mv"
complete -c s3cmd -l no-reduced-redundancy -l no-rr -d 'Store without reduced redundancy' -n "__fish_seen_subcommand_from put cp mv"
complete -c s3cmd -l storage-class -d 'Store with STANDARD, STANDARD_IA, or REDUCED_REDUNDANCY'
complete -c s3cmd -l access-logging-target-prefix -d 'Prefix for access logs' -n "__fish_seen_subcommand_from cfmodify accesslog"
complete -c s3cmd -l no-access-logging -d 'Disable access logging' -n "__fish_seen_subcommand_from cfmodify accesslog"
complete -c s3cmd -l default-mime-type -d 'Default MIME-type for objects'
complete -c s3cmd -s M -l guess-mime-type -d 'Guess MIME-type'
complete -c s3cmd -l no-guess-mime-type -d 'Don\'t guess MIME-type, use default'
complete -c s3cmd -l no-mime-magic -d 'Don\'t use mime magic when guessing'
complete -c s3cmd -s m -l mime-type -d 'Force MIME-type'
complete -c s3cmd -l add-header -d 'Add HTTP header'
complete -c s3cmd -l remove-header -d 'Remove HTTP header'
complete -c s3cmd -l server-side-encryption -d 'Use server-side encryption for upload'
complete -c s3cmd -l server-side-encryption-kms-id -d 'Encrypt with specified AWS KMS-managed key'
complete -c s3cmd -l encoding -d 'Use specified encoding'
complete -c s3cmd -l add-encoding-exts -d 'Add encoding to CSV extension list'
complete -c s3cmd -l verbatim -d 'Use S3 name as-is'
complete -c s3cmd -l disable-multipart -d 'No multipart on files larger than --multipart-chunk-size-mb'
complete -c s3cmd -l multipart-chunk-size-mb -d 'Multipart upload chunk size'
complete -c s3cmd -l list-md5 -d 'Include MD5 sums in bucket listings' -n "__fish_seen_subcommand_from ls"
complete -c s3cmd -s H -l human-readable-sizes -d 'Print sizes in human-readable form'
complete -c s3cmd -l ws-index -d 'Name of index-document' -n "__fish_seen_subcommand_from ws-create"
complete -c s3cmd -l ws-error -d 'Name of error-document' -n "__fish_seen_subcommand_from ws-create"
complete -c s3cmd -l expiry-date -d 'When expiration rule takes effect' -n "__fish_seen_subcommand_from expire"
complete -c s3cmd -l expiry-days -d 'Days to expire' -n "__fish_seen_subcommand_from expire"
complete -c s3cmd -l expiry-prefix -d 'Apply expiry to objects matching prefix' -n "__fish_seen_subcommand_from expire"
complete -c s3cmd -l progress -d 'Show progress (default on TTY)'
complete -c s3cmd -l no-progress -d 'Don\'t show progress meter (default if non-TTY)'
complete -c s3cmd -l stats -d 'Show file transfer stats'
complete -c s3cmd -l enable -d 'Enable CloudFront distribution' -n "__fish_seen_subcommand_from cfmodify"
complete -c s3cmd -l disable -d 'Disable CloudFront distribution' -n "__fish_seen_subcommand_from cfmodify"
complete -c s3cmd -l cf-invalidate -d 'Invalidate CloudFront file' -n "__fish_seen_subcommand_from cfmodify"
complete -c s3cmd -l cf-invalidate-default-index -d 'Invalidate default index' -n "__fish_seen_subcommand_from cfmodify"
complete -c s3cmd -l cf-no-invalidate-default-index-root -d 'Don\'t invalidate default index' -n "__fish_seen_subcommand_from cfmodify"
complete -c s3cmd -l cf-add-cname -d 'Add CNAME to CloudFront distribution' -n "__fish_seen_subcommand_from cfcreate cfmodify"
complete -c s3cmd -l cf-remove-cname -d 'Remove CNAME from CloudFront distribution' -n "__fish_seen_subcommand_from cfmodify"
complete -c s3cmd -l cf-comment -d 'Set COMMENT for CloudFront distribution' -n "__fish_seen_subcommand_from cfcreate cfmodify"
complete -c s3cmd -l cf-default-root-object -d 'Set default root object' -n "__fish_seen_subcommand_from cfcreate cfmodify"
complete -c s3cmd -s v -l verbose -d 'Verbose output'
complete -c s3cmd -s d -l debug -d 'Debug output'
complete -c s3cmd -l version -d 'Show version'
complete -c s3cmd -s F -l follow-symlinks -d 'Follow symlinks'
complete -c s3cmd -l cache-file -d 'Cache FILE containing MD5 values'
complete -c s3cmd -s q -l quiet -d 'Silence stdout output'
complete -c s3cmd -l ca-certs -d 'Path to SSL CA certificate FILE'
complete -c s3cmd -l check-certificate -d 'Validate SSL certificate'
complete -c s3cmd -l no-check-certificate -d 'Don\'t validate SSL certificate'
complete -c s3cmd -l check-hostname -d 'Validate SSL hostname'
complete -c s3cmd -l no-check-hostname -d 'Don\'t validate SSL hostname'
complete -c s3cmd -l signature-v2 -d 'Use AWS Signature version 2'
complete -c s3cmd -l limit-rate -d 'Limit upload or download speed (bytes/sec)'
complete -c s3cmd -l requester-pays -d 'Set REQUESTER PAYS for operations'
complete -c s3cmd -s l -l long-listing -d 'Produce long listing' -n "__fish_seen_subcommand_from ls"
complete -c s3cmd -l stop-on-error -d 'Stop on error in transfer'
complete -c s3cmd -l content-disposition -d 'Provide Content-Disposition for signed URLs'
complete -c s3cmd -l content-type -d 'Provide Content-Type for signed URLs'
