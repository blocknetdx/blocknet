pragma solidity ^0.4.15;

contract Ownable 
{
    /* Define variable owner of the type address */
    address _owner;

    /* This function is executed at initialization and sets the owner of the contract */
    constructor () public
    {
        _owner = msg.sender; 
    }

    function owner() public view returns (address) 
    {
        return _owner;
    }

    modifier onlyOwner() 
    {
        require(_owner == msg.sender, "Ownable: caller is not the owner");
        _;
    }
}