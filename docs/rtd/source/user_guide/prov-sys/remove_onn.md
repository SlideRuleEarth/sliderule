# Remove outstanding Org Number of Nodes requests

Request to remove any outstanding org number of nodes requests for the organization specified and the authorized user in the bearer token

**URL** : `ps.slideruleearth.io/api/remove_org_num_nodes/<str:org_name>/`

**Method** : `PUT`

**Auth required** : Yes. A valid access token for the organization must be included in the Authorization Header Bearer field

**Permissions required** : User is an active member of the organization (Note: the organization is specified as a claim in the provided token)

**Data constraints**

N/A

**Curl example**

```
curl -X PUT \
     -H "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ0b2tlbl90eXBlIjoiYWNjZXNzIiwiZXhwIjoxNjY4MTgxOTYzLCJpYXQiOjE2NjgwOTU1NjMsImp0aSI6ImJmYjIxMmExMzU0ZjQ4NGFhY2E2NmVjYWJmMmE3Mjg4Iiwib3JnX25hbWUiOiJVb2ZNRFRlc3QiLCJ1c2VyX25hbWUiOiJjZXVnYXJ0ZWJsYWlyIiwidXNlcl9pZCI6M30.nl1ACnWcoROhZ7K_HKOCOVfbqiDPBzmPdEPnAdb2vxk" \ https://ps.slideruleearth.io/api/remove_org_num_nodes_reqs/sliderule/
```

## Success Responses

**Condition** : user is an valid active member of the organization, the token in the Bearer field is a valid token for organization, the desired number of nodes is within the min/max range. The cluster is already deployed.

**Code** : `200 OK`

**Data Response**

```json
{"status":"SUCCESS","msg":"cleaned org node reqs for sliderule {username}"}
```

**Notes**

The response has a status field which indicated that the request was queued. The response has a msg field that inidicated that no update was initiated (because the cluster already had one node deployed). The msg field also indicates that list of queued requests. The queued requests contain the name of the cluster, the user that made the request, the number of nodes requested, and the expiration of the request. The response also has an error_msg field which is blank when the request is successful

## Error Responses

**Condition** : typo in the URL, i.e. invalid url

**Code** : `404 NOT FOUND`

**Content** : 
`{The requested resource was not found on this server.}`

**--- Or ---**

**Condition** : If a token for the wrong organization was provided

**Code** : `400 BAD REQUEST`

**Content** : 
```json
{"status":"FAILED","error_msg":"Token with wrong organization"}
```
 