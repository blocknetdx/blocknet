pragma solidity ^0.5.0;


contract ERC20 {
    uint public totalSupply;

    event Transfer(address indexed from, address indexed to, uint value);
    event Approval(address indexed owner, address indexed spender, uint value);

    function balanceOf(address who) public view returns (uint);
    function allowance(address owner, address spender) public view returns (uint);

    function transfer(address to, uint value) public returns (bool ok);
    function transferFrom(address from, address to, uint value) public returns (bool ok);
    function approve(address spender, uint value) public returns (bool ok);
}
