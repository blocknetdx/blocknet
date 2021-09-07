pragma solidity ^0.4.15;

import "acct_base.sol";

contract ACCT_ERC20 id ACCTBase
{
    event Received(address, uint);

    function receive() external payable 
    {
        emit Received(msg.sender, msg.value);
    }
    
    function initiate(bytes20 hashedSecret, address responder, uint256 escrowDuration)
        public
        payable
        isEmpty(hashedSecret)
    {
        require(msg.value > 0);
        Swap storage swap = swaps[hashedSecret];
        swap.swapType = SwapType.Initiated;
        swap.initiator = msg.sender;
        swap.responder = responder;
        swap.value = msg.value;
        swap.refundTimePoint = now + escrowDuration;
        emit Initiated(hashedSecret, swap.initiator, swap.responder, swap.value, swap.refundTimePoint);
    }

    function respond(bytes20 hashedSecret, address initiator, uint256 escrowDuration)
        public
        payable
        isEmpty(hashedSecret)
    {
        require(msg.value > 0);
        Swap storage swap = swaps[hashedSecret];
        swap.swapType = SwapType.Responded;
        swap.initiator = initiator;
        swap.responder = msg.sender;
        swap.value = msg.value;
        swap.refundTimePoint = now + escrowDuration;
        emit Responded(hashedSecret, swap.initiator, swap.responder, swap.value, swap.refundTimePoint);
    }

    function refund(bytes20 hashedSecret) public isRefundable(hashedSecret) 
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
    function redeem(bytes20 hashedSecret, bytes memory secret) public isRedeemable(hashedSecret, secret) 
    {
        Swap storage swap = swaps[hashedSecret];
        uint256 value = swap.value;
        swap.value = 0;
        swap.swapType = SwapType.None;
        msg.sender.transfer(value);
        emit Redeemed(hashedSecret, secret, msg.sender, value);
    }

    event Receive(uint value);

    function () payable 
    {
        // TODO need to associate sender address with hashed secret
        // and store to map
        Receive(msg.value);
    }
}
