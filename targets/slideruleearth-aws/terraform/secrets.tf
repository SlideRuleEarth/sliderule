data "aws_secretsmanager_secret_version" "secrets" {
  secret_id = "${var.domain}/secrets"
}

locals {
  secrets = jsondecode(
    data.aws_secretsmanager_secret_version.secrets.secret_string
  )
}
