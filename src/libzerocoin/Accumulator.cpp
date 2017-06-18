/**
 * @file       Accumulator.cpp
 *
 * @brief      Accumulator and AccumulatorWitness classes for the Zerocoin library.
 *
 * @author     Ian Miers, Christina Garman and Matthew Green
 * @date       June 2013
 *
 * @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
 * @license    This project is released under the MIT license.
 **/

#include <sstream>
#include "Zerocoin.h"

namespace libzerocoin {

//Accumulator class
Accumulator::Accumulator(const AccumulatorAndProofParams* p, const CoinDenomination d): params(p) {
	if (!(params->initialized)) {
		throw std::runtime_error("Invalid parameters for accumulator");
	}
  denomination = ZerocoinDenominationToValue(d);
	this->value = this->params->accumulatorBase;
}

Accumulator::Accumulator(const ZerocoinParams* p, const CoinDenomination d, const Bignum bnValue) {
	this->params = &(p->accumulatorParams);
  denomination = ZerocoinDenominationToValue(d);

	if (!(params->initialized)) {
		throw std::runtime_error("Invalid parameters for accumulator");
	}

	if(bnValue != 0)
		this->value = bnValue;
	else
		this->value = this->params->accumulatorBase;
}

void Accumulator::accumulate(const PublicCoin& coin) {
	// Make sure we're initialized
	if(!(this->value)) {
		throw std::runtime_error("Accumulator is not initialized");
	}

	if(this->denomination != coin.getDenomination()) {
		//std::stringstream msg;
		std::string msg;
		msg = "Wrong denomination for coin. Expected coins of denomination: ";
		msg += this->denomination;
		msg += ". Instead, got a coin of denomination: ";
		msg += coin.getDenomination();
		throw std::runtime_error(msg);
	}

	if(coin.validate()) {
		// Compute new accumulator = "old accumulator"^{element} mod N
		this->value = this->value.pow_mod(coin.getValue(), this->params->accumulatorModulus);
	} else {
		throw std::runtime_error("Coin is not valid");
	}
}

CoinDenomination Accumulator::getDenomination() const {
	return PivAmountToZerocoinDenomination(this->denomination);
}

const CBigNum& Accumulator::getValue() const {
	return this->value;
}

//Manually set accumulator value
void Accumulator::setValue(CBigNum bnValue) {
	this->value = bnValue;
}

Accumulator& Accumulator::operator += (const PublicCoin& c) {
	this->accumulate(c);
	return *this;
}

Accumulator& Accumulator::operator = (Accumulator rhs) {
    swap(*this, rhs);
    return *this;
}

bool Accumulator::operator == (const Accumulator rhs) const {
	return this->value == rhs.value;
}

//AccumulatorWitness class
AccumulatorWitness::AccumulatorWitness(const ZerocoinParams* p,
                                       const Accumulator& checkpoint, const PublicCoin coin): witness(checkpoint), element(coin) {
}

void AccumulatorWitness::AddElement(const PublicCoin& c) {
	if(element != c) {
		witness += c;
	}
}

const CBigNum& AccumulatorWitness::getValue() const {
	return this->witness.getValue();
}

bool AccumulatorWitness::VerifyWitness(const Accumulator& a, const PublicCoin &publicCoin) const {
	Accumulator temp(witness);
	temp += element;
	return (temp == a && this->element == publicCoin);
}

AccumulatorWitness& AccumulatorWitness::operator +=(
    const PublicCoin& rhs) {
	this->AddElement(rhs);
	return *this;
}

AccumulatorWitness& AccumulatorWitness::operator =(AccumulatorWitness rhs) {
	swap(*this, rhs);
	return *this;
}

} /* namespace libzerocoin */
