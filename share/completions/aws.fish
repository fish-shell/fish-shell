# These are very much incomplete completions for the aws-cli suite.
# In addition to a complete list of services, the `aws s3` completions are mostly complete
# (and are the primary reason this file exists). The automatically generated completions
# for `aws` via `fish_update_completions` are pretty useless due to non-standard formatting.

function __s3_is_maybe_bucket
    commandline -ct | string match -qr -- '^(|s|s3|s3:?/?/?[^/]*)$'
end

function __s3_is_bucket
    commandline -ct | string match -q -r -- '^s3:/?/?[^/]*$'
end

function __s3_is_remote_path
    commandline -ct | string match -q -r -- "^s3://.+/.*"
end

function __s3_ls_buckets
    aws s3 ls | string replace -rf '.* (\S+)$' 's3://$1/'
end

function __s3_ls_dir
    set -l dir (commandline -ct | string replace -rf '(s3://.*/).*' '$1')
    printf "$dir%s\n" (aws s3 ls $dir 2>/dev/null | string replace -fr '^(:?\S+ +\S+ +\S+ |^.*PRE )(.*)' '$2')
end

# Determines whether the first non-switch argument to `aws s3` was in $argv
# This accounts for things like `aws --debug s3 foo ... s3://...`
function __s3_cmd_in
    set -l is_s3 0
    set -l tokens (commandline -cx)
    for token in $tokens[2..-1]
        # Ignore switches everywhere
        if string match -qr -- "^--" $token
            continue
        end

        # Check if `aws` command is `s3`
        if test $is_s3 -eq 0
            if string match -q -- s3 $token
                set is_s3 1
                continue
            else
                return 1
            end
        end

        # Check if `aws s3` sub-sub-command is in $argv
        if contains $token $argv
            return 0
        else
            return 1
        end
    end

    return 1
end

# S3 completions
complete -c aws -n "__fish_prev_arg_in s3" -xa "cp mv rm help sync ls mb mv presign rb website"

# When completing a remote path, complete the bucket name first, then based off
# the bucket name, we can complete the path itself.

# Commands that take only remote parameters (cannot operate on local files).
complete -c aws -n "__s3_is_maybe_bucket && __s3_cmd_in ls rb rm" -xa "(__s3_ls_buckets)"
# Commands that can operate on local or remote files. To prevent the shell
# locking up unnecessarily, only complete if no argument was specified or if the
# argument being specified could be an S3 path.
complete -c aws -n "__s3_is_maybe_bucket && __s3_cmd_in mv cp presign mb sync" -a "(__s3_ls_buckets)"

# Complete the paths themselves
complete -c aws -n __s3_is_remote_path -xa "(__s3_ls_dir)"
complete -c aws -n __s3_is_bucket -xa "(__s3_ls_buckets)"

# This list is extracted from the output of `aws help`, which can't be ingested directly,
# as it emits considerable ANSI output and other terminal control characters.
set -l aws_services \
    acm \
    acm-pca \
    alexaforbusiness \
    amplify \
    apigateway \
    apigatewaymanagementapi \
    apigatewayv2 \
    application-autoscaling \
    appmesh \
    appstream \
    appsync \
    athena \
    autoscaling \
    autoscaling-plans \
    backup \
    batch \
    budgets \
    ce \
    chime \
    cloud9 \
    clouddirectory \
    cloudformation \
    cloudfront \
    cloudhsm \
    cloudhsmv2 \
    cloudsearch \
    cloudsearchdomain \
    cloudtrail \
    cloudwatch \
    codebuild \
    codecommit \
    codepipeline \
    codestar \
    cognito-identity \
    cognito-idp \
    cognito-sync \
    comprehend \
    comprehendmedical \
    configservice \
    configure \
    connect \
    cur \
    datapipeline \
    datasync \
    dax \
    deploy \
    devicefarm \
    directconnect \
    discovery \
    dlm \
    dms \
    docdb \
    ds \
    dynamodb \
    dynamodbstreams \
    ec2 \
    ecr \
    ecs \
    efs \
    eks \
    elasticache \
    elasticbeanstalk \
    elastictranscoder \
    elb \
    elbv2 \
    emr \
    es \
    events \
    firehose \
    fms \
    fsx \
    gamelift \
    glacier \
    globalaccelerator \
    glue \
    greengrass \
    guardduty \
    health \
    help \
    history \
    iam \
    importexport \
    inspector \
    iot \
    iot-data \
    iot-jobs-data \
    iot1click-devices \
    iot1click-projects \
    iotanalytics \
    kafka \
    kinesis \
    kinesis-video-archived-media \
    kinesis-video-media \
    kinesisanalytics \
    kinesisanalyticsv2 \
    kinesisvideo \
    kms \
    lambda \
    lex-models \
    lex-runtime \
    license-manager \
    lightsail \
    logs \
    machinelearning \
    macie \
    marketplace-entitlement \
    marketplacecommerceanalytics \
    mediaconnect \
    mediaconvert \
    medialive \
    mediapackage \
    mediastore \
    mediastore-data \
    mediatailor \
    meteringmarketplace \
    mgh \
    mobile \
    mq \
    mturk \
    neptune \
    opsworks \
    opsworks-cm \
    organizations \
    pi \
    pinpoint \
    pinpoint-email \
    pinpoint-sms-voice \
    polly \
    pricing \
    quicksight \
    ram \
    rds \
    rds-data \
    redshift \
    rekognition \
    resource-groups \
    resourcegroupstaggingapi \
    robomaker \
    route53 \
    route53domains \
    route53resolver \
    s3 \
    s3api \
    s3control \
    sagemaker \
    sagemaker-runtime \
    sdb \
    secretsmanager \
    securityhub \
    serverlessrepo \
    servicecatalog \
    servicediscovery \
    ses \
    shield \
    signer \
    sms \
    snowball \
    sns \
    sqs \
    ssm \
    stepfunctions \
    storagegateway \
    sts \
    support \
    swf \
    textract \
    transcribe \
    transfer \
    translate \
    waf \
    waf-regional \
    workdocs \
    worklink \
    workmail \
    workspaces \
    xray

complete -c aws -n "__fish_is_nth_token 1" -xa "$aws_services"
