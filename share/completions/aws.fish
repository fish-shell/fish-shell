# These are very much incomplete completions for the aws-cli suite.
# In addition to a complete list of services, the `aws s3` completions are mostly complete
# (and are the primary reason this file exists). The automatically generated completions
# for `aws` via `fish_update_completions` are pretty useless due to non-standard formatting.

function __s3_is_bucket
	commandline -ct | string match -q -r -- '^s3://[^/]?$'
end

function __s3_is_remote_path
	commandline -ct | string match -q -r -- "^s3://.*/.*"
end

function __s3_ls_buckets
	aws s3 ls | string replace -rf '.* (\S+)$' 's3://$1/'
end

function __s3_ls_dir
	set -l dir (commandline -ct | string replace -rf '(s3://.*/).*' '$1')
	printf "$dir%s\n" (aws s3 ls $dir 2>/dev/null | string replace -fr '^(:?\S+ +\S+ +\S+ |^.*PRE )(.*)' '$2')
end

# S3 remote completions
complete -c 'aws' -n "__fish_prev_arg_in s3" -xa "cp mv rm help sync ls mb mv presign rb website"
complete -c 'aws' -n "__s3_is_remote_path" -xa "(__s3_ls_dir)"
complete -c 'aws' -n "__s3_is_bucket" -xa "(__s3_ls_buckets)"
complete -c 'aws' -n "not __s3_is_remote_path && __fish_prev_arg_in ls" -xa "(__s3_ls_buckets)"
complete -c 'aws' -n "not __s3_is_remote_path && __fish_prev_arg_in mv cp rm" -a "(__s3_ls_buckets)"

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

complete -c aws -n '__fish_is_first_arg' -xa "$aws_services"
