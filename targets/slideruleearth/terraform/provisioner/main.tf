resource "aws_lambda_function" "provisioner" {
  function_name    = "Provisioner"
  role             = aws_iam_role.lambda.arn
  architectures    = ["arm64"]
  runtime          = "python3.11"
  memory_size      = 128 # MB
  timeout          = 300 # seconds
  handler          =  "main.lambda_handler"
  filename         = var.lambda_zipfile
  source_code_hash = filebase64sha256(var.lambda_zipfile)
  description      = "SlideRule Provisioner"

  environment {
    variables = {
      CONTAINER_REGISTRY = var.container_registry
    }
  }

  lifecycle {
    ignore_changes = [filename] # only update lambda function when souce_code_hash changes (the filename can be different)
  }
}
