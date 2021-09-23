/*
 * Copyright 2021 Vectorized, Inc.
 *
 * Licensed as a Redpanda Enterprise file under the Redpanda Community
 * License (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 * https://github.com/vectorizedio/redpanda/blob/master/licenses/rcl.md
 */

#pragma once

#include "cloud_storage/recursive_directory_walker.h"
#include "seastarx.h"

#include <seastar/core/future.hh>
#include <seastar/core/gate.hh>
#include <seastar/core/iostream.hh>

#include <filesystem>

namespace cloud_storage {

struct cache_item {
    ss::input_stream<char> body;
    size_t size;
};

class cache {
public:
    /// C-tor.
    ///
    /// \param cache_dir is a directory where cached data is stored
    cache(
      std::filesystem::path cache_dir,
      size_t _max_cache_size,
      ss::lowres_clock::duration _check_period) noexcept;

    ss::future<> start();
    ss::future<> stop();

    /// Get cached value as a stream if it exists on disk
    ss::future<std::optional<cache_item>> get(std::filesystem::path key);

    /// Add new value to the cache, overwrite if it's already exist
    ss::future<> put(std::filesystem::path key, ss::input_stream<char>& data);

    /// Return true if the following object is already in the cache
    ss::future<bool> is_cached(const std::filesystem::path& key);

    /// Remove element from cache by key
    ss::future<> invalidate(const std::filesystem::path& key);

    /// Triggers directory walker, creates a list of files to delete and deletes
    /// them until cache size <= 0.8 * max_cache_size
    ss::future<> clean_up_cache();
    uint64_t total_cleaned;

private:
    std::filesystem::path _cache_dir;
    size_t _max_cache_size;
    ss::lowres_clock::duration _check_period;

    ss::gate _gate;
    uint64_t _cnt;
    ss::timer<ss::lowres_clock> _timer;
    cloud_storage::recursive_directory_walker _walker;
};

} // namespace cloud_storage
