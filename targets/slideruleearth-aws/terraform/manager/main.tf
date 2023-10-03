data "archive_file" "zip_the_python_code" {
    type            = "zip"
    source_dir      = "${path.module}/python/"
    output_path     = "${path.module}/python/manager.zip"
}

resource "aws_lambda_function" "terraform_lambda_func" {
    filename        = "${path.module}/python/manager.zip"
    function_name   = "Certbot_Runner"
    role            = aws_iam_role.lambda.arn
    handler         = "index.certbot_handler"
    runtime         = "python3.8"
    depends_on      = [aws_iam_role_policy_attachment.s3, aws_iam_role_policy_attachment.certbot]
}
