# xbridge.conf

* Under the ```[Main]``` heading, edit the following to suit your requirements:

  * ```ExchangeWallets=``` Add/remove coin heading's here
  * ```LogPath=``` Change file path to desired location 

* Under each ```[COIN]``` heading, edit the following to suit your individual wallet's RPC configuration:

  * ```Address=``` Edit this field to match your labelled receive address
  * ```Username=``` Edit this field to match RPCuser in the coin's .conf
  * ```Password=``` Edit this field to match RPCpassword in the coin's .conf
  
  * https://github.com/blocknetdx/blockdx/blob/master/setup-docs/supportedwalletsCONF.md

* This xbridge.conf will be updated as coins are added. Anything under the 'DX COMPATIBLE COINS' section can be traded on the DX.


## DX COMPATIBLE COINS

```
[Main]
ExchangeWallets=BTC,SYS,LTC,DGB,DASH,DYN,otherwallet1,otherwallet2
FullLog=true
LogPath=C:\Users\PCusername\AppData\Roaming\blocknet\log
ExchangeTax=300

[RPC]
Enable=false
UserName=rpc1
Password=rpc1
UseSSL=false
Port=8080

[BTC]
Title=Bitcoin
Address=BTC ADDRESS
Ip=127.0.0.1
Port=8332
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=0
ScriptPrefix=5
SecretPrefix=128
COIN=100000000
MinimumAmount=0
DustAmount=0
CreateTxMethod=BTC
MinTxFee=0
BlockTime=600
GetNewKeySupported=false
ImportWithNoScanSupported=false
TxVersion=1
FeePerByte=200

[SYS]
Title=SysCoin
Address=SYS ADDRESS
Ip=127.0.0.1
Port=8370
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=0
ScriptPrefix=5
SecretPrefix=128
COIN=100000000
MinimumAmount=0
DustAmount=0
CreateTxMethod=BTC
MinTxFee=0
BlockTime=60
GetNewKeySupported=false
ImportWithNoScanSupported=false
TxVersion=1
FeePerByte=200

[LTC]
Title=Litecoin
Address=LTC ADDRESS
Ip=127.0.0.1
Port=9332
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=48
ScriptPrefix=5
SecretPrefix=176
COIN=100000000
MinimumAmount=0
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=true
FeePerByte=200
MinTxFee=60000
TxVersion=1
BlockTime=60

[DASH]
Title=Dash
Address=DASH ADDRESS
Ip=127.0.0.1
Port=9998
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=76
ScriptPrefix=16
SecretPrefix=204
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=true
MinTxFee=60000
BlockTime=150
FeePerByte=200

[DYN]
Title=Dynamic
Address=DYN ADDRESS
Ip=127.0.0.1
Port=31350
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=30
ScriptPrefix=10
SecretPrefix=140
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=false
MinTxFee=10000
BlockTime=128
FeePerByte=200

[VIA]
Title=ViaCoin
Address=VIA ADDRESS
Ip=127.0.0.1
Port=5222
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=71
ScriptPrefix=33
SecretPrefix=199
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=false
MinTxFee=60000
BlockTime=24
FeePerByte=200

[DGB]
Title=Digibyte
Address=DGB ADDRESS
Ip=127.0.0.1
Port=14022
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=30
ScriptPrefix=5
SecretPrefix=128
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=true
MinTxFee=100000
BlockTime=60
FeePerByte=200

[VTC]
Title=Vertcoin
Address=VTC ADDRESS
Ip=127.0.0.1
Port=5888
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=71
ScriptPrefix=5
SecretPrefix=199
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=false
MinTxFee=60000
BlockTime=150
FeePerByte=200

[DOGE]
Title=Dogecoin
Address=DOGE ADDRESS
Ip=127.0.0.1
Port=22555 
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=30
ScriptPrefix=22
SecretPrefix=158
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=true
MinTxFee=100000000
BlockTime=60
FeePerByte=100000

[MUE]
Title=MonetaryUnit
Address=YOUR ADDRESS
Ip=127.0.0.1
Port=29683
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=16
ScriptPrefix=76
SecretPrefix=126
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=true
MinTxFee=100000
BlockTime=40
FeePerByte=500

[NMC]
Title=Namecoin
Address=YOUR ADDRESS
Ip=127.0.0.1
Port=8336
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=52
ScriptPrefix=13
SecretPrefix=180
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=true
MinTxFee=100000
BlockTime=600
FeePerByte=100

[QTUM]
Title=Qtum
Address=YOUR ADDRESS
Ip=127.0.0.1
Port=3889
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=58
ScriptPrefix=50
SecretPrefix=128
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=true
MinTxFee=20000
BlockTime=150
FeePerByte=20
```

