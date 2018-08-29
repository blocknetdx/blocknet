//*****************************************************************************
//*****************************************************************************

#ifndef AUTOTRUNCATELOG_H
#define AUTOTRUNCATELOG_H

#include <boost/lexical_cast.hpp>
using boost::lexical_cast;

#include <limits>
#include <string>

/**
 * @brief Implements the "-autotruncatelog" option
 */
class AutoTruncateLog final {

public:
    static constexpr size_t BIG_MB() { return 1024; }
    static constexpr size_t MAX_MB() { return BIG_MB(); }
    static constexpr size_t SERVICENODE_MB() { return 0; }
    static constexpr size_t NON_SERVICENODE_MB() { return 512; }
    static constexpr size_t CHECKS_PER_DAY() { return 2; }
    static constexpr bool inRange(size_t truncate_mb) {
        return (truncate_mb <= MAX_MB());
    }
    static constexpr bool checkNow(size_t height) {
        return (height % BLOCKS_BETWEEN_CHECKS()) == 0;
    }

    AutoTruncateLog() = default; // disabled by default
    AutoTruncateLog(size_t bigSz, size_t newSz) : m_truncSz{bigSz, newSz} {}

    /**
     * @brief init is called for deferred initialization when an instance is global
     *        and configuration parameters for the constructor are not available.
     *
     * @param[in] sval is the value of the "autotruncatelog" config option,
     *            an empty string means use default
     * @param[in] isServicenode is used to set defaults when the value of the
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
     *            constexpr BIG_MB (set to 1 GB).
     *            A forced check is performed if @p height is omitted, or has a value of zero
     */
    void check(size_t height = 0);

    /**
     * @brief Indicates whether the "-autotruncatelog" option is enabled
     * @return true, if enabled
     */
    bool enabled() const { return m_truncSz.enabled(); }

private:
    static constexpr size_t BLOCKS_PER_DAY() { return 1440; }
    static constexpr size_t BLOCKS_BETWEEN_CHECKS() { return BLOCKS_PER_DAY() / CHECKS_PER_DAY(); }

    class TruncateSizes {
        static constexpr size_t maxBigSz{std::numeric_limits<size_t>::max()};
    public:
        TruncateSizes() = default;
        TruncateSizes(size_t bigSz, size_t newSz)
            : bigSz(bigSz), newSz(std::min(bigSz,newSz)) {}
        void disable() { bigSz = maxBigSz; }
        bool enabled() const { return bigSz != maxBigSz; }
        size_t bigSize() const { return bigSz; }
        size_t newSize() const { return newSz; }

    private:
        size_t bigSz{0};
        size_t newSz{0};
    };

    TruncateSizes m_truncSz;
    size_t m_lastHeightChecked{0};
};

static_assert(AutoTruncateLog::inRange(AutoTruncateLog::BIG_MB()), "");
static_assert(AutoTruncateLog::inRange(AutoTruncateLog::SERVICENODE_MB()), "");
static_assert(AutoTruncateLog::inRange(AutoTruncateLog::NON_SERVICENODE_MB()), "");

#endif // AUTOTRUNCATELOG_H
