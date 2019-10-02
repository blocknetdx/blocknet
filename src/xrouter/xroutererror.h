//******************************************************************************
//******************************************************************************

#ifndef XROUTERERROR_H
#define XROUTERERROR_H

//******************************************************************************
//******************************************************************************
namespace xrouter
{
    enum Error
    {
      SUCCESS = 0,
      BAD_REQUEST = 1004,
      BAD_VERSION = 1005,
      BAD_CONNECTOR = 1018,
      INVALID_PARAMETERS = 1025,
      BAD_ADDRESS = 1026,
      INSUFFICIENT_FUNDS = 1027,
      INSUFFICIENT_FEE = 1028,
      EXPIRED_PAYMENT_CHANNEL = 1029,
      UNSUPPORTED_BLOCKCHAIN = 1030,
      UNSUPPORTED_SERVICE = 1031,
      NOT_ENOUGH_NODES = 1032,
      MAXFEE_TOO_LOW = 1033,
      TOO_MANY_REQUESTS = 1034,
      UNAUTHORIZED = 1001,
      INTERNAL_SERVER_ERROR = 1002,
      SERVER_TIMEOUT = 1003
    };

    class XRouterError {
    public:
        std::string msg;
        Error code; 
        XRouterError(std::string message, enum Error errcode) : msg(message), code(errcode) { }
    };
} // namespace xrouter

#endif // XROUTERERROR_H
