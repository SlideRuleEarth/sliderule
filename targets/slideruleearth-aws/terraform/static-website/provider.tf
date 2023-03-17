
locals {
  terraform-git-repo = "sliderule-build-and-deploy"
}

provider "aws" {
  region = var.region
  default_tags {
    tags = {
      owner   = "SlideRule"
      Project = "static-website-${var.domain_root}"
      terraform-base-path = replace(path.cwd,
      "/^.*?(${local.terraform-git-repo}\\/)/", "$1")
      cost-grouping = "${var.cost_grouping}"
    }
  }
}
