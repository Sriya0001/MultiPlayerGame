import urllib.request
import json

def test():
    # 1. Join
    req = urllib.request.Request(
        'http://127.0.0.1:8080/api/action',
        data=json.dumps({'action':'JOIN','player_id':8888}).encode(),
        headers={'Content-Type':'application/json'}
    )
    print('JOIN:', urllib.request.urlopen(req).read().decode())

    # 2. Move
    req2 = urllib.request.Request(
        'http://127.0.0.1:8080/api/action',
        data=json.dumps({'action':'MOVE','player_id':8888, 'x': 350, 'y': 450}).encode(),
        headers={'Content-Type':'application/json'}
    )
    print('MOVE:', urllib.request.urlopen(req2).read().decode())

    # 3. State
    state = urllib.request.urlopen('http://127.0.0.1:8080/api/state').read().decode()
    print('STATE:', state)

if __name__ == '__main__':
    test()
