#ifndef XBRIDGEETHENCRIPTOR_H
#define XBRIDGEETHENCRIPTOR_H

#include <string>
#include <vector>
#include <functional>

#include "uint256.h"
#include "hash.h"
#include "util/strencodings.h"

using byte = unsigned char;
using bytes = std::vector<byte>;

template <class T, class Out>
inline void toBigEndian(T value, Out& out)
{
    for (unsigned int i = out.size(), j = 0; i != 0; i--, j++)
        out[i - 1] = *(value.begin() + j);
}

inline bytes toBigEndian(uint256 value) { bytes ret(32); toBigEndian(value, ret); return ret; }
inline bytes toBigEndian(uint160 value) { bytes ret(20); toBigEndian(value, ret); return ret; }

inline bytes asBytes(const std::string & value)
{
    return bytes((const byte*)value.data(), (const byte*)(value.data() + value.size()));
}

inline std::string asString(const bytes & value)
{
    return std::string((const char*)value.data(), (const char*)(value.data() + value.size()));
}

inline std::string as0xString(const std::string & value)
{
    std::string result("0x");
    result.append(std::move(value));
    return result;
}

inline std::string as0xString(const bytes & value)
{
    return as0xString(HexStr(value));
}

inline std::string as0xString(const uint256 & value)
{
    return as0xString(value.ToString());
}

inline std::string as0xStringNumber(const uint256 & value)
{
    std::string number = value.ToString();

    for(auto iter = number.begin(); iter != number.end();)
    {
        if(*iter == '0')
            iter = number.erase(iter);
        else
            break;
    }

    return as0xString(number);
}

inline std::string as0xStringNumberAligned(const uint256 & value)
{
    std::string number = value.ToString();

    for(auto iter = number.begin(); iter != number.end();)
    {
        if(*iter == '0')
        {
            number.push_back(*iter);
            iter = number.erase(iter);
        }
        else
            break;
    }

    return as0xString(number);
}

inline std::string as0xString(const uint160 & value)
{
    return as0xString(value.ToString());
}

// Concatenate the contents of a container onto a vector
template <class T, class U> std::vector<T>& operator+=(std::vector<T>& _a, const U & _b)
{
    for (auto const& i: _b)
        _a.push_back(i);
    return _a;
}
// Concatenate the contents of a container onto a vector, move variant
template <class T, class U> std::vector<T>& operator+=(std::vector<T>& _a, U&& _b)
{
    std::move(_b.begin(), _b.end(), std::back_inserter(_a));
    return _a;
}
// Concatenate two vectors of elements
template <class T>
inline std::vector<T> operator+(const std::vector<T> & _a, const std::vector<T> & _b)
{
    std::vector<T> ret(_a);
    ret += _b;
    return ret;
}
// Concatenate two vectors of elements, moving them
template <class T>
inline std::vector<T> operator+(std::vector<T>&& _a, std::vector<T>&& _b)
{
    std::vector<T> ret(std::move(_a));
    if (&_a == &_b)
        ret += ret;
    else
        ret += std::move(_b);
    return ret;
}

class EthEncoder
{
public:
    static bytes encode(bool value) { return encode(byte(value)); }
    static bytes encode(int value) { return encode(uint256(value)); }
    static bytes encode(size_t value) { return encode(uint256(value)); }
    static bytes encode(const char* value) { return encode(std::string(value)); }
    static bytes encode(byte value) { return bytes(31, 0) + bytes{value}; }
    static bytes encode(const uint256 & value) { return toBigEndian(value); }
    static bytes encode(const uint160 & value) { return toBigEndian(value); }
    static bytes encode(const bytes & value, bool padLeft = true)
    {
        const bytes padding = bytes((32 - value.size() % 32) % 32, 0);
        return padLeft ? padding + value : value + padding;
    }
    static bytes encode(const std::string & value) { return encode(asBytes(value), false); }

    template <class T>
    static bytes encode(const std::vector<T> & value)
    {
        bytes ret;
        for (const auto & v : value)
            ret += encode(v);
        return ret;
    }

    // Encode all args given to function to hex
    template <class FirstArg, class... Args>
    static bytes encodeArgs(const FirstArg & firstArg, const Args &... followingArgs)
    {
        return encode(firstArg) + encodeArgs(followingArgs...);
    }
    static bytes encodeArgs()
    {
        return bytes();
    }

    // Encode function signature
    static bytes encodeSig(const std::string & signature)
    {
        uint256 hash = HashSha3(signature.begin(), signature.end());

        bytes ret(hash.begin(), hash.begin() + 4);

        return ret;
    }
};

//class EthTransaction
//{
//public:
//    std::string nonce;
//    std::string gasPrice;
//    std::string gasLimit;
//    std::string to;
//    std::string value;
//    std::string data;
//    std::string v;
//    std::string r;
//    std::string s;
//};

//class RlpEncoder
//{
//public:
//    std::string encode(const std::string & str);
//    std::string encode(const EthTransaction & transaction, bool toSign);
//    std::string encodeLength(int length, int offset);
//    std::string intToHex(int n);
//    std::string bytesToHex(const std::string & input);
//    std::string removeHexFormatting(const std::string & str);
//    std::string hexToRlpEncode(const std::string & str);
//    std::string hexToBytes(const std::string & str);
//    int char2int(char input);
//    void hex2bin(const char* src, char* target);
//};

#endif // XBRIDGEETHENCRIPTOR_H
