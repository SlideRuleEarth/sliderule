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
 description = "the name tag for the EC2 instance"
 type        = string
 default     = "sliderule"
}
variable "ami_name" {
  description = "name of ami to use for cluster base image"
  type        = string
  default     = "sliderule-latest"
}
variable "node_asg_desired_capacity" {
  description = "number of nodes in node autoscaling group"
  type        = number
  default     = 2
}
variable "node_asg_min_capacity" {
  description = "minimum number of nodes in node autoscaling group"
  type        = number
  default     = 0
}
variable "node_asg_max_capacity" {
  description = "maxmum number of nodes in node autoscaling group"
  type        = number
  default     = 7
}
variable "sliderule_image" {
 description = "sliderule docker version to use"
 type        = string
 default     = "742127912612.dkr.ecr.us-west-2.amazonaws.com/sliderule:latest"
}
variable "ilb_image" {
 description = "intelligent load balancer docker version to use"
 type        = string
 default     = "742127912612.dkr.ecr.us-west-2.amazonaws.com/ilb:latest"
}
variable "monitor_image" {
 description = "monitor docker version to use"
 type        = string
 default     = "742127912612.dkr.ecr.us-west-2.amazonaws.com/monitor:latest"
}
variable "domain" {
  description = "root domain of site to use"
  default = "testsliderule.org"
}
