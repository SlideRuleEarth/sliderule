
locals {
  terraform-git-repo = "sliderule-build-and-deploy"
}

provider "aws" {
  region = var.region
  default_tags {
    tags = {
      Owner   = "SlideRule"
      Project = "demo-${var.domain_root}"
      terraform-base-path = replace(path.cwd,
      "/^.*?(${local.terraform-git-repo}\\/)/", "$1")
      cost-grouping = "${var.cost_grouping}"
    }
  }
}
