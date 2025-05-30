locals {
 terraform-git-repo = "sliderule-cluster"
}

provider "aws" {
  region  = var.aws_region
  default_tags {
    tags = {
      Owner   = "SlideRule"
      Project = "cluster-${var.organization_name}"
      terraform-base-path = replace(path.cwd,
      "/^.*?(${local.terraform-git-repo}\\/)/", "$1")
      cost-grouping = "${var.organization_name}"
    }
  }
}