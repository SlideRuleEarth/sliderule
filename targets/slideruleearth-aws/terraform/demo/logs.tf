resource "aws_cloudwatch_log_group" "demo-log-group" {
  name              = "/${var.domain_root}-${var.Name_tag}/ecs/"
  retention_in_days = var.log_retention_in_days
  tags = {
    Name = "${var.domain_root}-${var.Name_tag}-LgGrp"
  }
}

resource "aws_cloudwatch_log_stream" "demo-log-stream" {
  name           = "${var.domain_root}-${var.Name_tag}-LgStrm"
  log_group_name = aws_cloudwatch_log_group.demo-log-group.name
}
