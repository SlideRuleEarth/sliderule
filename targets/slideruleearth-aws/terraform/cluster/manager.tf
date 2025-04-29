resource "aws_instance" "manager" {
    ami                         = data.aws_ami.sliderule_cluster_ami.id
    availability_zone           = var.availability_zone
    ebs_optimized               = false
    instance_type               = "c7g.large"
    monitoring                  = false
    key_name                    = var.key_pair_name
    vpc_security_group_ids      = [aws_security_group.manager-sg.id]
    subnet_id                   = aws_subnet.sliderule-subnet.id
    associate_public_ip_address = true
    source_dest_check           = true
    iam_instance_profile        = aws_iam_instance_profile.s3-role.name
    private_ip                  = var.manager_ip
    root_block_device {
      volume_type               = "gp2"
      volume_size               = 40
      delete_on_termination     = true
    }
    tags = {
      "Name" = "${var.cluster_name}-manager"
    }
    user_data = <<-EOF
      #!/bin/bash
      echo ${var.cluster_name} > ./clustername.txt
      export MANAGER_SECRET_SALT='${local.secrets.manager_secret_salt}'
      export DOMAIN=${var.domain}
      export MANAGER_IMAGE=${var.container_repo}/monitor:${var.cluster_version}
      mkdir /data
      aws s3 cp s3://sliderule/config/manager.db /data/manager.db
      aws s3 cp s3://sliderule/infrastructure/software/${var.cluster_name}-docker-compose-manager.yml ./docker-compose.yml
      docker-compose -p cluster up --detach
    EOF
}
