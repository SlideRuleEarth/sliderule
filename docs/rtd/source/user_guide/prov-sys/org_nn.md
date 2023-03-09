# Org Number of Nodes

Get the number of nodes currently deployed in the cluster for the organization along with the minimum and maximum allowed for the cluster

**URL** : `ps.slideruleearth.io/api/org_num_nodes/<str:org_name>/`

**Method** : `GET`

**Auth required** : Yes. A valid access token for the organization must be included in the Authorization Header Bearer field

**Permissions required** : User is an active member of the organization (Note: the organization is specified as a claim in the provided token)

**Data constraints**

N/A

**Curl example**

```
curl -X GET \
     -H "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ0b2tlbl90eXBlIjoiYWNjZXNzIiwiZXhwIjoxNjY5NzI2OTg4LCJpYXQiOjE2Njk2NDA1ODgsImp0aSI6ImI2Njc2Zjg1YTBkNjRlNjY4ZTEyZWQxYjlmNTFhN2M0Iiwib3JnX25hbWUiOiJVb2ZNRFRlc3QiLCJ1c2VyX25hbWUiOiJjZXVnYXJ0ZWJsYWlyIiwidXNlcl9pZCI6M30.4Q2E76l4UGHbUlrN7hYPKIEa4FKMJ7UhFEwJTV6fqdk" https://ps.testsliderule.org/api/org_num_nodes/UofMDTest/
```

## Success Responses

**Condition** : user is an valid active member of the organization, the token in the Bearer field is a valid token for organization, the desired number of nodes is within the min/max range. The cluster is already deployed with one node.

**Code** : `200 OK`

**Data Response**

```json
{"status":"SUCCESS","min_nodes":0,"current_nodes":1,"max_nodes":3}
```
**Notes**

The response has a status field which indicated that the request was successful and fields for the mimimum,maximum and current number of nodes

## Error Responses

**Condition** : typo in the URL, i.e. invalid url

**Code** : `404 NOT FOUND`

**Content** : `{The requested resource was not found on this server.}`

**--- Or ---**

**Condition** : If a token for the wrong organization was provided

**Code** : `400 BAD REQUEST`

**Content** : 
```json
{"status":"FAILED","error_msg":"Token with wrong organization"}
```
 