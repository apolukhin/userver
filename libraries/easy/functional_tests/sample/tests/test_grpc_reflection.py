# /// [Functional test]
async def test_hello_simple(service_client):
    response = await service_client.get('/hello')
    assert response.status == 200
    assert 'text/plain' in response.headers['Content-Type']
    assert response.text == 'Hello!\n'
    # /// [Functional test]
    
async def disable_test_hello_base(service_client):
    response = await service_client.get('/hello')
    assert response.status == 200
    assert 'text/plain' in response.headers['Content-Type']
    assert response.text == 'Hello, unknown user!\n'
    assert 'X-RequestId' not in response.headers.keys(), 'Unexpected header'

    response = await service_client.get('/hello', params={'name': 'userver'})
    assert response.status == 200
    assert 'text/plain' in response.headers['Content-Type']
    assert response.text == 'Hello, userver!\n'

