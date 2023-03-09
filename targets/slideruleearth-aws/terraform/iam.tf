# IAM Role
resource "aws_iam_role" "s3-role" {
    name = "${var.cluster_name}-iam-role"
    assume_role_policy = file("assume-role-policy.json")
}

# IAM Profile
resource "aws_iam_instance_profile" "s3-role" {
    name = "${var.cluster_name}-iam-profile"
    role = aws_iam_role.s3-role.name
}

# Custom Managed S3 Policy
resource "aws_iam_policy" "s3-policy" {
  name   = "${var.cluster_name}-iams3-policy"
  policy = file("s3-policy.json")
}

# Custom Managed EC2 Policy
resource "aws_iam_policy" "ec2-policy" {
  name   = "${var.cluster_name}-iamec2-policy"
  policy = file("ec2-policy.json")
}

# Attach Custom S3 Policy
resource "aws_iam_role_policy_attachment" "s3-role-policy-local" {
  role       = aws_iam_role.s3-role.id
  policy_arn = aws_iam_policy.s3-policy.arn
}

# Attach Custom EC2 Policy
resource "aws_iam_role_policy_attachment" "ec2-role-policy-local" {
  role       = aws_iam_role.s3-role.id
  policy_arn = aws_iam_policy.ec2-policy.arn
}

# Attach AWS Managed Policies (for ECR access)
resource "aws_iam_role_policy_attachment" "ec2-role-policy-aec2crro" {
  role       = aws_iam_role.s3-role.id
  policy_arn = "arn:aws:iam::aws:policy/AmazonEC2ContainerRegistryReadOnly"
}

# Attach AWS Managed Policies (required by SMCE)
resource "aws_iam_role_policy_attachment" "ec2-role-policy-assmmic" {
  role       = aws_iam_role.s3-role.id
  policy_arn = "arn:aws:iam::aws:policy/AmazonSSMManagedInstanceCore"
}
resource "aws_iam_role_policy_attachment" "ec2-role-policy-cwasp" {
  role       = aws_iam_role.s3-role.id
  policy_arn = "arn:aws:iam::aws:policy/CloudWatchAgentServerPolicy"
}
resource "aws_iam_role_policy_attachment" "ec2-role-policy-cwaap" {
  role       = aws_iam_role.s3-role.id
  policy_arn = "arn:aws:iam::aws:policy/CloudWatchAgentAdminPolicy"
}
