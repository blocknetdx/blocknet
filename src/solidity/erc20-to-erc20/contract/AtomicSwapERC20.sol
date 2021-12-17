pragma solidity ^0.5.0;

import "./ERC20.sol";

contract AtomicSwapERC20 {
    struct Swap {
        uint256 duration;
        uint256 erc20ValueMaker;
        uint256 erc20ValueTaker;
        address erc20Maker;
        address erc20Taker;
        address erc20ContractMaker;
        address erc20ContractTaker;
        bytes32 secretLock;
        bytes secretKey;
    }
    
    enum States {
        INVALID,
        OPEN,
        CLOSED,
        EXPIRED
    }

    mapping (bytes32 => Swap) private swaps;
    mapping (bytes32 => States) private swapStates;
    
    /**
    * @dev Emitted when Swap opened
    */
    event Open(bytes32 _swapID, address _withdrawTrader, bytes32 _secretLock);
    
    /**
    * @dev Emmited when Swap expired by Maker or Taker
    */
    event Expire(bytes32 _swapID);
    
    /**
    * @dev Emmited when Swap closed
    */
    event Close(bytes32 _swapID, bytes _secretKey);

    /**
    * @dev Modifier checks that swap is initialized and nothing more
    */
    modifier onlyInvalidSwaps(bytes32 _swapID) {
        require(swapStates[_swapID] == States.INVALID, 
            "ERROR_SWAP_NOT_INVALID"
        );
        _;
    }

    /**
    * @dev Modifier checks that swap is open(starts)
    */
    modifier onlyOpenSwaps(bytes32 _swapID) {
        require(swapStates[_swapID] == States.OPEN, 
            "ERROR_SWAP_NOT_OPEN"
        );
        _;
    }
    
    /**
    * @dev Modifier checks that swap is closed(deal)
    */
    modifier onlyClosedSwaps(bytes32 _swapID) {
        require(swapStates[_swapID] == States.CLOSED, 
            "ERROR_SWAP_NOT_CLOSED"
        );
        _;
    }
    
    /**
    * @dev Modifier checks that swap is not blocked by time
    */
    modifier onlyExpirableSwaps(bytes32 _swapID) {
        require(swaps[_swapID].duration <= block.timestamp, 
            "ERROR_DURATION_END"
        );
        _;
    }
    
    /**
    * @dev Modifier checks that secretKey correctly converted to the lockKey
    */
    modifier onlyWithSecretKey(bytes32 _swapID, bytes memory _secretKey) {
        require(swaps[_swapID].secretLock == sha256(_secretKey), 
            "ERROR_SECRET_KEY"
        );
        _;
    }
    
    /**
    * @dev Modifier that allows only Maker or Taker do something
    */
    modifier onlyMakerORTaker(bytes32 _swapID) {
        require(msg.sender == swaps[_swapID].erc20Maker || 
            msg.sender == swaps[_swapID].erc20Taker,
            "ERROR_INVALID_MSG_SENDER"
        );
        _;
    }

    /**
     * @dev Starts the swap and set OPEN to swap
     * Firstly check that swapID is never used before, after that
     * Checks both of sides allow negotiated amounts
     * Then takes ERC20 tokens from Maker and Taker to the Swap,
     * After that swap creates instance of the deal and change swap state to OPEN  
     * @param _swapID - sha256 which the parties agree on
     * @param _erc20ValueMaker - The number of tokens that the Maker are put up for a deal 
     * @param _erc20ValueTaker - The number of tokens that the Taker are put up for a deal 
     * @param _erc20ContractMaker - Address of Maker ERC20 Contract
     * @param _erc20ContractTaker - Address of Taker ERC20 contract
     * @param _takerAddress - Address of second side(Taker)
     * @param _secretLock - Hash of the secretKey
     * @param _duration - Duration for accepting the deal
     */
    function open(
        bytes32 _swapID, 
        uint256 _erc20ValueMaker, 
        uint256 _erc20ValueTaker, 
        address _erc20ContractMaker,
        address _erc20ContractTaker, 
        address _takerAddress, 
        bytes32 _secretLock, 
        uint256 _duration
        ) public onlyInvalidSwaps(_swapID) {
        ERC20 erc20ContractMakerInstance = ERC20(_erc20ContractMaker);
        ERC20 erc20ContractTakerInstance = ERC20(_erc20ContractTaker);

        require(_erc20ValueMaker <= erc20ContractTakerInstance.
            allowance(msg.sender, address(this)), "ERROR_MAKER_VALUE");
        require(_erc20ValueTaker <= erc20ContractMakerInstance.
            allowance(_takerAddress, address(this)), "ERROR_TAKER_VALUE");

        // Transfer value from the ERC20 trader to this contract.
        require(erc20ContractTakerInstance.
            transferFrom(msg.sender, address(this), _erc20ValueMaker), 
            "ERROR_TRANSFER_TOKEN_FROM_MAKER"
        );
        require(erc20ContractMakerInstance.
            transferFrom(_takerAddress, address(this), _erc20ValueTaker), 
            "ERROR_TRANSFER_TOKEN_FROM_TAKER"
        );

        // Store the details of the swap.
        Swap memory swap = Swap({
            duration: block.timestamp + _duration,
            erc20ValueMaker: _erc20ValueMaker,
            erc20ValueTaker: _erc20ValueTaker,
            erc20Maker: msg.sender,
            erc20Taker: _takerAddress, 
            erc20ContractMaker: _erc20ContractMaker,
            erc20ContractTaker: _erc20ContractTaker,
            secretLock: _secretLock,
            secretKey: new bytes(0)
        });

        swaps[_swapID] = swap;
        swapStates[_swapID] = States.OPEN;

        emit Open(_swapID, _takerAddress, _secretLock);
    }

    /**
     * @dev Close the deal, and set CLOSED to the swap
     * Firstly checks that swap in OPEN
     * Secondly, swap can be close only by Taker, he need to set secretKey
     * Then set CLOSE to the swap and send value to the both of sides
     * @param _swapID - ID of the certain swap
     * @param _secretKey - sha256(secretKey) must be eq with lockKey
     */
    function close(
        bytes32 _swapID, 
        bytes memory _secretKey
        ) public
          onlyOpenSwaps(_swapID)
          onlyWithSecretKey(_swapID, _secretKey) 
        {
        // Close the swap.
        Swap memory swap = swaps[_swapID];
        swaps[_swapID].secretKey = _secretKey;
        swapStates[_swapID] = States.CLOSED;

        // Transfer the ERC20 funds from this contract to the withdrawing trader.
        ERC20 erc20ContractMaker = ERC20(swap.erc20ContractMaker);
        ERC20 erc20ContractTaker = ERC20(swap.erc20ContractTaker);

        require(erc20ContractMaker.transfer(swap.erc20Taker, swap.erc20ValueMaker));
        require(erc20ContractTaker.transfer(swap.erc20Maker, swap.erc20ValueTaker));

        emit Close(_swapID, _secretKey);
    }

    /**
     * @dev Close the deal, and set EXPIRED to the swap
     * initialize staking, admin, oracle contracts for it.
     * @param _swapID describes prices, timeline, limits of new pool.
     */
    function expire(bytes32 _swapID) public onlyOpenSwaps(_swapID) onlyMakerORTaker(_swapID) {
        // Expire the swap.
        Swap memory swap = swaps[_swapID];
        swapStates[_swapID] = States.EXPIRED;

        // Transfer the ERC20 value from this contract back to the ERC20 trader.
        ERC20 erc20ContractMaker = ERC20(swap.erc20ContractMaker);
        ERC20 erc20ContractTaker = ERC20(swap.erc20ContractTaker);

        require(erc20ContractMaker.transfer(swap.erc20Maker, swap.erc20ValueMaker));
        require(erc20ContractTaker.transfer(swap.erc20Taker, swap.erc20ValueTaker));

        emit Expire(_swapID);
    }

    /**
     * @dev Get info about the deal
     * @param _duration - Time when swap can`t be close
     * @param _erc20ValueMaker - Amount that Maker takes to the deal
     * @param _erc20ContractAddressMaker - Address of Maker ERC20 contract
     * @param _takerAddress - Address of Taker
     * @param _secretLock - Hash of secretKey(sha256)
     */
    function check(bytes32 _swapID) public view returns (
        uint256 _duration, 
        uint256 _erc20ValueMaker,
        address _erc20ContractAddressMaker, 
        address _takerAddress, 
        bytes32 _secretLock) {
        Swap memory swap = swaps[_swapID];

        return (
            swap.duration, 
            swap.erc20ValueMaker, 
            swap.erc20ContractMaker, 
            swap.erc20Taker, 
            swap.secretLock 
        );
    }

    /**
     * @dev Get the secret key.
     * @notice Can be used only when swap was CLOSED
     * @param _swapID - ID of certain swap
     */
    function checkSecretKey(bytes32 _swapID) public view 
        onlyClosedSwaps(_swapID) 
        returns (bytes memory secretKey) {
        Swap memory swap = swaps[_swapID];
        return swap.secretKey;
    }
}
