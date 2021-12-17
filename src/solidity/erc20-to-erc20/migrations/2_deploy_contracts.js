var Ether = artifacts.require("./AtomicSwapEther.sol");
var ERC20 = artifacts.require("./AtomicSwapERC20.sol");
var TestERC20 = artifacts.require("./TestERC20.sol");
var Test2ERC20 = artifacts.require("./Test2ERC20.sol");

//const accounts[1], change it when you start ganache
const Taker = "0x4AED5EB92d13DD8e4AB176bb354d86B914A47732";

module.exports = function(deployer) {
  deployer.deploy(Ether);
  deployer.deploy(ERC20);
  deployer.deploy(TestERC20, Taker);
  deployer.deploy(Test2ERC20, Taker);
};
