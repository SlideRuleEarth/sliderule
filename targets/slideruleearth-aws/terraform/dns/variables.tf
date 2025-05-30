# -------------------------------------------------------------
# AWS Variables
# -------------------------------------------------------------
variable "aws_region" {
  description = "The AWS region to deploy into"
  type        = string
  default     = "us-west-2"
}
variable "public_ip" {
  type        = string
  default     = "127.0.0.1"
}
variable "organization" {
  description = "name for the sliderule organization"
  type        = string
  default     = "sliderule"
}
variable "domain" {
  description = "root domain of sliderule cluster"
  default     = "slideruleearth.io"
}