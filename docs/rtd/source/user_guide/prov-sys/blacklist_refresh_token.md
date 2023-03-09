# Blacklist a refresh token 

Make a given refresh token invalid

**URL** : `ps.slideruleearth.io/api/token/blacklist/`

**Method** : `POST`

**Auth required** : Yes. A valid refresh token must be provided in the body

**Permissions required** : None

**Data constraints**

A valid, unused, unexpired refresh token.

**Curl example**

```
curl -X POST \
 -H Content-Type: application/json \
 -d {"refresh": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ0b2tlbl90eXBlIjoicmVmcmVzaCIsImV4cCI6MTY3MDA3MzEyNSwiaWF0IjoxNjY3NDgxMTI112345678901234567890MzExZjE0M2VlODUyYjE4YWEyOWRkMzg3MCIsIm9yZ19uYW1lIjoiVW9mTURUZXN0IiwidXNlcl9uYW1lIjoiY2V1Z2FydGVibGFpciIsInVzZXJfaWQiOjN9.NW4mJUsm48oNK0iRqojNEiC0fyqm3GANAjcii3T41pE"}
 https://ps.slideruleearth.io/api/token/blacklist/  
```

## Success Responses

**Condition** : user in token claim is a valid active member of the organization, the refresh token in the body  is a valid refresh token for organization. 

**Code** : `200 OK`

**Data Response**

```json 
{}
```
**Note**:
The response is a Null json string

**Condition** : user in token claim is member but NOT an valid ***active*** member of the organization, the refresh token in the body is a valid token for the organization. 

**Code** : `200 OK`

**Data Response**
```json
{"active":false}
```
**Notes**

## Error Responses

**Condition** : typo in the URL, i.e. invalid url

**Code** : `404 NOT FOUND`

**Content** : `{The requested resource was not found on this server.}`

**--- Or ---**

**Condition** : The token is invalid

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