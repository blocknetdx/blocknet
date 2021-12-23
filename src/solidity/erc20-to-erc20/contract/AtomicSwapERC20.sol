pragma solidity ^0.8.0;

import "@openzeppelin/contracts/token/ERC20/IERC20.sol";

contract AtomicSwapERC20 {
    struct Swap {
        uint256 endTimelock;
        uint256 makerReceiveValue;
        uint256 takerReceiveValue;
        address makerAddress;
        address takerAddress;
        address makerERC20;
        address takerERC20;
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
        require(swaps[_swapID].endTimelock <= block.timestamp, 
            "ERROR_TIMELOCK"
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
        require(msg.sender == swaps[_swapID].makerAddress || 
            msg.sender == swaps[_swapID].takerAddress,
            "ERROR_INVALID_MSG_SENDER"
        );
        _;
    }

    /**
     * @dev Starts the swap and set OPEN state to swap
     * Firstly check that swapID is never used before, after that
     * Checks both of sides allow negotiated amounts
     * Then takes ERC20 tokens from Maker and Taker to the Swap
     * After that swap creates instance of the deal and change swap state to OPEN  
     * @param _swapID - sha256 which the parties agree on
     * @param _makerReceiveValue - The number of tokens that the Maker are put up for a deal 
     * @param _takerReceiveValue - The number of tokens that the Taker are put up for a deal 
     * @param _makerERC20 - Address of Maker ERC20 Contract
     * @param _takerERC20 - Address of Taker ERC20 contract
     * @param _takerAddress - Address of second side(Taker)
     * @param _secretLock - Hash of the secretKey
     * @param _duration - Duration for accepting the deal
     */
    function open(
        bytes32 _swapID, 
        uint256 _makerReceiveValue, 
        uint256 _takerReceiveValue, 
        address _makerERC20,
        address _takerERC20, 
        address _takerAddress, 
        bytes32 _secretLock, 
        uint256 _duration
        ) public payable onlyInvalidSwaps(_swapID) {

        IERC20 makerERC20Instance = IERC20(_makerERC20);
        IERC20 takerERC20Instance = IERC20(_takerERC20);

        address _makerAddress = msg.sender;

        require(_takerReceiveValue <= makerERC20Instance.
            allowance(_makerAddress, address(this)), "ERROR_TAKER_VALUE");
        require(_makerReceiveValue <= takerERC20Instance.
            allowance(_takerAddress, address(this)), "ERROR_MAKER_VALUE");

        // Transfer value from the ERC20 trader to this contract
        require(makerERC20Instance.
            transferFrom(_makerAddress, address(this), _takerReceiveValue), 
            "ERROR_TRANSFER_TOKEN_FROM_MAKER"
        );
        require(takerERC20Instance.
            transferFrom(_takerAddress, address(this), _makerReceiveValue), 
            "ERROR_TRANSFER_TOKEN_FROM_TAKER"
        );

        // Store the details of the swap
        Swap memory swap = Swap({
            endTimelock: block.timestamp + _duration,
            makerReceiveValue: _makerReceiveValue,
            takerReceiveValue: _takerReceiveValue,
            makerAddress: _makerAddress,
            takerAddress: _takerAddress, 
            makerERC20: _makerERC20,
            takerERC20: _takerERC20,
            secretLock: _secretLock,
            secretKey: new bytes(0)
        });

        swaps[_swapID] = swap;
        swapStates[_swapID] = States.OPEN;

        emit Open(_swapID, _takerAddress, _secretLock);
    }

    /**
     * @dev Close the deal, and set CLOSED state to the swap
     * Firstly checks that swap in OPEN state
     * Secondly, swap can be close only by Taker, he need to set secretKey
     * Then set CLOSE to the swap and send value to the both of sides
     * @param _swapID - ID of the certain swap
     * @param _secretKey - sha256(secretKey) must be eq with lockKey
     */
    function close(
        bytes32 _swapID, 
        bytes memory _secretKey
        ) public
          payable 
          onlyOpenSwaps(_swapID)
          onlyWithSecretKey(_swapID, _secretKey) 
        {
        // Close the swap.
        Swap memory swap = swaps[_swapID];
        swaps[_swapID].secretKey = _secretKey;
        swapStates[_swapID] = States.CLOSED;

        // Transfer the ERC20 funds from this contract to the withdrawing trader.
        IERC20 makerERC20 = IERC20(swap.makerERC20);
        IERC20 takerERC20 = IERC20(swap.takerERC20);

        require(makerERC20.transfer(swap.takerAddress, swap.takerReceiveValue));
        require(takerERC20.transfer(swap.makerAddress, swap.makerReceiveValue));

        emit Close(_swapID, _secretKey);
    }

    /**
     * @dev Close the deal, and set EXPIRED to the swap
     * Expire can be called any time before swap is CLOSED
     * Because what if one of side is dead
     * @param _swapID -ID of the certain swap
     */
    function expire(bytes32 _swapID) 
        public 
        payable 
        onlyOpenSwaps(_swapID) 
        onlyMakerORTaker(_swapID) {
        // Expire the swap.
        Swap memory swap = swaps[_swapID];
        swapStates[_swapID] = States.EXPIRED;

        // Transfer the ERC20 value from this contract back to the ERC20 trader.
        IERC20 makerERC20 = IERC20(swap.makerERC20);
        IERC20 takerERC20 = IERC20(swap.takerERC20);

        require(makerERC20.transfer(swap.makerAddress, swap.takerReceiveValue));
        require(takerERC20.transfer(swap.takerAddress, swap.makerReceiveValue));

        emit Expire(_swapID);
    }

    /**
     * @dev Get info about the deal
     * @param _endTimelock - Time when swap will close automaticly
     * @param _makerReceiveValue - Amount that Maker takes to the deal
     * @param _makerERC20 - Address of Maker ERC20 contract
     * @param _takerAddress - Address of Taker
     * @param _secretLock - Hash of secretKey(sha256)
     */
    function check(bytes32 _swapID) public view returns (
        uint256 _endTimelock, 
        uint256 _makerReceiveValue,
        address _makerERC20, 
        address _takerAddress, 
        bytes32 _secretLock) {
        Swap memory swap = swaps[_swapID];

        return (
            swap.endTimelock, 
            swap.makerReceiveValue, 
            swap.makerERC20, 
            swap.takerAddress, 
            swap.secretLock 
        );
    }

    /**
     * @dev Get the secret key after swap is CLOSED
     * @param _swapID - ID of certain swap
     */
    function checkSecretKey(bytes32 _swapID) public view 
        onlyClosedSwaps(_swapID) 
        returns (bytes memory secretKey) {
        Swap memory swap = swaps[_swapID];
        return swap.secretKey;
    }
}
