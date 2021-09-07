pragma solidity ^0.4.15;

import "ownable.sol";

contract Mortal is Ownable
{
    /* Define variable owner of the type address */
    address owner;

    /* This function is executed at initialization and sets the owner of the contract */
    function Mortal() 
    {
        owner = msg.sender; 
    }

    /* Function to recover the funds on the contract */
    function kill() 
    { 
        if (msg.sender == owner) 
        selfdestruct(owner); 
    }
}