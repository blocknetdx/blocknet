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
        obj2 = json.loads(check_output(["./darkcoind", "-datadir=datadir2", "getinfo"]).strip())
        obj = json.loads(check_output(["./darkcoind", "-datadir=datadir2", "getpoolinfo"]).strip())

        print obj
        print obj2['blocks'], obj['connected_to_masternode'], , obj['connected_to_masternode'] == obj['current_masternode']

        if last_block != obj2['blocks']:
                print "subscribe to masternode"
                check_output(["./darkcoind", "-datadir=datadir2", "darksendsub"])
                last_block = obj2['blocks'] 

        if obj['entries'] > 0 and obj['state'] == 2:
                amount = round((random()*10), 3)
                addr = check_output(["./darkcoind", "-datadir=datadir2", "getaccountaddress", str(address_current)]).strip()
                address_current += 1
                print "Pool is active, sending %s to %s" % (amount, addr)
                call(["./darkcoind", "-datadir=datadir2", "darksend", addr, str(amount)])
                restart_daemon += 1

        if restart_daemon >= 15:
                print "Restarting daemon"
                sleep(10)
                call(["./darkcoind", "-datadir=datadir2", "stop"])
                sleep(10)
                call(["./darkcoind", "-datadir=datadir2", "-daemon"])
                sleep(60)
                restart_daemon = 0
                print "Restarted"

        sleep(random()*15)

