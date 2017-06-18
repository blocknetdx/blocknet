# Copyright 2013 Corgan Labs
# This file is part of the Zerocoin project
# See LICENSE file or http://opensource.org/licenses/MIT for terms

cmake_minimum_required(VERSION 2.8)
project(zerocoin)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 ")

# Just add Debug info for Debug builds
if (CMAKE_BUILD_TYPE MATCHES "DEBUG")
  add_definitions(-DZEROCOIN_DEBUG)
endif()

# Project-wide directory settings
set(BITCOIN_BIGNUM_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/bitcoin_bignum)
set(ZEROCOIN_INCLUDE_DIR include/zerocoin)

########################################################################
# Shared library generation libzerocoin.so
########################################################################
# Tell compiler where to search for include files
include_directories(
  ${BITCOIN_BIGNUM_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
  ${OPENSSL_INCLUDE_DIR}
)

# List of libraries to link in
#
list(APPEND zerocoin_libs
  ${Boost_LIBRARIES}
  ${OPENSSL_LIBRARIES}
)

set(ZEROCOIN_HEADERS
  Accumulator.h
  AccumulatorProofOfKnowledge.h
  Coin.h
  CoinSpend.h
  Commitment.h
  Denominations.h
  ParamGeneration.h
  Params.h
  SerialNumberSignatureOfKnowledge.h
  ZerocoinDefines.h
)

#
# List of source files to compile
#
list(APPEND zerocoin_sources
  Accumulator.cpp
  AccumulatorProofOfKnowledge.cpp
  Coin.cpp
  Denominations.cpp
  CoinSpend.cpp
  Commitment.cpp
  ParamGeneration.cpp
  Params.cpp
  SerialNumberSignatureOfKnowledge.cpp
)

#
add_library(zerocoin ${ZEROCOIN_HEADERS} ${zerocoin_sources})
target_link_libraries(zerocoin ${zerocoin_libs})

########################################################################
# Executable files
########################################################################

#add_executable(paramgen paramgen.cpp)
#target_link_libraries(paramgen zerocoin ${zerocoin_libs})
