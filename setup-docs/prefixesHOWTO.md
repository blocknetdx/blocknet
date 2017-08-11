# How To Find .CONF Prefixes Instructions

1. Address Prefix:

    1. Go to: http://lenschulwitz.com/base58
    2. Enter wallet address into Base 58 Decoder
    3. First 2 digits of HEX copy into: http://www.binaryhexconverter.com/hex-to-decimal-converter
    4. Decimal value = address prefix

2. Script Prefix:

    1. Go to wallet console
    2. Type `validateaddress <wallet address>`
    3. Copy `pubkey`
    4. If it says something about locked wallet go `walletpassphrase <walletpassword> <pick a number in seconds>` example: `walletpassphrase password 10000`
    5. Type `decodescript <enter copied pubkey from step iii>`
    6. Take the p2sh output and decode that to HEX on the base58 website
    7. Then convert first 2 digits of hex. decimal value = script prefix

3. Secret Prefix:

    1. Go to wallet console
    2. Type `dumpprivkey <wallet address>`
    3. Copy the `privkey` 
    4. Decode the privkey to HEX on the base 58 website
    5. Then convert first 2 digits of hex. decimal value = secret prefix

4.  RPC Port:

    1. Search wallet’s GitHub, official webpage, bitcointalk, general google searches
    2. Try some of the website sources listed below on the next page
    3. Under the wallets Debug or Tool menu, click the Peers tab. The IP address port is sometimes 1 below what is listed. This only works sometimes.
    4. From command line (windows: `netstat -an` / linux: `netstat -anp`) That should show open ports (on windows it wont’ show the process, but there is a GUI that does)

Thanks @86b (Slack) for figuring this method out
