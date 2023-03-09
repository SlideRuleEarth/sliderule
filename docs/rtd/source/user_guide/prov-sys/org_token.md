# Obtain an access/refresh token pair

"Login" to a api session by obtaining an access bearer token to use to authorize subsequent requests

**URL** : `ps.slideruleearth.io/api/org_token/`

**Method** : `POST`

**Auth required** : YES username and password are required

**Permissions required** : Yes. User must be an active member of the organization for which the token is to be used

**Data constraints**

Provide username password and organization name.

```json
{
    "username" : "{username}",
    "password" : "******************",
    "org_name" : "UofMDTest"
}
```


**Curl Example**
```
curl -X POST \
     -H "Content-Type: application/json" \
     -d '{    
            "username" : "{username}",
            "password" : "******************",
            "org_name" : "UofMDTest"
        }' \
https://ps.slideruleearth.io/api/org_token/
```


## Success Response

**Condition** : If the user is an active member of the organization

**Code** : `200 Ok`

```json
{
    "exp": "2022-11-29T13:03:08UTC",
    "access_lifetime": "86400.0",
    "refresh_lifetime": "2592000.0",
    "refresh": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ0b2tlbl90eXBlIjoicmVmcmVzaCIsImV4cCI6MTY3MjIzMjU4OCwiaWF0IjoxNjY5NjQwNTg4LCJqdGkiOiJh1234567890ZmN2U0YjAwOWNkOGI2ZTI1MzRiNTdkNyIsIm9yZ19uYW1lIjoiVW9mTU1234567890dXNlcl9uYW1lIjoiY2V1Z2FydGVibGFpciIsInVzZXJfaWQiOjN9.czx8dPA4msGC1234581K6iwVwpW815kzluk7Htw-gow",
    "access": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ0b2tlbl90eXBlIjoiYWNjZXNzIiwi12345joxNjY5NzI2OTg4LCJpYXQiOjE2Njk2NDA1ODgsImp0aSI6Im123452Zjg1YTBkNjRlNjY4ZTEyZWQxYjlmNTFhN2M0Iiwib3JnX25hbWUiOiJVb2ZNRFRlc3Qi12345678905hbWUiOiJjZXVnYXJ0ZWJsYWlyIiwidXNlcl9pZCI6M30.4Q2E76l4UGHbUlrN7hYPKIEa4FKMJ7UhFEwJTV6fqdk"
}
```
**Notes**: exp is expiration of the access token. access_lifetime and refresh_lifetime are given is seconds. The refresh token can be used only once to obtain a new access token. Otherwise the user must use the /api/org_token/ endpoint and submit username and password.

## Error Responses

**Condition** : If organization is not valid.

**Code** : `403 Forbidden`

**Content** : 
```json
{"detail": "{org} is NOT a valid organization name"}
```

**--- Or ---**

**Condition** : If username or password are not valid.

**Code** : `401 Unauthorized`

**Content** : 
```json
{"detail": "No active account found with the given credentials"}
```
