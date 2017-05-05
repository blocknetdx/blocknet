#ifndef PIVX_ZEROCOIN_H
#define PIVX_ZEROCOIN_H
#include "../libzerocoin/bignum.h"

class CZerocoinMint
{
private:
    CBigNum value;
    int denomination;
    CBigNum randomness;
    CBigNum serialNumber;
    bool isUsed;
    int nHeight;
    int id;

public:
    CZerocoinMint()
    {
        SetNull();
    }

    void SetNull()
    {
        isUsed = false;
        randomness = 0;
        value = 0;
        denomination = -1;
        nHeight = -1;
        id = -1;
    }

    CBigNum GetValue() const { return value; }
    void SetValue(CBigNum value){ this->value = value; }
    int GetDenomination() const { return denomination; }
    void SetDenomination(int denomination){ this->denomination = denomination; }
    int GetHeight() const { return nHeight; }
    void SetHeight(int nHeight){ this->nHeight = nHeight; }
    bool IsUsed() const { return this->isUsed; }
    void SetUsed(bool isUsed){ this->isUsed = isUsed; }
    int GetId() const { return id; }
    void SetId(int id){ this->id = id; }
    CBigNum GetRandomness() const{ return randomness; }
    void SetRandomness(CBigNum rand){ this->randomness = rand; }
    CBigNum GetSerialNumber() const { return serialNumber; }
    void SetSerialNumber(CBigNum serial){ this->serialNumber = serial; }

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(isUsed);
        READWRITE(randomness);
        READWRITE(serialNumber);
        READWRITE(value);
        READWRITE(denomination);
        READWRITE(nHeight);
        READWRITE(id);
    };

};
#endif //PIVX_ZEROCOIN_H
