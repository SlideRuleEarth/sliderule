locals {
  ami_name = "sliderule-arm-${var.cluster_version}"
}

data "aws_ami" "sliderule_cluster_ami" {
  most_recent = true

  filter {
    name   = "name"
    values = [local.ami_name]
  }

  filter {
    name   = "virtualization-type"
    values = ["hvm"]
  }

  owners = ["self"]
}