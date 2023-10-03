# IAM Role
resource "aws_iam_role" "lambda" {
    name = "sliderule-manager-iam-role"
    assume_role_policy = file("assume-role-policy.json")
}

# Custom Managed Logs Policy
resource "aws_iam_policy" "logs" {
  name   = "sliderule-manager-logs-iam-policy"
  policy = file("logs-policy.json")
}

# Custom Managed S3 Policy
resource "aws_iam_policy" "s3" {
  name   = "sliderule-manager-s3-iam-policy"
  policy = file("s3-policy.json")
}

# Custom Managed Certbot Policy
resource "aws_iam_policy" "certbot" {
  name   = "sliderule-manager-certbot-iam-policy"
  policy = file("certbot-policy.json")
}

# Attach Custom Logs Policy
resource "aws_iam_role_policy_attachment" "logs" {
  role       = aws_iam_role.lambda.id
  policy_arn = aws_iam_policy.logs.arn
}

# Attach Custom S3 Policy
resource "aws_iam_role_policy_attachment" "s3" {
  role       = aws_iam_role.lambda.id
  policy_arn = aws_iam_policy.s3.arn
}

# Attach Custom Certbot Policy
resource "aws_iam_role_policy_attachment" "certbot" {
  role       = aws_iam_role.lambda.id
  policy_arn = aws_iam_policy.certbot.arn
}
