
import requests
import json

def web_api_get(config):
    cmd = json.loads(config) 
    reply = requests.get(url=cmd["url"], headers=cmd["headers"])
    str = json.dumps(reply.json(), indent = 2) #indent=2按照缩进格式
    return str

def web_api_post(config):
    cmd = json.loads(config) 
    reply = requests.get(url=["url"], data=cmd["data"], headers=cmd["headers"])
    str = json.dumps(reply.json(), indent = 2) #indent=2按照缩进格式
    return str
