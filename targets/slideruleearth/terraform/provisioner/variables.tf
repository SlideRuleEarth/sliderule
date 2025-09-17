
variable "container_registry" {
  description = "The Docker container registry for SlideRule container images"
  type        = string
  default     = "742127912612.dkr.ecr.us-west-2.amazonaws.com"
}

# required
variable "lambda_zipfile" {
  description = "The code and environment for the lambda function"
  type        = string
}
