# BLOCKNET
![alt text](https://github.com/blocknetdx/blockdx/blob/master/setup-docs/pictures/block.PNG "Logo Title Text 1")

The Internet Of Blockchains

## Blocknet Decentralised Exchange Setup Guide

**These instructions are to run as a trader on the Blocknet Decentralised Exchange** 

* Blocknet’s DX uses the xbridgep2p™ blockchain router technology to enable users to exchange tokens and assets, and to utilise smart contracts between blockchains.

* For initial simplicity, at present xbridgep2p™ is embedded into the blocknet wallet, and its code repository is here: https://github.com/atcsecure/blocknet/branches


## Overview
Setup requires an integration between the Blocknet wallet and the wallets of coins you want as currency pairs. At this early stage, nothing is automated and no UI is built (only a quick wallet integration has been done for the time being). Configuration is by manually creating (or editing) at least four .conf files: 

 * blocknet.conf

 * xbridge.conf

 * configuration file for each currency you want to support

Integration is via the wallets’ RPC APIs. For security reasons we recommend that wallets all run on a single box and communicate over localhost (127.0.0.1), though wallets may also be run on multiple machines and connect via IP address. General documentation on JSON RPC features is available at https://en.bitcoin.it/wiki/Running_Bitcoin.


## Setup blocknet.conf
The current version of the Blocknet wallet requires testing on the testnet until its hardfork, which will enable OP_CHECKLOCKTIMEVERIFY. As such, it’s necessary to configure the Blocknet wallet to run on the testnet. 

Additionally, the .conf file is required to contain the IP address of at least one service node.*
Create (or edit) a file named “blocknet.conf”, and paste the following into it:

   ```
   testnet=1 connect=104.238.198.117
   ```
   * Place the blocknet.conf file into the %AppData%\blocknet directory.


## Setup  .conf Files for the Wallets of Your Trading Coins:
The wallet of each coin you want to trade needs to be configured with a username/password and an allow from IP, if you’re using only a local machine use IP:127.0.0.1

 * Download the latest wallet, let it sync up fully, then close the wallet.

 * Click the START button on your desktop, where it says “Search program and files” then type “%appdata%” and the “Roaming” directory should pop up. Click on “Roaming” or hit enter.

 * Find your wallet’s designated folder, eg: Bitcoin

 * If you don’t have a .conf file started you will need to open up Notepad to create one.

 * Copy and paste the following (this can be added to what is already present in file if you have “addnodes” or other configurations in here already):

```
server=1
listen=1
rpcuser=yourusername
rpcpassword=yourpassword
rpcallowip=127.0.0.1
enableaccounts=1 (required for BitBay and Syscoin; probably fine for other wallets)
staking=0 (required for BitBay and Syscoin; probably fine for other wallets)
```

 * Change `rpcuser` and `rpcpassword` to something unique to you. For security reasons you should have a different RPC username and password for each wallet. 

 * If you’re using a single machine use IP: `127.0.0.1`

 * When you are done, click File, Save as, Type in: “bitcoin.conf”.
    * Ensure the file is not “bitcoin.conf.txt”

 * Save it and then place the .CONF into its corresponding wallet folder.
    * For this example: %Appdata%/Roaming/Bitcoin 

 * Remember what you wrote for the username, password, and IP.

 * Create an identical .CONF file for each wallet you are going to be using on the decentralised exchange.
 

## Configure Trading Coin Addresses
In each trading coin’s wallet, create a new address and label it something informative, like “DX address”. (xbridge expects a labelled receive address)

 * To create a new address, go to your wallet’s “receive” tab and click “new address”

 * To label an address, you may either right-click on it or click the “label” field. 

 * This needs to be done for all wallets being used.


## Setup xbridgep2p
The Blocknet’s Xbridge technology is available both as a standalone application and integrated into the Blocknet wallet. The wallet-integrated version currently includes the latest enhancements and is the one to use for testing. If you want to compile from source, visit:
https://github.com/atcsecure/blocknet/branches 

## Setup xbridge.conf
 * To see the full list of coin .conf's see: https://github.com/blocknetdx/blockdx/blob/master/setup-docs/xbridgeCONF.md

 * Download the following configuration file to the blocknet wallet folder in (for Windows) C:\Users\[yourusername]\AppData\Roaming\blocknet:
http://builds.xcurrency.co/blocknet/Build/Official_Blocknet_Wallet/xbridgep2p.conf

 * If the above file is not up to date, open it using Notepad++ or your text editor of choice, and replace its contents with the following, replacing highlighted text with your own details:

 * Note: to avoid crashes or failed trades, you currently need to run each wallet that is configured below. Please edit your config file to feature only and all the coins you wish to trade with.

 * To run as a service node, the .CONF file needs to contain the BTC dust value, as seen in the example above.

 * Paste the RPC usernames and passwords you created for each currency pair into the “Username” and “Password” fields in each section above.

 * For other coins, you will need to find the port the wallet is using on Localhost. Here is a working method to do so:

    * Open Task Manager and go to the “details” tab
    * Locate the relevant wallet and make a note of its PID.
    * Run Command Prompt and type “netstat -ano | field “[PID]”.D]”.
    * Look for an entry like the following: TCP	0.0.0.0:9332   0.0.0.0:0     LISTENING   	5336

 * The number after the colons in the IP address is the port to use (here, “9332”). Type it into xbridge.conf at the `Port=` line.

 * Note: wallets open different ports to connect to peers over the internet, and other ports on localhost for various purposes. The port you are looking for is a four-digit number after either the zero address (0.0.0.0) or localhost (127.0.0.1). The other ports use five-digit numbers.
 
 * Note: there may be more than one port open for your coin. In such cases, try the first one, and if it doesn’t work, try the next one.

 * Save the File (if you just edited the existing file, just click save, if you made a new file then click file, save as, and in the file name type: “xbridge.conf”).
    * Ensure the file is not “xbridgep2p.conf.txt”

 * Place this file into the folder where you extracted the “xbridgep2p” client.

 * If you’re using the new version of the Blocknet wallet with an integrated DX, place the“xbridge.conf” file into the C:\Users\PCusername\AppData\Roaming\blocknet directory.

 * You will be coming back to this to edit it in the future to add future coins, and change `RPCusername` `RPCpassword` `Port` `Address` 
 
 * Do not change the other settings in the .CONF file unless you are conducting tests.


