import sys
from subprocess import call, check_output
from time import sleep
from random import random, randint
import json

address_current = 0
sent = 0
restart_daemon = 0
last_block = 0
while True:
        amount = str(round(random()*10, 1))
        obj2 = json.loads(check_output(["./darkcoind", "getinfo"]).strip())
        obj = json.loads(check_output(["./darkcoind", "getpoolinfo"]).strip())

        print obj
        print obj2['blocks'], obj['masternode']

        if last_block != obj2['blocks']:
                print "subscribe to masternode"
                check_output(["./darkcoind", "darksendsub"])
                last_block = obj2['blocks'] 

        if obj['entries'] > 0:
                amount = round((random()*10), 3)
                addr = check_output(["./darkcoind", "getaccountaddress", str(address_current)]).strip()
                address_current += 1
                print "Pool is active, sending %s to %s" % (amount, addr)
                call(["./darkcoind", "darksend", addr, str(amount)])
                restart_daemon += 1

        if restart_daemon >= 15:
                print "Restarting daemon"
                sleep(10)
                call(["./darkcoind", "stop"])
                sleep(10)
                call(["./darkcoind", "-daemon"])
                sleep(60)
                restart_daemon = 0
                print "Restarted"

        sleep(random()*15)

