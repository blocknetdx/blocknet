pragma solidity ^0.4.15;

import "acct_base.sol";

contract BlocknetDXv1 id ACCTBase
{
    /** Initiate an atomic swap to another blockchain
     *
     * \param hashedSecret   Hash of the initiator's secret
     * \param responder      Address of the responder on this blockchain
     * \param escrowDuration Escrow period, in seconds (from now)
     */
    function initiate(
        bytes20 hashedSecret,
        address responder,
        uint256 escrowDuration
    )
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

    /** Respond to an atomic swap from another blockchain
     *
     * \param hashedSecret   Hash of the initiator's secret
     * \param initiator      Address of the initiator on this blockchain
     * \param escrowDuration Escrow period, in seconds (from now)
     */
    function respond(
        bytes20 hashedSecret,
        address initiator,
        uint256 escrowDuration
    )
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

    /** The initiator/responder wants its coins back
     *
     * \param hashedSecret Hash of initiator's secret
     */
    function refund(bytes20 hashedSecret) public isRefundable(hashedSecret) {
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
    function redeem(bytes20 hashedSecret, bytes memory secret) public isRedeemable(hashedSecret, secret) {
        Swap storage swap = swaps[hashedSecret];
        uint256 value = swap.value;
        swap.value = 0;
        swap.swapType = SwapType.None;
        msg.sender.transfer(value);
        emit Redeemed(hashedSecret, secret, msg.sender, value);
    }
}
