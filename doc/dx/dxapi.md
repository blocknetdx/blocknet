## Blocknet RPC expanded with API for Decentralized Exchange. 
## They can be used like ordinary blocknet rpc methods.

`dxGetTransactionList` - without params, return list of pending and active XBridge transactions
example:
`./blocknetdx-cli dxGetTransactionList`

Output:
```
[
    {
        "id" : "8d8a570e1b3d003c2a2063491f9eea14e27fe3800443c981b0535aff9f59e37c",
        "from" : "LTC",
        "from address" : "LTnoVFAnKSMj4v2eFXBJuMmyjqSQT9eXBy",
        "fromAmount" : "0.00050000000000000001",
        "to" : "SYS",
        "to address" : "12BueeBVD2uiAHViXf7jPVQb2MSQ1Eggey",
        "toAmount" : "1",
        "state" : "Open"
    }
]
```
                                   
`dxGetTransactionsHistoryList` - without params, return list of historic XBridge transactions
example:
```
./blocknetdx-cli dxGetTransactionsHistoryList
```

Output:
```
[
    {
        "id" : "8d8a570e1b3d003c2a2063491f9eea14e27fe3800443c981b0535aff9f59e37c",
        "from" : "LTC",
        "from address" : "LTnoVFAnKSMj4v2eFXBJuMmyjqSQT9eXBy",
        "fromAmount" : "0.00050000000000000001",
        "to" : "SYS",
        "to address" : "12BueeBVD2uiAHViXf7jPVQb2MSQ1Eggey",
        "toAmount" : "1",
        "state" : "Expired"
    },
        "id" : "8d8a570e1b3d003c2a2063491f9eea14e27fe3800443c981b0535afacf59e37c",
        "from" : "BTC",
        "from address" : "LTnoVFAnKSMj4v2eFXBJuMmyjqSQT9eXBy",
        "fromAmount" : "0.00050000000000000001",
        "to" : "SYS",
        "to address" : "12BueeBVD2uiAHViXf7jPVQb2MSQ1Eggey",
        "toAmount" : "1000",
        "state" : "Open"
    }

]
```

`dxGetTransactionInfo` - return info about certain transaction
	params:
id - id of transaction

example:
```
./blocknetdx-cli dxGetTransactionInfo d63f5ed682ad744b176af1d58e9602219a40ab9bf3b506baeca81b975d999b38
```

Output:
```
[
    {
        "id" : "8d8a570e1b3d003c2a2063491f9eea14e27fe3800443c981b0535aff9f59e37c",
        "from" : "LTC",
        "from address" : "LTnoVFAnKSMj4v2eFXBJuMmyjqSQT9eXBy",
        "fromAmount" : "0.00050000000000000001",
        "to" : "SYS",
        "to address" : "12BueeBVD2uiAHViXf7jPVQb2MSQ1Eggey",
        "toAmount" : "1",
        "state" : "Expired"
    }
]
```




`dxGetCurrencyList` - without params, return list of connected wallets with their currencies names
example:
```
./blocknetdx-cli dxGetCurrencyList
```


Output:


```
{
    "DEC" : "",
    "DOGE" : "",
    "LTC" : "",
    "SYS" : ""
}
```




`dxCreateTransaction` - create transaction for exchanging coins 	
params:
address from - wallet address from which coins will be taken
currency from - currency name (BTC, SYS, LTC etc…)
amount from - amount of coins that will be taken
address to - wallet address that will receive coins 
currency to - currency name (BTC, SYS, LTC etc…)
amount to - amount of coins that will be receive

example:
```
./blocknetdx-cli dxCreateTransaction LTnoVFAnKSMj4v2eFXBJuMmyjqSQT9eXBy LTC 0.001 12BueeBVD2uiAHViXf7jPVQb2MSQ1Eggey SYS 1
```

Output:
```
{
    "id" : "222db6ba15d82ab9939b6f192d7c67c8abf12eef07f2e91983b68864b8678126"
}
```





`dxAcceptTransaction` - accept exchanging transaction

params:
id - id of exchanging transaction
address from - from wallet address
address to - to wallet address



example:
```
./blocknetdx-cli dxAcceptTransaction 222db6ba15d82ab9939b6f192d7c67c8abf12eef07f2e91983b68864b8678126 LTnoVFAnKSMj4v2eFXBJuMmyjqSQT9eXBy 12BueeBVD2uiAHViXf7jPVQb2MSQ1Eggey
```

Output:
```
{
    "id" : "222db6ba15d82ab9939b6f192d7c67c8abf12eef07f2e91983b68864b8678126"
}
```

`dxCancelTransaction` - cancel exchanging transaction

	params:
id - id of exchanging transaction


example:
```
./blocknetdx-cli dxCancelTransaction 8d8a570e1b3d003c2a2063491f9eea14e27fe3800443c981b0535aff9f59e37c
```

Output:
```
{
    "id" : "8d8a570e1b3d003c2a2063491f9eea14e27fe3800443c981b0535aff9f59e37c"
}
```


