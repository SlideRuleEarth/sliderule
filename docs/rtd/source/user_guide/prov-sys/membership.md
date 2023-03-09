# Obtain membership status of access token's user

Request the membership status of the user and organization in the provided access token.

**URL** : `ps.slideruleearth.io/api/membership_status/<str:org_name>/`

**Method** : `GET`

**Auth required** : Yes. A valid access token for the organization must be included in the Authorization Header Bearer field

**Permissions required** : User is an active member of the organization (Note: the organization is specified as a claim in the provided token)

**Data constraints**

N/A

**Curl Example**
```
curl -X GET \
    -H '"Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ0b2tlbl90eXBlIjoiYWNjZXNzIiwiZXhwIjoxNjY4MTgxOTYzLCJpYXQiOjE2NjgwOTU1NjMsImp0aSI6ImJmYjIxMmExMzU0ZjQ4NGFhY2E2NmVjYWJmMmE3Mjg4Iiwib3JnX25hbWUiOiJVb2ZNRFRlc3QiLCJ1c2VyX25hbWUiOiJjZXVnYXJ0ZWJsYWlyIiwidXNlcl9pZCI6M30.nl1ACnWcoROhZ7K_HKOCOVfbqiDPBzmPdEPnAdb2vxk" \ 
https://ps.slideruleearth.io/api/membership_status/sliderule/
```
## Success Responses

**Condition** : user in token claim is a valid active member of the organization, the token in the Bearer field is a valid token for organization. 

**Code** : `200 OK`

**Data Response**
```json
{"active":true}
```

**Condition** : user in token claim is member but NOT an valid ***active*** member of the organization, the token in the Bearer field is a valid token for organization. 

**Code** : `200 OK`

**Data Response**
```json
{"active":false}
```
**Notes**

## Error Responses:

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
 **--- Or ---**

 **Condition** : The user in token claim is NOT a member of the organization

**Code** : `403 Forbidden`

 **Content** : 
 ```json
 {"detail": "{username} is NOT a member of {org}"}
 ```

**Condition** : The token is invalid

**Code** : `403 Forbidden`

 **Content** : 
 ```json
 {"detail": "Given token not valid for any token type"}
```

