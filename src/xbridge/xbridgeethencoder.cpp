#include "xbridgeethencoder.h"

//string RlpEncoder::encode(const string& str)
//{
//    if(str.size() == 1 && (unsigned char)str[0] < 128)
//        return str;
//    else
//        return encodeLength(str.size(), 128) + str;

//}

//string RlpEncoder::encode(const EthTransaction& transaction, bool toSign)
//{
//    string serialized = hexToRlpEncode(transaction.nonce) +
//                        hexToRlpEncode(transaction.gasPrice) +
//                        hexToRlpEncode(transaction.gasLimit) +
//                        hexToRlpEncode(transaction.to) +
//                        hexToRlpEncode(transaction.value) +
//                        hexToRlpEncode(transaction.data);
//    if(!toSign)
//        serialized += hexToRlpEncode(transaction.v) +
//                      hexToRlpEncode(transaction.r) +
//                      hexToRlpEncode(transaction.s);

//    return hexToBytes(encodeLength(serialized.length(), 192)) + serialized;
//}

//string RlpEncoder::encodeLength(int length, int offset)
//{
//    string temp;
//    if(length < 56)
//    {
//        temp = (char)(length + offset);
//        return temp;
//    }
//    else
//    {
//        string hexLength = intToHex(length);
//        int	lLength = hexLength.length()/2;
//        string fByte = intToHex(offset + 55 + lLength);
//        return fByte + hexLength;
//    }
//}

//string RlpEncoder::intToHex(int n)
//{
//    stringstream stream;
//    stream << std::hex << n;
//    string result(stream.str());
//    if(result.size() % 2)
//        result = "0" + result;
//    return result;
//}

//string RlpEncoder::bytesToHex(const string & input)
//{
//    static const char* const lut = "0123456789ABCDEF";
//    size_t len = input.length();
//    std::string output;
//    output.reserve(2 * len);

//    for (size_t i = 0; i < len; ++i)
//    {
//        const unsigned char c = input[i];
//        output.push_back(lut[c >> 4]);
//        output.push_back(lut[c & 15]);
//    }

//    return output;
//}

//string RlpEncoder::removeHexFormatting(const string & str)
//{
//    if(str[0] == '0' && str[1] == 'x')
//        return str.substr(2, str.length() - 2);
//    return str;
//}

//string RlpEncoder::hexToRlpEncode(const string & str)
//{
//    string res = removeHexFormatting(str);
//    return encode(hexToBytes(res));
//}

//string RlpEncoder::hexToBytes(const string & str)
//{
//    char inp [str.length()] = {};
//    memcpy(inp, str.c_str(), str.length());
//    char dest [sizeof(inp) / 2] = {};
//    hex2bin(inp, dest);

//    return string(dest, sizeof(dest));
//}

//int RlpEncoder::char2int(char input)
//{
//    if(input >= '0' && input <= '9')
//        return input - '0';
//    if(input >= 'A' && input <= 'F')
//        return input - 'A' + 10;
//    if(input >= 'a' && input <= 'f')
//        return input - 'a' + 10;
//}

//void RlpEncoder::hex2bin(const char* src, char* target)
//{
//    while(*src && src[1])
//    {
//        *(target++) = char2int(*src)*16 + char2int(src[1]);
//        src += 2;
//    }
//}
