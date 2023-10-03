#data "archive_file" "zip_the_python_code" {
#    type            = "zip"
#    source_dir      = "${path.module}/python/"
#    output_path     = "${path.module}/python/manager.zip"
#}
#
#resource "aws_lambda_function" "terraform_lambda_func" {
#    filename        = "${path.module}/python/manager.zip"
#    function_name   = "Certbot_Runner"
#    role            = aws_iam_role.lambda.arn
#    handler         = "index.certbot_handler"
#    runtime         = "python3.8"
#    depends_on      = [aws_iam_role_policy_attachment.s3, aws_iam_role_policy_attachment.certbot]
#}


# ---------------------------------------------------------------------------------------------------------------------
# LAMBDA
# ---------------------------------------------------------------------------------------------------------------------

resource "aws_lambda_function" "certbot" {
  function_name    = var.lambda_name
  role             = aws_iam_role.lambda.arn
  runtime          = var.lambda_runtime
  memory_size      = var.lambda_memory_size
  timeout          = var.lambda_timeout
  handler          = local.lambda_handler
  filename         = local.lambda_filename
  source_code_hash = local.lambda_hash
  description      = local.lambda_description

  environment {
    variables = local.lambda_environment
  }
  # Ignore changes in path that happen when different people apply. We have source_code_hash to track actual code changes.
  lifecycle {
    ignore_changes = [filename]
  }
}

# ---------------------------------------------------------------------------------------------------------------------
# CLOUDWATCH EVENTS RULE THAT TRIGGERS ON A SCHEDULE
# ---------------------------------------------------------------------------------------------------------------------

resource "aws_cloudwatch_event_rule" "schedule" {
  name_prefix         = local.name_prefix
  description         = "Triggers lambda function ${aws_lambda_function.certbot.function_name} on a regular schedule."
  schedule_expression = "cron(${var.cron_expression})"
}

resource "aws_cloudwatch_event_target" "schedule" {
  rule = aws_cloudwatch_event_rule.schedule.name
  arn  = aws_lambda_function.certbot.arn
  # Since certbot lambda function gets all input from environment variables,
  # an empty JSON object is good enough.
  input = "{}"
}

resource "aws_lambda_permission" "schedule" {
  source_arn    = aws_cloudwatch_event_rule.schedule.arn
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.certbot.function_name
  principal     = "events.amazonaws.com"
}

# ---------------------------------------------------------------------------------------------------------------------
# LOCAL VARIABLES
# ---------------------------------------------------------------------------------------------------------------------

locals {
  name_prefix = "${var.lambda_name}-"

  aws_region     = data.aws_region.current.name
  aws_partition  = data.aws_arn.current.partition
  aws_account_id = data.aws_caller_identity.current.account_id

  certbot_emails  = join(",", var.emails)
  certbot_domains = join(",", var.domains)

  lambda_handler  = "main.lambda_handler"
  lambda_filename = var.lambda_filename
  lambda_hash     = filebase64sha256(local.lambda_filename)
  lambda_description = var.lambda_description

  lambda_environment = merge({
    EMAILS     = local.certbot_emails
    DOMAINS    = local.certbot_domains
    DNS_PLUGIN = var.certbot_dns_plugin
    S3_BUCKET  = var.upload_s3.bucket
    S3_PREFIX  = var.upload_s3.prefix
    S3_REGION  = var.upload_s3.region    
  }, var.lambda_custom_environment)
}