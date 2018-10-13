// Copyright (c) 2018 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CURRENCY_H
#define CURRENCY_H

#include <boost/rational.hpp>

#include <array>
#include <cctype>    // toupper
#include <cstdint>
#include <cstring>   // strnlen, memcmp
#include <string>
#include <stdexcept>

namespace ccy {

    /**
     * @brief Symbol - currency symbol in fixed size array (no heap alloc as in std::string).
     *             Maximum length is enforced by the constructor.
     *             Lower case is forced to upper case.
     */
    class Symbol {
        static constexpr size_t MAX_LENGTH{8};
        std::array<char,MAX_LENGTH> _sym{{0}};
    public:
        Symbol() = default;
        Symbol(const char* src, size_t length) {
            if (length < 1 || length > MAX_LENGTH)
                throw std::length_error(std::string{__func__}+": '"+std::string{src,length}+
                                        "' empty or exceeds max length of "+std::to_string(MAX_LENGTH));
            for (size_t i=0; i < length && src[i]; ++i)
                _sym[i] = ::toupper(src[i]);
        }
        Symbol(const std::string& src) : Symbol(src.data(),src.length()) {}
        Symbol(const char* src) : Symbol(src,::strnlen(src,MAX_LENGTH+1)) {}
        template<size_t length> Symbol(const char (&src)[length]) : Symbol(src,length) {}
        static inline std::string validate(const std::string& raw) {
            std::string out{};
            try {
                out = Symbol{raw}.to_string();
            } catch (const std::exception& /* e */) {
                return {};
            }
            return out;
        }
        const void* vdata() const { return reinterpret_cast<const void*>(_sym.data()); }
        std::string to_string() const {
            return {_sym.data(),::strnlen(_sym.data(),MAX_LENGTH)};
        }
        bool operator==(const Symbol& other ) const {
            return ::memcmp(vdata(), other.vdata(), MAX_LENGTH) == 0;
        }
        bool operator!=(const Symbol& other ) const {
            return not (*this == other);
        }
        bool operator<(const Symbol& other ) const {
            return ::memcmp(vdata(), other.vdata(), MAX_LENGTH) < 0;
        }
    };

    using Amount = uint64_t;
    using Basis  = uint64_t;

    template<typename T> constexpr T pow10(size_t x) {
        return x ? 10*pow10<T>(x-1) : 1;
    }

    class Currency {
        Symbol _sym{};
        Basis _basis{0};
    public:
        Currency() = default;
        Currency(const std::string& sym, Basis basis)
            : _sym{sym}, _basis{basis}
        {
            if (not inRange())
                throw std::out_of_range(
                    "Currency basis="+std::to_string(basis)+" must be in range {"+
                    std::to_string(min_basis())+".."+
                    std::to_string(max_basis())+"}");
        }
        static inline constexpr Basis min_basis() { return pow10<Basis>( 0); }
        static inline constexpr Basis max_basis() { return pow10<Basis>(18); } // Ethereum
        Basis basis() const { return _basis; }
        bool inRange() const { return basis() >= min_basis() && basis() <= max_basis(); }
        std::string to_string() const { return _sym.to_string(); }
        bool operator==(const Currency& other ) const {
            return _sym == other._sym && basis() == other.basis();
        }
        bool operator!=(const Currency& other ) const { return not (*this == other); }
        bool operator<(const Currency& other ) const { return _sym < other._sym; }
    };


/**
 * @brief Asset - keeps track of an amount and the associated currency
 */
    class Asset {
        // variables
        Currency _currency{};
        Amount _amount{0};

    public:

        template<typename T>
        class Price {
            T _value;
        public:
            Price() : _value{0} {}
            Price(const Asset& to, const Asset& from) {
                if (from.accumulator() == 0) return;
                auto ratio = to.amount_rational() / from.amount_rational();
                using Rational = decltype(ratio);
                using IntType = Rational::int_type;
                IntType basis = to.currency().basis();
                auto roundUp = Rational{1,2*basis};
                auto rounded = boost::rational_cast<IntType>(basis * (ratio + roundUp));
                _value = boost::rational_cast<T>(Rational{rounded, basis});
            }
            T value() const { return _value; }
        };

        // ctors
        Asset() = default;
        Asset(const Currency& currency, Amount amount = 0)
            : _currency(currency), _amount(amount)
        {}

        // accessors
        Amount& accumulator() { return _amount; }
        const Amount& accumulator() const { return _amount; }
        const Currency& currency() const { return _currency; }

        template<typename IntType = uintmax_t>
        boost::rational<IntType> amount_rational() const {
            return boost::rational<IntType>{_amount, currency().basis()};
        }

        template<typename T = double> T amount() const {
            return boost::rational_cast<T>(amount_rational());
        }

        Asset& operator+=(const Asset& x) {
            accumulator() += x.accumulator();
            return *this;
        }
    };

} // namespace ccy

#endif // CURRENCY_H
