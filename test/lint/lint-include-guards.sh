#!/usr/bin/env bash
#
# Copyright (c) 2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Check include guards.

export LC_ALL=C
HEADER_ID_PREFIX="BITCOIN_"
HEADER_ID_PREFIX2="BLOCKNET_"
HEADER_ID_SUFFIX="_H"

REGEXP_EXCLUDE_FILES_WITH_PREFIX="src/(crypto/ctaes/|leveldb/|secp256k1/|tinyformat.h|univalue/|json/)"

EXIT_CODE=0
for HEADER_FILE in $(git ls-files -- "*.h" | grep -vE "^${REGEXP_EXCLUDE_FILES_WITH_PREFIX}")
do
    HEADER_ID_BASE=$(cut -f2- -d/ <<< "${HEADER_FILE}" | sed "s/\.h$//g" | tr / _ | tr "[:lower:]" "[:upper:]")
    HEADER_ID="${HEADER_ID_PREFIX}${HEADER_ID_BASE}${HEADER_ID_SUFFIX}"
    HEADER_ID2="${HEADER_ID_PREFIX2}${HEADER_ID_BASE}${HEADER_ID_SUFFIX}"
    if [[ $(grep -cE "^#(ifndef|define) ${HEADER_ID}" "${HEADER_FILE}") != 2 && $(grep -cE "^#(ifndef|define) ${HEADER_ID2}" "${HEADER_FILE}") != 2 ]]; then
        echo "${HEADER_FILE} seems to be missing the expected include guard:"
        echo "  #ifndef ${HEADER_ID2}"
        echo "  #define ${HEADER_ID2}"
        echo "  ..."
        echo "  #endif // ${HEADER_ID2}"
        echo
        EXIT_CODE=1
    fi
done
exit ${EXIT_CODE}
