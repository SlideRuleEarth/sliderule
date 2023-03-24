# ------------------------------------------------------------
# Remote access
# ------------------------------------------------------------
variable "key_pair_name" {
  description = "The EC2 Key Pair to associate with the EC2 Instance for SSH access."
  type        = string
  default     = "surface"
}

# -------------------------------------------------------------
# AWS Variables
# -------------------------------------------------------------
variable "aws_region" {
  description = "The AWS region to deploy into"
  type        = string
  default     = "us-west-2"
}
variable  "availability_zone" {
  description = "The AWS availability zone to deploy into"
  type        = string
  default     = "us-west-2a"
}

# -------------------------------------------------------------
# Network Variables
# -------------------------------------------------------------
variable "monitor_ip" {
  type    = string
  default = "10.0.1.4"
}
variable "ilb_ip" {
  type    = string
  default = "10.0.1.5"
}
variable "vpcCIDRblock" {
    default = "10.0.0.0/16"
}
variable "publicCIDRblock" {
    default = "0.0.0.0/0"
}

#----------------------------------------------------------------
# Deployment Variables
#----------------------------------------------------------------
variable "cluster_name" {
 description = "organization name for the cluster of sliderule nodes"
 type        = string
 default     = "sliderule"
}
variable "cluster_version" {
 description = "version of docker containers to use for sliderule cluster"
 type        = string
 default     = "latest"
}
variable "node_asg_desired_capacity" {
  description = "number of nodes in node sliderule cluster"
  type        = number
  default     = 2
}
variable "node_asg_min_capacity" {
  description = "minimum number of nodes in node sliderule cluster"
  type        = number
  default     = 0
}
variable "node_asg_max_capacity" {
  description = "maxmum number of nodes in node sliderule cluster"
  type        = number
  default     = 7
}
variable "domain" {
  description = "root domain of sliderule cluster"
  default = "testsliderule.org"
}
variable "is_public" {
  description = "(True/False): The cluster is public (Note: public clusters do NOT require authentication)"
  default = "False"
}
variable "container_repo" {
  description = "container registry holding sliderule container images"
  default = "742127912612.dkr.ecr.us-west-2.amazonaws.com"
}
