/*
 * Copyright 2021 Vectorized, Inc.
 *
 * Licensed as a Redpanda Enterprise file under the Redpanda Community
 * License (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 * https://github.com/vectorizedio/redpanda/blob/master/licenses/rcl.md
 */

#include "bytes/iobuf.h"
#include "cloud_storage/cache_service.h"
#include "units.h"

#include <seastar/core/app-template.hh>
#include <seastar/core/file-types.hh>
#include <seastar/core/fstream.hh>
#include <seastar/core/seastar.hh>
#include <seastar/core/thread.hh>

#include <iostream>

int main(int args, char** argv, char** env) {
    ss::app_template app;
    cloud_storage::cache cache_service(
      std::filesystem::path("/home/lena/work/lenaan/redpanda/test_dir"),
      100_GiB,
      ss::lowres_clock::duration(100));
    return app.run(args, argv, [&] {
        return ss::async([&] {
            cache_service.start().get();

            std::filesystem::path key("/home/lena/work/lenaan/redpanda/vbuild/"
                                      "release/clang/lenkin_cache.txt");

            std::filesystem::path remote_data_key(
              "/home/lena/work/lenaan/redpanda/vbuild/release/clang/"
              "test_remote_data.txt");

            auto remote_file = ss::open_file_dma(
                                 remote_data_key.native(), ss::open_flags::ro)
                                 .get();

            auto is = ss::make_file_input_stream(remote_file);
            // cache_service.put(key, std::move(is)).get();

            // if (cache_service.is_cached(key).get()) {
            //     std::cout << key << " is cached\n";
            //     std::optional<cloud_storage::cache_item> returned_item
            //       = cache_service.get(key).get();
            //     std::cout << key << " has size " << returned_item->size << "\n";
            //     std::cout << returned_item->body.read().get().get() << "\n";
            // }

            cache_service.invalidate(key).get();
            std::cout << key << " is invalidated\n";

            if (cache_service.is_cached(key).get()) {
                std::cout << key << " is cached\n";
                std::optional<cloud_storage::cache_item> returned_item
                  = cache_service.get(key).get();
                std::cout << key << " has size " << returned_item->size << "\n";
                std::cout << returned_item->body.read().get().get() << "\n";
            } else {
                std::cout << key << " is not cached\n";
            }

            cache_service.stop().get();
        });
    });
}
