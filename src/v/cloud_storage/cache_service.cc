/*
 * Copyright 2021 Vectorized, Inc.
 *
 * Licensed as a Redpanda Enterprise file under the Redpanda Community
 * License (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 * https://github.com/vectorizedio/redpanda/blob/master/licenses/rcl.md
 */

#include "cloud_storage/logger.h"
#include "random/generators.h"
#include "utils/gate_guard.h"
#include "vlog.h"

#include <seastar/core/coroutine.hh>
#include <seastar/core/file.hh>
#include <seastar/core/fstream.hh>
#include <seastar/core/future.hh>
#include <seastar/core/print.hh>
#include <seastar/core/seastar.hh>
#include <seastar/core/smp.hh>
#include <seastar/util/defer.hh>

#include <cloud_storage/cache_service.h>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <system_error>

namespace cloud_storage {

cache::cache(
  std::filesystem::path cache_dir,
  size_t max_cache_size,
  ss::lowres_clock::duration check_period) noexcept
  : _cache_dir(std::move(cache_dir))
  , _max_cache_size(max_cache_size)
  , _check_period(check_period)
  , _cnt(0) {}

// deletes cache files starting with oldest
ss::future<> cache::clean_up_cache() {
    gate_guard guard{_gate};
    std::cout << "\nstart walking " << _cache_dir << "\n" << std::flush;
    co_await _walker.walk(_cache_dir.native());
    std::cout << "finished walking! " << _cache_dir << "\n" << std::flush;

    std::cout << "_walker.cache_size: " << _walker.cache_size << "\n"
              << std::flush;
    std::cout << "_max_cache_size: " << _max_cache_size << "\n" << std::flush;
    if (_walker.cache_size >= _max_cache_size) {
        std::cout << "_walker.cache_size >= _max_cache_size\n";
        auto to_delete = _walker.cache_size - (_max_cache_size * 4 / 5);
        std::cout << "to_delete: " << to_delete << "\n" << std::flush;

        auto candidates_for_deletion = _walker.files;

        std::cout << "we have " << candidates_for_deletion.size()
                  << " files to delete\n";
        std::cout << "first for deletion: "
                  << candidates_for_deletion[0].filename << "\n"
                  << std::flush;
        uint64_t deleted = 0;
        size_t i_to_delete = 0;
        while (deleted < to_delete) {
            auto next_to_delete = candidates_for_deletion[i_to_delete];
            i_to_delete++;
            // todo: don't delete .part file that are being written
            // open file exclusively before deleting

            std::cout << "next_to_delete.size: " << next_to_delete.size << "\n"
                      << std::flush;

            bool deleted_filename = true;
            auto file_to_remove = next_to_delete.filename;

            ss::file file_part;
            try {
                co_await ss::remove_file(file_to_remove);
            } catch (std::exception& e) {
                deleted_filename = false;
                std::cout << "couldn't delete " << file_to_remove << ": "
                          << e.what() << " \n"
                          << std::flush;
            }
            if (deleted_filename) {
                std::cout << "successfully deleted " << file_to_remove << "\n"
                          << std::flush;
                deleted += next_to_delete.size;
            }
        }
        total_cleaned += deleted;
    }
}

ss::future<> cache::start() {
    vlog(cst_log.debug, "Starting archival cache service");

    // todo: we can implement more optimal cache eviction
    _timer.set_callback([this] {
        if (ss::this_shard_id() == 0) {
            return clean_up_cache();
        }
        return ss::make_ready_future<>();
    });

    _timer.arm_periodic(_check_period);
    return ss::make_ready_future<>();
}

ss::future<> cache::stop() {
    vlog(cst_log.debug, "Stopping archival cache service");
    _timer.cancel();
    return _gate.close();
}

ss::future<std::optional<cache_item>> cache::get(std::filesystem::path key) {
    gate_guard guard{_gate};
    vlog(cst_log.debug, "Trying to get {} from archival cache.", key.native());
    ss::file cache_file;
    try {
        cache_file = co_await ss::open_file_dma(
          (_cache_dir / key).native(), ss::open_flags::ro);
    } catch (std::filesystem::filesystem_error& e) {
        if (e.code() == std::errc::no_such_file_or_directory) {
            co_return std::nullopt;
        } else {
            throw;
        }
    }

    auto data_size = co_await cache_file.size();
    auto data_stream = ss::make_file_input_stream(cache_file);
    co_return std::optional(cache_item{std::move(data_stream), data_size});
}

ss::future<>
cache::put(std::filesystem::path key, ss::input_stream<char>& data) {
    gate_guard guard{_gate};
    vlog(cst_log.debug, "Trying to put {} to archival cache.", key.native());

    auto filename = (_cache_dir / key).filename();
    auto dir_path = (_cache_dir / key).remove_filename();
    co_await ss::recursive_touch_directory(dir_path.string());

    // tmp file is used to protect against concurrent writes to the same file.
    // One tmp file is written only once by one thread. tmp file should not be
    // read directly. _cnt is an atomic counter that ensures the uniqueness of
    // names for tmp files within one shard, while shard_id ensures uniqueness
    // across multiple shards.
    auto tmp_filename = std::filesystem::path(ss::format(
      "{}_{}_{}.part", filename.string(), ss::this_shard_id(), (++_cnt)));
    auto flags = ss::open_flags::wo | ss::open_flags::create
                 | ss::open_flags::exclusive;
    auto tmp_cache_file = co_await ss::open_file_dma(
      (dir_path / tmp_filename).native(), flags);
    auto out = co_await ss::make_file_output_stream(tmp_cache_file);

    co_await ss::copy(data, out)
      .then([&out]() { return out.flush(); })
      .finally([&out]() { return out.close(); });

    // commit write transaction
    co_await ss::rename_file(
      (dir_path / tmp_filename).native(), (dir_path / filename).native());
}

ss::future<bool> cache::is_cached(const std::filesystem::path& key) {
    gate_guard guard{_gate};
    vlog(cst_log.debug, "Checking {} in archival cache.", key.native());
    return ss::file_exists((_cache_dir / key).native());
}

ss::future<> cache::invalidate(const std::filesystem::path& key) {
    gate_guard guard{_gate};
    vlog(
      cst_log.debug,
      "Trying to invalidate {} from archival cache.",
      key.native());
    try {
        co_await ss::remove_file((_cache_dir / key).native());
    } catch (std::filesystem::filesystem_error& e) {
        if (e.code() == std::errc::no_such_file_or_directory) {
            co_return;
        } else {
            throw;
        }
    }
};

} // namespace cloud_storage
