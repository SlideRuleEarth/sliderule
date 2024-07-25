resource "aws_launch_template" "sliderule_template" {
  name_prefix   = "${var.cluster_name}-"
  image_id      = data.aws_ami.sliderule_cluster_ami.id

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
  )

  tag_specifications {
    resource_type = "instance"
    tags = {
      Name                = "${var.cluster_name}-node"
      Owner               = "SlideRule"
      Project             = "cluster-${var.cluster_name}"
      terraform-base-path = replace(path.cwd, "/^.*?(${local.terraform-git-repo}\\/)/", "$1")
      cost-grouping       = "${var.cluster_name}"
    }
  }
}



resource "aws_autoscaling_group" "sliderule-cluster" {
  capacity_rebalance    = true
  desired_capacity      = var.node_asg_desired_capacity
  min_size              = var.node_asg_min_capacity
  max_size              = var.node_asg_max_capacity
  health_check_type     = "EC2"
  vpc_zone_identifier   = [ aws_subnet.sliderule-subnet.id ]

  mixed_instances_policy {
    instances_distribution {
      on_demand_base_capacity                  = var.node_asg_min_capacity
      on_demand_percentage_above_base_capacity = 0
      spot_allocation_strategy                 = var.spot_allocation_strategy
      spot_max_price                           = var.spot_max_price
    }

    launch_template {
      launch_template_specification {
        launch_template_id = "${aws_launch_template.sliderule_template.id}"
      }

      override {
        instance_type = "r7g.xlarge"
      }

      override {
        instance_type = "r8g.2xlarge"
      }

      override {
        instance_type = "r8g.xlarge"
      } 

      override {
        instance_type = "c7g.2xlarge"
      }
    }
  }

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
