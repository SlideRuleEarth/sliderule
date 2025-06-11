resource "aws_autoscaling_group" "sliderule-cluster" {
  capacity_rebalance    = true
  desired_capacity      = var.node_asg_desired_capacity
  min_size              = var.node_asg_min_capacity
  max_size              = var.node_asg_max_capacity
  health_check_type     = "EC2"
  vpc_zone_identifier   = [ aws_subnet.sliderule-subnet.id ]

  launch_template {
    id      = aws_launch_template.sliderule_template.id
    version = "$Latest"
  }

  tag {
    key                 = "Name"
    value               = "${local.organization}-node"
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
    value               = "cluster-${local.organization}"
    propagate_at_launch = true
  }

  tag {
    key                 = "terraform-base-path"
    value               = replace(path.cwd, "/^.*?(${local.terraform-git-repo}\\/)/", "$1")
    propagate_at_launch = true
  }

  tag {
    key                 = "cost-grouping"
    value               = "${local.organization}"
    propagate_at_launch = true
  }
}
