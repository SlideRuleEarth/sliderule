
# ---------------------------------------------------------------------------------------------------------------------
# ECS execution ROLE
# ---------------------------------------------------------------------------------------------------------------------
resource "aws_iam_role" "tasks-service-role" {
  name               = "${var.domain_root}-${var.Name_tag}-EcsTskSrvc" 
  path               = "/"
  assume_role_policy = data.aws_iam_policy_document.tasks-service-assume-policy.json
}

data "aws_iam_policy_document" "tasks-service-assume-policy" {
  statement {
    actions = ["sts:AssumeRole"]

    principals {
      type        = "Service"
      identifiers = ["ecs-tasks.amazonaws.com"]
    }
  }
}

resource "aws_iam_role_policy_attachment" "tasks-service-role-attachment" {
  role       = aws_iam_role.tasks-service-role.name
  policy_arn = "arn:aws:iam::aws:policy/service-role/AmazonECSTaskExecutionRolePolicy"
}
# ---------------------------------------------------------------------------------------------------------------------
# ECS task ROLE
# ---------------------------------------------------------------------------------------------------------------------
resource "aws_iam_role" "ecs_task_role" {
  name = "${var.domain_root}-${var.Name_tag}-EcsTsk"
 
  assume_role_policy = <<EOF
{
 "Version": "2012-10-17",
 "Statement": [
   {
     "Action": "sts:AssumeRole",
     "Principal": {
       "Service": "ecs-tasks.amazonaws.com"
     },
     "Effect": "Allow",
     "Sid": ""
   }
 ]
}
EOF
}
resource "aws_iam_policy" "static-web-fargate" {
  name        = "${var.domain_root}-${var.Name_tag}-tsk-fargate"
  description = "Policy that allows fargate logs"
 
 policy = <<EOF
{
   "Version": "2012-10-17",
   "Statement": [
       {
           "Effect": "Allow",
           "Action": [
                "cloudwatch:*",
                "logs:*"
           ],
           "Resource": "*"
       }
   ]
}
EOF
}

resource "aws_iam_role_policy_attachment" "ecs-tasks-service-role-policy-attachment" {
  role       = aws_iam_role.tasks-service-role.name
  policy_arn = aws_iam_policy.static-web-fargate.arn
} 