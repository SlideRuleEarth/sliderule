output "lambda_arn" {
  value = aws_lambda_function.certbot.arn
}

output "lambda_code_hash" {
  value = aws_lambda_function.certbot.source_code_hash
}

output "lambda_code_size" {
  value = aws_lambda_function.certbot.source_code_size
}