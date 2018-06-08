//*****************************************************************************
//*****************************************************************************

#ifndef AUTOTRUNCATELOG_H
#define AUTOTRUNCATELOG_H

#include <boost/lexical_cast.hpp>
using boost::lexical_cast;

#include <string>

namespace {
    constexpr size_t ONE_MB{1024*1024};
    constexpr size_t BLOCKS_PER_DAY{1440};

    // check debug.log file size only twice a day
    constexpr size_t BLOCKS_BETWEEN_CHECKS{BLOCKS_PER_DAY/2};

    constexpr size_t MIN_MB{0};
    constexpr size_t MAX_MB{1024};
    constexpr size_t THRESHOLD_MB{1024};
    constexpr size_t DEF_SERVICENODE{0};
    constexpr size_t DEF_NON_SERVICENODE{512};

    constexpr bool inRange(size_t v) {
        return (v <= MAX_MB /* && v >= MIN_MB */);
    }

    constexpr bool checkNow(size_t height) {
        return (height % BLOCKS_BETWEEN_CHECKS) == 0;
    }

    static_assert(inRange(THRESHOLD_MB), "");
    static_assert(inRange(DEF_SERVICENODE), "");
    static_assert(inRange(DEF_NON_SERVICENODE), "");
    static_assert(DEF_SERVICENODE <= THRESHOLD_MB, "");
    static_assert(DEF_NON_SERVICENODE <= THRESHOLD_MB, "");
} // anonymous

/**
 * @brief specifies the threshold in bytes that triggers truncation
 *        and the size of the file after truncation
 */
class TruncateSizes {
 public:
    TruncateSizes() = default;
    TruncateSizes(size_t bigSz, size_t newSz)
     : bigSz(bigSz), newSz(std::min(bigSz,newSz)) {}
    void disable() { bigSz = 0; }
    bool enabled() const { return bigSz != 0; }
    bool tooBig(size_t curSz) const {
        return enabled() && curSz >= bigSz;
    }
    size_t bigSize() const { return bigSz; }
    size_t newSize() const { return newSz; }

 private:
    size_t bigSz{0};
    size_t newSz{0};
};

/**
 * @brief Implements the "-autotruncatelog" option
 */
class AutoTruncateLog final {
public:
    AutoTruncateLog() = default;
    AutoTruncateLog(size_t bigSz, size_t newSz) : m_truncSz{bigSz, newSz} {}

    /**
     * @brief Deferred initialization is used when instance is global and configuration
     *        args are not available for the default constructor.
     *        Only called if "autotruncatelog" is specified
     *
     * @param[in] sval is the value of the "autotruncatelog" config option
     *            an empty string means use default
     * @param[in] isServicenode Is used to set defaults when the value of the
     *            "autotruncatelog" config option is an empty string
     *
     * @return 0= success, 1= sval is not in valid range, 2= lexical_cast exception
     */
    int init(const std::string& sval, bool isServicenode);

    /**
     * @brief Called on every accepted new block with the current @p height of the blockchain
     * @param[in] height (optional) The @p height is used as a low resolution "clock"
     *            so as to periodically check the "debug.log" file size and truncate
     *            when it gets too big.  The threshold is currently hard-coded as a
     *            constexpr THRESHOLD_MB (set to 1 GB).
     *            A forced check is performed if @p height is omitted, or has a value of zero
     */
    void check(size_t height = 0);

    /**
     * @brief Indicates whether the "-autotruncatelog" option is enabled
     * @return true, if enabled
     */
    bool enabled() const { return m_truncSz.enabled(); }

private:
    TruncateSizes m_truncSz;
    size_t m_lastHeightChecked{0};
};

#endif // AUTOTRUNCATELOG_H
