locals {
  ami_name = format("sliderule-%s", split(".","${var.cluster_version}")[0])
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