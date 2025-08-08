variable "domains" {
  description = "A list of domains to provision certificates. Can contain wildcard domains like *.example.com"
  type        = list(string)
  default     = ["*.slideruleearth.io"]
}

# https://letsencrypt.org/docs/expiration-emails/
variable "emails" {
  description = "A list of emails used for registration, recovery contact and expiry notification."
  type        = list(string)
  default     = ["jp.swinski@nasa.gov"]
}

variable "upload_s3" {
  description = "The S3 bucket to upload certificates."
  type = object({
    bucket = string,
    prefix = string,
    region = string,
  })
  default = {
    bucket = "sliderule"
    prefix = "config"
    region  = "us-west-2"
  }
}

# https://certbot.eff.org/docs/using.html#dns-plugins
variable "certbot_dns_plugin" {
  description = "The dns plugin for certbot."
  type        = string
  default     = "dns-route53"
}

variable "lambda_architectures" {
  description = "The architecture that the lambda runs on (arm64 or x86_64)."
  type        = list(string)
  default     = ["arm64"]
}

variable "lambda_runtime" {
  description = "Name of the runtime for lambda function."
  type        = string
  default     = "python3.9"
}

variable "lambda_memory_size" {
  description = "The amount of memory in MB that lambda function has access to."
  type        = number
  default     = 128
}

variable "lambda_timeout" {
  description = "The amount of time that lambda function has to run in seconds."
  type        = number
  default     = 300
}

variable "cron_expression" {
  description = "A cron expression for CloudWatch event rule that triggers lambda. Default is 00:00:00+0800 at 1st day of every month."
  type        = string
  default     = "0 16 L * ? *"
}