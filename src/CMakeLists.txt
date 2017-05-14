CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

project(leveldb)


IF (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	add_compile_options(-Wno-deprecated-declarations -Wno-inaccessible-base -Wno-unused-local-typedefs -Wno-logical-op-parentheses -Wno-shift-negative-value -Wno-array-bounds)
	add_definitions("-DOS_MACOSX -DLEVELDB_PLATFORM_POSIX")
else()	
	add_definitions("-DLEVELDB_PLATFORM_POSIX")
endif()

set(LDB_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/db/builder.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/db/c.cc
##	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/db/db_bench.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/db/db_impl.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/db/db_iter.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/db/dbformat.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/db/dumpfile.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/db/filename.cc
#	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/db/leveldbutil.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/db/log_reader.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/db/log_writer.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/db/memtable.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/db/repair.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/db/table_cache.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/db/version_edit.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/db/version_set.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/db/write_batch.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/port/port_posix.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/table/block_builder.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/table/block.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/table/filter_block.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/table/format.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/table/iterator.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/table/merger.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/table/table.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/table/table_builder.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/table/two_level_iterator.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/util/arena.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/util/bloom.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/util/cache.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/util/coding.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/util/comparator.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/util/crc32c.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/util/env_posix.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/util/env.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/leveldb/util/filter_policy.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/util/hash.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/util/histogram.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/util/logging.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/util/options.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/util/status.cc
	${CMAKE_CURRENT_SOURCE_DIR}/leveldb/helpers/memenv/memenv.cc
)
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/leveldb/
  )


add_library(leveldb ${LDB_SOURCES})



