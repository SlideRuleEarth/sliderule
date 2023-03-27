data "aws_secretsmanager_secret_version" "openidc" {
  secret_id = "prod/openidc/secrets"
}

locals {
  provsys_openidc = jsondecode(
    data.aws_secretsmanager_secret_version.openidc.secret_string
  )
}

resource "aws_instance" "monitor" {
    ami                         = data.aws_ami.sliderule_cluster_ami.id
    availability_zone           = var.availability_zone
    ebs_optimized               = false
    instance_type               = "c7g.large"
    monitoring                  = false
    key_name                    = var.key_pair_name
    vpc_security_group_ids      = [aws_security_group.monitor-sg.id]
    subnet_id                   = aws_subnet.sliderule-subnet.id
    associate_public_ip_address = true
    source_dest_check           = true
    iam_instance_profile        = aws_iam_instance_profile.s3-role.name
    private_ip                  = var.monitor_ip
    root_block_device {
      volume_type               = "gp2"
      volume_size               = 40
      delete_on_termination     = true
    }
    tags = {
      "Name" = "${var.cluster_name}-monitor"
    }
    user_data = <<-EOF
      #!/bin/bash
      echo ${var.cluster_name} > ./clustername.txt
      aws s3 cp s3://sliderule/infrastructure/software/${var.cluster_name}-export_logs.sh ./export_logs.sh
      chmod +x ./export_logs.sh
      aws s3 cp s3://sliderule/infrastructure/software/${var.cluster_name}-cronjob.txt ./cronjob.txt
      crontab ./cronjob.txt
      aws ecr get-login-password --region us-west-2 | docker login --username AWS --password-stdin 742127912612.dkr.ecr.us-west-2.amazonaws.com
      export CLIENT_ID='${local.provsys_openidc.client_id}'
      export CLIENT_SECRET='${local.provsys_openidc.client_secret}'
      export DOMAIN=${var.domain}
      export MONITOR_IMAGE=${var.container_repo}/monitor:${var.cluster_version}
      export PROXY_IMAGE=${var.container_repo}/proxy:${var.cluster_version}
      aws s3 cp s3://sliderule/infrastructure/software/${var.cluster_name}-docker-compose-monitor.yml ./docker-compose.yml
      docker-compose -p cluster up --detach
    EOF
}
