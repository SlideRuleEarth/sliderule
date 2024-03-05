
variable "domainApex" {
    description = "domain name without subdomain e.g. testsliderule.org"
    default = "testsliderule.org"
}

variable "domainName" {
    description = "full domain name of site to use with TLD e.g. docs.testsliderule.org"
    default = "docs.testsliderule.org"
}

variable "domain_root" {
    description = "domain name without subdomain e.g. testsliderule"
    default = "testsliderule.org"
}

variable "cost_grouping" {
  description = "the name tag to identify a grouping or subset of resources"
  type        = string
  default     = "docs"
}