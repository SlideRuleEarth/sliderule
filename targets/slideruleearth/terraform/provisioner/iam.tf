# IAM Role
resource "aws_iam_role" "lambda" {
    name = "provisioner-iam-role"
    assume_role_policy = file("assume-role-policy.json")
}

# Custom Managed Logs Policy
resource "aws_iam_policy" "logs" {
  name   = "provisioner-logs-iam-policy"
  policy = file("logs-policy.json")
}

# Custom Managed S3 Policy
resource "aws_iam_policy" "s3" {
  name   = "provisioner-s3-iam-policy"
  policy = file("s3-policy.json")
}

# Custom Managed Provisioner Policy
resource "aws_iam_policy" "provisioner" {
  name   = "provisioner-iam-policy"
  policy = file("provisioner-policy.json")
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

# Attach Custom Provisioner Policy
resource "aws_iam_role_policy_attachment" "provisioner" {
  role       = aws_iam_role.lambda.id
  policy_arn = aws_iam_policy.provisioner.arn
}
