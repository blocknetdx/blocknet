const atomicSwap = artifacts.require("./AtomicSwapERC20.sol");
const testERC20 = artifacts.require("./TestERC20.sol");


contract('Cross Chain Atomic Swap with ERC20', (accounts) => {

  const lock = "0x261c74f7dd1ed6a069e18375ab2bee9afcb1095613f53b07de11829ac66cdfcc";
  const key = "0x42a990655bffe188c9823a2f914641a32dcbb1b28e8586bd29af291db7dcd4e8";

  const swapID_swap = "0x0505915948dcd6756a8f5169e9c539b69d87d9a4b8f57cbb40867d9f91790211";
  const swapID_duration = "0xa6c79a27049109e472b246b5dfbe08aedff1e9e2259597e54032dbad4958d4ad";
  const swapID_expiry = "0xc3b89738306a66a399755e8535300c42b1423cac321938e7fe30b252abf8fe74";

  const maker = accounts[0];
  const taker = accounts[1];
  const another_person = accounts[2];

  const littleExchangeAmount = 500;
  const startBalance = 25000;
  const exchangeAmount = 10000;
  const Sum = 15000;

  let startAmountMaker, startAmountTaker;

  function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  it("Deposit erc20 tokens into the contract", async () => {
    const swap = await atomicSwap.deployed();
    const token = await testERC20.deployed();
    const duration = 10000; // seconds

    const resultOfMaker = await token.balanceOf(maker);
    const resultOfTaker = await token.balanceOf(taker);

    startAmountMaker = resultOfMaker;
    startAmountTaker = resultOfTaker;

    await token.approve(swap.address, exchangeAmount / 2, {from: maker});
    await token.approve(swap.address, exchangeAmount, {from: taker});

    assert.equal(resultOfMaker.words[0], startBalance);
    assert.equal(resultOfTaker.words[0], startBalance);

    await swap.open(
      swapID_swap, 
      exchangeAmount / 2,
      exchangeAmount,
      token.address,
      token.address, 
      taker, 
      lock, 
      duration, 
      {from: maker})
  })

  it("Check balance of {Maker, Taker}", async function(){

    const token = await testERC20.deployed();

    const resultOfMaker = await token.balanceOf(maker);
    const resultOfTaker = await token.balanceOf(taker);

    assert.equal(resultOfMaker.words[0], startBalance - exchangeAmount / 2);
    assert.equal(resultOfTaker.words[0], startBalance - exchangeAmount);
  });

  it("Check balance of Swap", async function(){

    const token = await testERC20.deployed();
    const swap = await atomicSwap.deployed();
    const result = await token.balanceOf(swap.address);

    assert.equal(result.words[0], Sum);
  });

  it("Check the erc20 tokens in the lock box", async () => {
    const swap = await atomicSwap.deployed();
    const token = await testERC20.deployed();
    const result  = await swap.check(swapID_swap);

    assert.equal(result[1].toNumber(), exchangeAmount / 2);
    assert.equal(result[2].toString(), token.address);
    assert.equal(result[3].toString(), taker);
    assert.equal(result[4].toString(), lock);
  })

  it("Withdraw the erc20 tokens from the lockbox", async () => {
    const swap = await atomicSwap.deployed();
    await swap.close(swapID_swap, key, {from: taker});
  })

  it("Check balance of Swap after deal", async function (){
    const token = await testERC20.deployed();
    const swap = await atomicSwap.deployed();
    const result = await token.balanceOf(swap.address);

    assert.equal(result.words[0], 0);
  });

  it("Check balance of {Maker, Taker}", async function(){

    const token = await testERC20.deployed();

    const resultOfMaker = await token.balanceOf(maker);
    const resultOfTaker = await token.balanceOf(taker);

    assert.equal(resultOfMaker.words[0], startBalance + exchangeAmount / 2);
    assert.equal(resultOfTaker.words[0], startBalance - exchangeAmount / 2);

    console.log("Maker start: ",startAmountMaker.words[0], "end: ", resultOfMaker.words[0], 
    "\nTaker start: ", startAmountTaker.words[0], "end: ", resultOfTaker.words[0]);
  });

  it("Get secret key from the contract", async () => {
    const swap = await atomicSwap.deployed();
    const secretkey = await swap.checkSecretKey(swapID_swap, {from: maker});
    assert.equal(secretkey.toString(), key);
  })

  it("Deposit erc20 tokens into the contract", async function(){
    const swap = await atomicSwap.deployed();
    const token = await testERC20.deployed();
    const duration = 100; // seconds

    await token.approve(swap.address, littleExchangeAmount, {from: maker});
    await token.approve(swap.address, littleExchangeAmount, {from: taker});

    await swap.open(
      swapID_duration, 
      littleExchangeAmount,
      littleExchangeAmount,
      token.address,
      token.address, 
      taker, 
      lock, 
      duration, 
      {from: maker})
  });

  it("Sleep until deal duration end and try to make it", async function (){
    const swap = await atomicSwap.deployed();
    await sleep(20000); //Sleep for 20 seconds

    try{await swap.close(swapID_duration, key, {from: taker}); } catch(all){
      assert.equal("ERROR_DURATION_END", all.reason, "Should be reverted");
    }
  });

  it("Deposit erc20 tokens into the contract", async () => {
    const swap = await atomicSwap.deployed();
    const token = await testERC20.deployed();
    const duration = 100; // seconds

    startAmountMaker = await token.balanceOf(maker);
    startAmountTaker = await token.balanceOf(taker);

    await token.approve(swap.address, littleExchangeAmount, {from: maker});
    await token.approve(swap.address, littleExchangeAmount, {from: taker});

    await swap.open(
      swapID_expiry, 
      littleExchangeAmount,
      littleExchangeAmount,
      token.address,
      token.address, 
      taker, 
      lock, 
      duration, 
      {from: maker})
  })

  it("Trying to expire by another person", async function(){
    const swap = await atomicSwap.deployed();

    try{await swap.expire(swapID_expiry, {from: another_person})} catch(all) {
      assert.equal("ERROR_INVALID_MSG_SENDER", all.reason, "Should be reverted");
    }
  });

  it("Expire by Maker or Taker", async function(){
    const swap = await atomicSwap.deployed();
    const token = await testERC20.deployed();
    await swap.expire(swapID_expiry); //Expire by Maker

    const resultOfMaker = await token.balanceOf(maker);
    const resultOfTaker = await token.balanceOf(taker);
    
    console.log("Maker start: ",startAmountMaker.words[0], "end: ", resultOfMaker.words[0], 
    "\nTaker start: ", startAmountTaker.words[0], "end: ", resultOfTaker.words[0]);
  });

  it("Trying to close deal after expire", async function(){
    const swap = await atomicSwap.deployed();

    try{await swap.close(swapID_expiry, key)} catch(all) {
      assert.equal("ERROR_SWAP_NOT_OPEN", all.reason, "Should be reverted");
    }
  });
});
