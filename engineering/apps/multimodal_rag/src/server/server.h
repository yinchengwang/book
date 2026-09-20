#ifndef MMRAG_SERVER_SERVER_H
#define MMRAG_SERVER_SERVER_H

namespace mmrag {

class Server {
public:
    explicit Server(int port);
    void run();

private:
    int port_;
};

}  // namespace mmrag

#endif  // MMRAG_SERVER_SERVER_H
