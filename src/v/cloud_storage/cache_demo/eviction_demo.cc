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
#include <seastar/core/sleep.hh>
#include <seastar/core/thread.hh>

#include <chrono>
#include <cstddef>
#include <iostream>
#include <string>

int main(int args, char** argv, char** env) {
    using namespace std::chrono_literals;
    std::filesystem::path home_dir{
      "/home/lena/work/lenaan/redpanda/vbuild/debug/clang"};
    std::filesystem::path cache_dir{home_dir / "archival_cache"};

    ss::app_template app;
    cloud_storage::cache cache_service(
      cache_dir, 3_KiB, ss::lowres_clock::duration(5s));
    return app.run(args, argv, [&] {
        return ss::async([&] {
            cache_service.start().get();

            std::filesystem::path remote_data_key(
              home_dir / "test_data_1K.txt");

            for (size_t i = 0; i < 8; ++i) {
                auto remote_file = ss::open_file_dma(
                                     remote_data_key.native(),
                                     ss::open_flags::ro)
                                     .get();
                auto is = ss::make_file_input_stream(remote_file);

                std::filesystem::path key(
                  cache_dir / ("abc" + std::to_string(i))
                  / ("lenkin_cache_" + std::to_string(i) + ".txt"));
                cache_service.put(key, is).get();
                auto ret = remote_file.close();

                std::cout << "I've put " << key << "!\n" << std::flush;

                ss::sleep(ss::lowres_clock::duration(3s)).get();
            }

            cache_service.stop().get();
        });
    });
}
