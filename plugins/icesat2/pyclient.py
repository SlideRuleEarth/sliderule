import requests

rqst_parm = '{ "hello" : "world" }'

r = requests.post('http://127.0.0.1:9081/echo', data=rqst_parm)

print(r.headers)
print(r.text)
print(r.json())
