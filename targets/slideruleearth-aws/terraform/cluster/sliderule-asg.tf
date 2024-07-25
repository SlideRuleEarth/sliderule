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
        instance_type = var.cluster_instance_type
      }

      # override {
      #   image_id = ??
      #   instance_type = "r7g.xlarge"
      # }

      # override {
      #   image_id = ??
      #   instance_type = "r8g.2xlarge"
      # }

      # override {
      #   image_id = ??
      #   instance_type = "r8g.xlarge"
      # } 

      # override {
      #   image_id = ??
      #   instance_type = "c7g.2xlarge"
      # }
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
