#include "server/server.h"

// cpp-httplib single-header vendored under llama.cpp's tree.
// The plan-recommended path (rag/third_party/httplib/httplib.h) does not exist;
// we fall back to the known-good copy under llama.cpp's vendor folder.
#include <httplib.h>

#include <cstdio>
#include <string>

namespace mmrag {

Server::Server(int port) : port_(port) {}

void Server::run() {
    httplib::Server svr;

    svr.Get("/health", [](const httplib::Request& /*req*/, httplib::Response& res) {
        res.set_content("{\"status\":\"healthy\"}", "application/json");
    });

    std::printf("Listening on 0.0.0.0:%d\n", port_);
    std::fflush(stdout);

    if (!svr.listen("0.0.0.0", port_)) {
        std::fprintf(stderr, "Failed to bind 0.0.0.0:%d\n", port_);
    }
}

}  // namespace mmrag
