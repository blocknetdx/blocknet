// Copyright (c) 2011-2013 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//
// Unit tests for block-chain checkpoints
//

#include "checkpoints.h"

#include "uint256.h"

#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(Checkpoints_tests)

BOOST_AUTO_TEST_CASE(sanity)
{
    uint256 p1000 = uint256("0x8defd49579d63545f9e8cdda31f8503e0513328ca3f7428f33a915258c764d15");
    uint256 p10000 = uint256("0x6af2431daa7456e4620e9493091648eeaac8ddfd53d8cff8101c26806e301d9a");
    BOOST_CHECK(Checkpoints::CheckBlock(1000, p1000));
    BOOST_CHECK(Checkpoints::CheckBlock(10000, p10000));


    // Wrong hashes at checkpoints should fail:
    BOOST_CHECK(!Checkpoints::CheckBlock(1000, p10000));
    BOOST_CHECK(!Checkpoints::CheckBlock(10000, p1000));

    // ... but any hash not at a checkpoint should succeed:
    BOOST_CHECK(Checkpoints::CheckBlock(1000+1, p10000));
    BOOST_CHECK(Checkpoints::CheckBlock(10000+1, p1000));

    BOOST_CHECK(Checkpoints::GetTotalBlocksEstimate() >= 10000);
}

BOOST_AUTO_TEST_SUITE_END()