## Startup
 * Ensure that each wallet is fully synced.

 * Ensure that each wallet is fully unlocked.

 * Start the currency pair wallets.

 * Start the blocknet wallet after starting the other wallets.


## Verify communication between wallets.
In order to ensure that the xbridge client is communicating with your wallets and the .conf files are setup properly, on the Blocknet wallet, click the “XBridge” tab and then click the “console” button.

As the wallet starts up, you’ll see the DX initialise using the values you entered into your xbridge.conf file:

 * Wait until you see “200” messages on the console. This signifies that the wallets are communicating over RPC and setup has been successful.

 * Note: If, amidst the “200” messages, you see a message similar to [I] 2017-Apr-19 17:48:31 [0x2],listaccounts exception couldn't connect to server, then it is likely that at least one of your specified trading wallets have not been run.

 * Note: If you fail to get “200” messages, it’s possible that the ports assigned to wallets differ from those specified in your .conf file. To check this, open Command Prompt, type netstat -an, and take a look which ports are being used over localhost (127.0.0.1), or sometimes over 0.0.0.0.
 
 
## Place an Order
Once you’ve confirmed that the wallets are communicating and setup has been successful, do the following:

   * In the “XBridge” tab of the Blocknet wallet, click on the “New Transaction” button. A new window will open:

   * Click on the “Address book” icon. This opens up a new window that displays the addresses you created in each currency pair wallet. 

      * Note: If you do not see these addresses, it means that your wallets are not communicating over RPC.

      * Note: It may take up to about 30 seconds for xbridgep2p to connect with your wallets, but once startup has completed it will populate your currency pair addresses.

      * Note: Do not manually paste an address into the “from” and “to” fields. Select addresses that xbridgep2p has been given by your currency pair wallets.


