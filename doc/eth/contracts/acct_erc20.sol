pragma solidity ^0.4.15;

import "acct_base.sol";

contract ERC20Interface
{
    function balanceOf(address _owner) external constant returns (uint256 balance);
    function allowance(address _owner, address _spender) external constant returns (uint remaining);
    function transfer(address _to, uint _value) external returns (bool);
    function transferFrom(address _from, address _to, uint _value) external returns (bool);
}

contract ACCT_ERC20 is ACCTBase
{
    ERC20Interface erc20Instance;


    constructor (address erc20address) public
    {
        erc20Instance = ERC20Interface(erc20address);
    }
    
    function balanceOf(address accountAddress) public constant returns (uint256 balance)
    {
        return erc20Instance.balanceOf(accountAddress);
    }
    
    function allowance(address initiator) public constant returns (uint256 amount)
    {
        return erc20Instance.allowance(initiator, address(this));
    }

    function initiate(uint256 value, bytes20 hashedSecret, address responder, uint256 escrowDuration)
        public
        isEmpty(hashedSecret)
    {
        require(value > 0, "bad value");
        
        // TODO check minimal value for escrowDurationn
        require(escrowDuration > 0, "bad escrowDuration");

        // require(balanceOf(msg.sender) >= value);
        require(erc20Instance.allowance(msg.sender, address(this)) >= value, "amount not approved");
        
        erc20Instance.transferFrom(msg.sender, address(this), value);
        
        Swap storage swap    = swaps[hashedSecret];
        swap.swapType        = SwapType.Initiated;
        swap.initiator       = msg.sender;
        swap.responder       = responder;
        swap.value           = value;
        swap.refundTimePoint = now + escrowDuration;

        emit Initiated(hashedSecret, swap.initiator, swap.responder, swap.value, swap.refundTimePoint);
    }

    function respond(uint256 value, bytes20 hashedSecret, address initiator, uint256 escrowDuration)
        public
        isEmpty(hashedSecret)
    {
        require(value > 0, "value less than zero");

        // TODO check minimal value for escrowDurationn
        require(escrowDuration > 0, "bad escrowDuration");

        // require(balanceOf(msg.sender) >= value);
        require(erc20Instance.allowance(msg.sender, address(this)) >= value, "amount not approved");
        
        erc20Instance.transferFrom(msg.sender, address(this), value);
        
        Swap storage swap    = swaps[hashedSecret];
        swap.swapType        = SwapType.Responded;
        swap.initiator       = initiator;
        swap.responder       = msg.sender;
        swap.value           = value;
        swap.refundTimePoint = now + escrowDuration;

        emit Responded(hashedSecret, swap.initiator, swap.responder, swap.value, swap.refundTimePoint);
    }




    function refund(bytes20 hashedSecret) 
        public 
        isRefundable(hashedSecret) 
    {
        Swap storage swap = swaps[hashedSecret];
        uint256 value = swap.value;
        swap.value = 0;
        swap.swapType = SwapType.None;
        msg.sender.transfer(value);
        emit Refunded(hashedSecret, msg.sender, value);
    }

    /** The initiator/responder wants to execute the deal
     *
     * \param hashedSecret Hash of initiator's secret
     * \param secret       Initiator's secret
     */
    function redeem(bytes20 hashedSecret, bytes memory secret) 
        public
        isRedeemable(hashedSecret, secret) 
    {
        Swap storage swap = swaps[hashedSecret];
        uint256 value = swap.value;
        swap.value = 0;
        swap.swapType = SwapType.None;
        msg.sender.transfer(value);
        emit Redeemed(hashedSecret, secret, msg.sender, value);
    }

    // event Received(address, uint);

    // function receive() external payable 
    // {
    //     emit Received(msg.sender, msg.value);
    // }

    // event Receive(uint value);

    // function () payable 
    // {
    //     // TODO need to associate sender address with hashed secret
    //     // and store to map
    //     Receive(msg.value);
    // }
}
