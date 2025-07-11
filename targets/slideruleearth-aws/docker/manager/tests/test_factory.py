
def test_hello(client):
    response = client.get('/echo/hello')
    assert response.data == b'hello'