## Problem Diagnosis
To verify that each wallet is communicating with xbridgep2p make sure the created receive addresses for each wallet is listed in the address book. If this part fails, close your wallets and review their configuration files. 

Verify the ports are actually open. You may use Command Prompt to do so by typing in “netstat -an” and reviewing the print. Check that the ports you specified in the .conf files (8332 for Bitcoin and 8370 for SYS) are open over localhost (127.0.0.1).

 * Check that no OS-based firewall is blocking communication. You may do this through your firewall’s interface.

 * Check the progress of RPC communication in C:\Users\yourusername\AppData\Roaming\blocknet\log

 * Check the progress of CLTV-based coin exchange steps in C:\Users\yourusername\AppData\Roaming\blocknet\log-tx

 * Check on general wallet events in C:\Users\yourusername\AppData\Roaming\walletname\testnet\debug.log


## Run as a Service Node
A “service node” performs the function of collecting and distributing trade fees to the network. To run one, it is currently required that your Blocknet wallet holds 5000 BLOCK. When you run as a service node, you will receive trade fees on the DX.

   * Before opening up the xbridge client in exchange mode, you will have to run, fully sync and unlock all wallets that are in the xbridgep2p.conf file. You will also need to run a fully synced Blocknet wallet.

   * Verify there is communication between all wallets as per the above section.

   * Navigate to the folder where the xbridgep2p client, .conf and .bat files are located.

   * Either run 
“dx.bat” 
Or, in Command Prompt, run “blocknet.exe --enable-exchange”
Or paste “"C:\Program Files\Blocknet DX\blocknet-qt.exe” -enable-exchange” into the application shortcut’s “target” field
Or paste enable-exchange=1 into blocknet.conf

   * This will open up a command window and then a couple seconds later the client loads.

   * At the top of the client should have “[exchange enabled]” and should display “service node” on the bottom left side. 

   * No transactions can be created in service node mode.


## Security Tips
(With thanks to threepwood)

Since our technology essentially makes you your own exchange, here are some tips on how to keep your money safe.

   * The Blocknet’s team will never ask for your private keys or coins. Do not get fooled by impersonators.
Exchanges

   * Always move your coins from exchange to your private wallet.

   * Use long and random password.

   * Set up 2FA on logins and any withdrawals.

   * Disable password recovery via SMS/phone service. Disable all password recovery options for maximum security.

   * Recovery passwords are fine but keep them printed and offline.

   * Make sure your stored emails do not contain any extra information such as passwords or social security numbers.

   * Use different email addresses where possible. This limits the ability for hackers to run their automated "Forgot my password" links. 
   
   * Online Activity / Personal Information Disclosure. Do not promote your coin count etc online.

   * Limit your online public persona. This can attract unwanted attention which can make you a target.

   * Disable any online accounts you no longer use.

   * Assume hacking groups are building up social profiles on yourself. Your interests, time you are usually online, who you interact with.

   * Hacking groups use automated scripts so if those resources are exhausted they will try to social engineer your contacts.

   * Hacking groups are experts at social engineering. They have done this thousands of times.

   * Do not open random links and files provided in Slack, etc.

   * Do not fall for sob stories (Boohoo I lost all my coins) without proper due diligence.

   * Take multiple backups of your private keys regularly.

   * Verify backup by importing keys to the client.

   * Store your backups in M-DISC or in paper format.

   * Use dedicated wallet / staking PC and make it your safe haven. DO NOT USE IT FOR ANY OTHER ONLINE ACTIVITIES.

   * Encrypt your hard drives.

   * Use open source where possible.

   * Keep your software updated.

   * Do not install software from unknown 3rd party actors.

   * Use network level segmentation and mitigate attack surface against your wallet PC.
