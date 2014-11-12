
#include "core.h"


bool CTxOut::IsDust() const
{
    // DarkCoin: IsDust() detection disabled, allows any valid dust to be relayed.
    // The fees imposed on each dust txo is considered sufficient spam deterrant. 
    return false;
}
