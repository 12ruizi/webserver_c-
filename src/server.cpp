#include "reactor_server.h"

int main()
{
    Reactor_server server(8080, 1024);

    if (!server.init())
    {
        std::cerr << "Initialization failed" << std::endl;
        return 1;
    }

    // 设置信号处理，优雅退出
    signal(SIGINT, [](int)
           {
        std::cout << "\nShutting down server..." << std::endl;
        exit(0); });
    // 启动服务器
    server.start();

    return 0;
}