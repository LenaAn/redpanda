/*
 * Copyright 2020 Redpanda Data, Inc.
 *
 * Use of this software is governed by the Business Source License
 * included in the file licenses/BSL.md
 *
 * As of the Change Date specified in that file, in accordance with
 * the Business Source License, use of this software will be governed
 * by the Apache License, Version 2.0
 */
#pragma once
#include "bytes/iobuf.h"
#include "kafka/protocol/errors.h"
#include "kafka/protocol/schemata/describe_acls_request.h"
#include "kafka/protocol/schemata/describe_acls_response.h"
#include "kafka/types.h"
#include "model/fundamental.h"
#include "model/metadata.h"
#include "model/timestamp.h"

#include <seastar/core/future.hh>

namespace kafka {

struct describe_acls_response;

class describe_acls_api final {
public:
    using response_type = describe_acls_response;

    static constexpr const char* name = "describe_acls";
    static constexpr api_key key = api_key(29);
};

struct describe_acls_request final {
    using api_type = describe_acls_api;

    describe_acls_request_data data;

    void encode(response_writer& writer, api_version version) {
        data.encode(writer, version);
    }

    void decode(request_reader& reader, api_version version) {
        data.decode(reader, version);
    }
};

inline std::ostream&
operator<<(std::ostream& os, const describe_acls_request& r) {
    return os << r.data;
}

struct describe_acls_response final {
    using api_type = describe_acls_api;

    describe_acls_response_data data;

    void encode(response_writer& writer, api_version version) {
        data.encode(writer, version);
    }

    void decode(iobuf buf, api_version version) {
        data.decode(std::move(buf), version);
    }
};

inline std::ostream&
operator<<(std::ostream& os, const describe_acls_response& r) {
    return os << r.data;
}

} // namespace kafka
