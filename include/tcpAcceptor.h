#include <func.h>
// tcp连接的过程就是返回 一个可用的 listen_fd
// 步骤是create bind listen
// 我需要知道的参数 本地地址 端口号
class TcpAcceptor
{
private:
    int listen_fd;
    int _port;

public:
    TcpAcceptor(int port)
        : listen_fd(-1),
          _port(port) {

          };
    // 获取ip地址  port 返回 listen_fd
    bool init()
    {

        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd<0)
        {
            perror("socket");
            return false;
        }
        // 设置so_reuseAddr;接口
        int opt = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        // 绑定地址
        struct sockaddr_in addr;
        bzero(&addr, 0);
        int ret = 0;
        addr.sin_port = htons(_port);
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        ret = bind(listen_fd, (const sockaddr *)&addr, sizeof(addr));
        if (ret)
        {
            perror("bind");
            close(listen_fd);
            return false;
        }
        // 开始监听
        if (listen(listen_fd, 10) < 0)
        {
            perror("listen");
            close(listen_fd);
            return false;
        }
        std::cout << "TCP acceptor init on port" << _port << std::endl;
        return true;
    }

    int accept_Connection()
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            perror("acceept");
            return -1;
        }
        return client_fd;
    }
    int getListenFd() const { return listen_fd; }
//启动连接 
void start()
{
  init();
  
}



    ~TcpAcceptor()
    {
        if (listen_fd >= 0)
        {
            close(listen_fd);
        }
    }
};
