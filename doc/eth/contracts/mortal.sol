pragma solidity ^0.4.15;

import "ownable.sol";

contract Mortal is Ownable
{
    /* This function is executed at initialization and sets the owner of the contract */
    constructor () public
    {
        _owner = msg.sender; 
    }

    /* Function to recover the funds on the contract */
    function kill() public
    { 
        if (msg.sender == _owner) 
        selfdestruct(_owner); 
    }
}