## COMMUNICATES BUT TX FAILS
```
[SEQ]
Title=Sequence
Address=SEQ ADDRESS
Ip=127.0.0.1
Port=16663
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=63
ScriptPrefix=64
SecretPrefix=170
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=false
MinTxFee=100000
BlockTime=60
FeePerByte=200

[BAY]
Title=BitBay
Address=BAY ADDRESS
Ip=127.0.0.1
Port=19915
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=25
ScriptPrefix=85
SecretPrefix=153
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=true
MinTxFee=10000
BlockTime=64
FeePerByte=200

[STRAT]
Title=Stratis
Address=STRAT ADDRESS
Ip=127.0.0.1
Port=26174 
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=63
ScriptPrefix=125
SecretPrefix=191
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=true
MinTxFee=1000000
BlockTime=60
FeePerByte=1000

[FTC]
Title=Feathercoin
Address=FTC ADDRESS
Ip=127.0.0.1
Port=9337
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=14
ScriptPrefix=5
SecretPrefix=142
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=true
ImportWithNoScanSupported=true
MinTxFee=1000000
BlockTime=60
FeePerByte=1000

[QRK]
Title=Quarkcoin
Address=QRK ADDRESS
Ip=127.0.0.1
Port=8372
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=58
ScriptPrefix=9
SecretPrefix=186
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=false
MinTxFee=60000
BlockTime=30
FeePerByte=200

[XST]
Title=Stealthcoin
Address=XST ADDRESS
Ip=127.0.0.1
Port=46502
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=62
ScriptPrefix=85
SecretPrefix=190
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=true
MinTxFee=1000000
BlockTime=60
FeePerByte=1000
```

## NOT TESTED
```
[BLK]
Title=Blackcoin
Address=BLK ADDRESS
Ip=127.0.0.1
Port=15715
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=25
ScriptPrefix=85
SecretPrefix=153
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=false
MinTxFee=60000
BlockTime=30
FeePerByte=200

[BRK]
Title=Breakoutcoin
Address=BRK ADDRESS
Ip=127.0.0.1
Port=50542
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=19
ScriptPrefix=1
SecretPrefix=147
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=false
MinTxFee=60000
BlockTime=300
FeePerByte=200

[BRX]
Title=Breakoutstake
Address=BRX ADDRESS
Ip=127.0.0.1
Port=50542
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=19
ScriptPrefix=1
SecretPrefix=147
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=false
MinTxFee=60000
BlockTime=300
FeePerByte=200

[BLOCK]
Title=Blocknet
Address=BLOCK ADDRESS
Ip=127.0.0.1
Port=21358
Username=YOUR USERNAME
Password=YOURP PASSWORD
AddressPrefix=26
ScriptPrefix=28
SecretPrefix=154
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=true
MinTxFee=60000
BlockTime=150
FeePerByte=200

[DCR]
Title=Decred
Address=DCR ADDRESS
Ip=127.0.0.1
Port=9109
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=7
ScriptPrefix=7
SecretPrefix=110
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=true
MinTxFee=60000
BlockTime=150
FeePerByte=200

[POT]
Title=Potcoin
Address=POT ADDRESS
Ip=127.0.0.1
Port=42000
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=55
ScriptPrefix=5
SecretPrefix=183
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=true
MinTxFee=1000000
BlockTime=75
FeePerByte=1000

[PPC]
Title=Peercoin
Address=YOUR ADDRESS
Ip=127.0.0.1
Port=9902
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=55
ScriptPrefix=117
SecretPrefix=183
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=true
MinTxFee=1000000
BlockTime=600
FeePerByte=1000

[GRS]
Title=Groestlcoin
Address=YOUR ADDRESS
Ip=127.0.0.1
Port=1441
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=36
ScriptPrefix=5
SecretPrefix=128
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=true
MinTxFee=2000000
BlockTime=60
FeePerByte=2000

[BCC]
Title=Bitconnect
Address=YOUR ADDRESS
Ip=127.0.0.1
Port=9240
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=18
ScriptPrefix=85
SecretPrefix=146
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=true
MinTxFee=1000000
BlockTime=120
FeePerByte=1000
```

## WORK IN PROGRESS
```
[SWIFT]
Title=BitSwift
Address=SWIFT TEST ADDRESS
Ip=127.0.0.1
Port=21137 
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=
ScriptPrefix=
SecretPrefix=
COIN=
MinimumAmount=
TxVersion=
DustAmount=
CreateTxMethod=
GetNewKeySupported=
ImportWithNoScanSupported=
MinTxFee=
BlockTime=
FeePerByte=

[XC]
Title=XCurrency
Address=XC TEST ADDRESS
Ip=127.0.0.1
Port=32347 
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=
ScriptPrefix=
SecretPrefix=
COIN=
MinimumAmount=
TxVersion=
DustAmount=
CreateTxMethod=
GetNewKeySupported=
ImportWithNoScanSupported=
MinTxFee=
BlockTime=
FeePerByte=

[ZEC]
Title=ZCash
Address=ZEC TEST ADDRESS
Ip=127.0.0.1
Port=8232
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=28 & b8
ScriptPrefix=28 & bd
SecretPrefix=128
COIN=100000000
MinimumAmount=0
TxVersion=1
DustAmount=0
CreateTxMethod=BTC
GetNewKeySupported=false
ImportWithNoScanSupported=false
MinTxFee=???
BlockTime=150
FeePerByte=200

[ETH]
Title=Ethereum
Address=ETH TEST ADDRESS
Ip=127.0.0.1
Port=8545 
Username=YOUR USERNAME
Password=YOUR PASSWORD
AddressPrefix=
ScriptPrefix=
SecretPrefix=
COIN=
MinimumAmount=
TxVersion=
DustAmount=
CreateTxMethod=
GetNewKeySupported=
ImportWithNoScanSupported=
MinTxFee=
BlockTime=
FeePerByte=
```
