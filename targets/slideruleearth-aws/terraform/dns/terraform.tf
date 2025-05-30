terraform {
  required_version = ">= 0.13"
  backend "s3" {
    bucket  = "sliderule"
    key     = "tf-states/cluster.tfstate"
    workspace_key_prefix = "tf-workspaces"
    encrypt = true
    profile = "default"
    region  = "us-west-2"
  }
}