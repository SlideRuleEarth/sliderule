resource "aws_autoscaling_group" "node-cluster" {
  launch_configuration  = aws_launch_configuration.node-instance.id
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
}

resource "aws_launch_configuration" "node-instance" {
  image_id                    = data.aws_ami.sliderule_cluster_ami.id
  instance_type               = "r6g.xlarge"
  key_name                    = var.key_pair_name
  associate_public_ip_address = true
  security_groups             = [aws_security_group.node-sg.id]
  iam_instance_profile        = aws_iam_instance_profile.s3-role.name
  user_data = <<-EOF
      #!/bin/bash
      export ENVIRONMENT_VERSION=${var.environment_version}
      export IPV4=$(hostname -I | awk '{print $1}')
      aws ecr get-login-password --region us-west-2 | docker login --username AWS --password-stdin 742127912612.dkr.ecr.us-west-2.amazonaws.com
      export CLUSTER=${var.cluster_name}
      export SLIDERULE_IMAGE=${var.sliderule_image}
      export PROVISIONING_SYSTEM="https://ps.${var.domain}"
      aws s3 cp s3://sliderule/infrastructure/software/${var.cluster_name}-docker-compose-node.yml ./docker-compose.yml
      docker-compose -p cluster up --detach
  EOF
}
