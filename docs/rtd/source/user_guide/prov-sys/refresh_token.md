# Refresh an access token 

Request a new access token using a refresh token

**URL** : `ps.slideruleearth.io/api/org_token/refresh/`

**Method** : `POST`

**Auth required** : Yes. A valid refresh token must be provided in the body

**Permissions required** : User in the refresh token is an active member of the organization (Note: the organization is specified as a claim in the provided token)

**Data constraints**

A valid, unused, unexpired refresh token.

**Curl Example**

```
curl -X POST \
     -H Content-Type: application/json \
     -d {"refresh": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ0b2tlbl90eXBlIjoicmVmcmVzaCIsImV4cCI6MTY3MDA3MzEyNSwiaWF0IjoxNjY3NDgxMTI112345678901234567890MzExZjE0M2VlODUyYjE4YWEyOWRkMzg3MCIsIm9yZ19uYW1lIjoiVW9mTURUZXN0IiwidXNlcl9uYW1lIjoiY2V1Z2FydGVibGFpciIsInVzZXJfaWQiOjN9.NW4mJUsm48oNK0iRqojNEiC0fyqm3GANAjcii3T41pE"} \
https://ps.slideruleearth.io/api/org_token/refresh/ 
```

## Success Responses

**Condition** : user in token claim is a valid active member of the organization, the refresh token in the body  is a valid refresh token for organization. 

**Code** : `200 OK`

**Data Response**

```json 
{
    "exp": "2022-11-29T15:24:09UTC",
    "access_lifetime": "86400.0",
    "refresh_lifetime": "2592000.0",
    "refresh": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ0b2tlbl90eXBlIjoicmVmcmVzaCIsImV4cCI6MTY3MjI0MTA0OSwiaWF0IjoxNjY5NjQ5MDQ5LCJqdG123456789012345678901234567890ZTIwZTIyM2Q0OGZmMyIsIm9yZ19uYW1lIjoiVW9mTURUZXN0IiwidXNlcl9uYW1lIjoiY2V1Z2FydGVibGFpciIsInVzZXJfaWQiOjN9.hqbI18P8alQ1v2s0qun1NxOlou3QG0Ggd4vPmAotaZc",
    "access": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ0b2tlbl90eXBlIjoiYWNjZXNzIiwiZXhwIjoxNjY5NzM1NDQ5LCJpYXQiOjE2Njk2NDkwNDksImp0aSI6IjJmODcwYjlkM2E4ZjQyNWE4ZjgxNzI2YTQ3YWUxN2Q0Iiwib3JnX25hbWUiOiJVb2ZNRFRlc3QiLCJ1c2VyX25hbWUiOiJjZXVnYXJ0ZWJsYWlyIiwidXNlcl9pZCI6M30.NX396fsa-FotYMctDVe0CSVRDs17Q5lVuoBPCWPkRhM"
}
```

**Condition** : user in token claim is member but NOT an valid ***active*** member of the organization, the refresh token in the body is a valid token for the organization. 

**Code** : `200 OK`

**Data Response**

{"active":false}

**Notes**


## Error Responses

**Condition** : typo in the URL, i.e. invalid url

**Code** : `404 NOT FOUND`

**Content** : `{The requested resource was not found on this server.}`

 **--- Or ---**

 **Condition** : The user in refresh token claim is NOT a member of the organization

**Code** : `403 Forbidden`

 **Content** : 
 ```json
 "detail": "{user} is NOT a member of {organization}"
 ```
**--- Or ---**

**Condition** : The access token is invalid

**Code** : `403 Forbidden`

 **Content** : 
 ```json
 "detail": "Given token not valid for any token type"
 ```

**--- Or ---**

**Condition** : The refresh token is blacklisted (i.e. has already been used)

**Code** : `403 Forbidden`

 **Content** : 
 ```json
 { "detail": "Token is blacklisted", "code": "token_not_valid" }
 ```