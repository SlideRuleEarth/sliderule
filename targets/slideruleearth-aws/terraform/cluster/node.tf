resource "aws_launch_template" "sliderule_template" {
  name_prefix   = "${var.cluster_name}-"
  image_id      = data.aws_ami.sliderule_cluster_ami.id
  instance_type = "t4g.2xlarge"

  block_device_mappings {
    device_name = "/dev/xvda"
    ebs {
      volume_size = var.cluster_volume_size
      volume_type = "gp2"
      delete_on_termination = true
    }
  }

  key_name = var.key_pair_name

  network_interfaces {
    associate_public_ip_address = true
    security_groups             = [aws_security_group.sliderule-sg.id]
  }

  iam_instance_profile {
    name = aws_iam_instance_profile.s3-role.name
  }

  user_data = base64encode(<<-EOF
    #!/bin/bash
    export ENVIRONMENT_VERSION=${var.environment_version}
    export IPV4=$(hostname -I | awk '{print $1}')
    export ORGANIZATION=${local.organization}
    export CLUSTER=${var.cluster_name}
    export IS_PUBLIC=${var.is_public}
    export SLIDERULE_IMAGE=${var.container_repo}/sliderule:${var.cluster_version}
    export PROVISIONING_SYSTEM="https://ps.${var.domain}"
    export CONTAINER_REGISTRY=${var.container_repo}
    aws ecr get-login-password --region us-west-2 | docker login --username AWS --password-stdin ${var.container_repo}
    aws s3 cp s3://sliderule/config/ /plugin/ --recursive --exclude "*" --include "*.so"
    aws s3 cp s3://sliderule/infrastructure/software/${var.cluster_name}-docker-compose-node.yml ./docker-compose.yml
    docker-compose -p cluster up --detach
  EOF
  )

  tag_specifications {
    resource_type = "instance"
    tags = {
      Name                = "${local.organization}-node"
      Owner               = "SlideRule"
      Project             = "cluster-${local.organization}"
      terraform-base-path = replace(path.cwd, "/^.*?(${local.terraform-git-repo}\\/)/", "$1")
      cost-grouping       = "${local.organization}"
    }
  }
}