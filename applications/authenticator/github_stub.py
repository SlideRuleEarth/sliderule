from fastapi import FastAPI, Request

app = FastAPI()

@app.post("/test")
async def handle_test(request: Request):
    body = await request.json()
    return {
        "received": body
    }