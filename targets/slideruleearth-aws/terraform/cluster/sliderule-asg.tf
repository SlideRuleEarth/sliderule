resource "aws_autoscaling_group" "sliderule-cluster" {
  launch_configuration  = aws_launch_configuration.sliderule-instance.id
  desired_capacity      = var.node_asg_desired_capacity
  min_size              = var.node_asg_min_capacity
  max_size              = var.node_asg_max_capacity
  health_check_type     = "EC2"
  vpc_zone_identifier   = [ aws_subnet.sliderule-subnet.id ]
  tag {
    key                 = "Name"
    value               = "${var.cluster_name}-node"
    propagate_at_launch = true
  }

  # the following match the provider "aws" default tags in providers.tf ...
  tag {
    key                 = "Owner"
    value               = "SlideRule"
    propagate_at_launch = true
  }

  tag {
    key                 = "Project"
    value               = "cluster-${var.cluster_name}"
    propagate_at_launch = true
  }

  tag {
    key                 = "terraform-base-path"
    value               = replace(path.cwd, "/^.*?(${local.terraform-git-repo}\\/)/", "$1")
    propagate_at_launch = true
  }

  tag {
    key                 = "cost-grouping"
    value               = "${var.cluster_name}"
    propagate_at_launch = true
  }
}

resource "aws_launch_configuration" "sliderule-instance" {
  image_id                    = data.aws_ami.sliderule_cluster_ami.id
  instance_type               = var.cluster_instance_type
  root_block_device {
    volume_type               = "gp2"
    volume_size               = var.cluster_volume_size
    delete_on_termination     = true
  }
  key_name                    = var.key_pair_name
  associate_public_ip_address = true
  security_groups             = [aws_security_group.sliderule-sg.id]
  iam_instance_profile        = aws_iam_instance_profile.s3-role.name
  user_data = <<-EOF
      #!/bin/bash
      export ENVIRONMENT_VERSION=${var.environment_version}
      export IPV4=$(hostname -I | awk '{print $1}')
      export CLUSTER=${var.cluster_name}
      export IS_PUBLIC=${var.is_public}
      export SLIDERULE_IMAGE=${var.container_repo}/sliderule:${var.cluster_version}
      export PROVISIONING_SYSTEM="https://ps.${var.domain}"
      export CONTAINER_REGISTRY=${var.container_repo}
      aws ecr get-login-password --region us-west-2 | docker login --username AWS --password-stdin ${var.container_repo}
      aws s3 cp s3://sliderule/infrastructure/software/${var.cluster_name}-docker-compose-sliderule.yml ./docker-compose.yml
      docker pull ${var.container_repo}/oceaneyes
      docker-compose -p cluster up --detach
  EOF
}
