// 需要连接 线程池 如何连接线程池
#include <func.h>
#include "EventDispatcher.h"
#include "Handler.h"
#include "tcpAcceptor.h"
// 如果连接 接听 全写到一个类里面
//  需要有一个构建函数 思考会不会太繁琐了？需要同时 初始化socket epoll  以及 tcp 连接

// 写cpp文件；
// 请求处理上下文
//----------------------------------

class Reactor_server
{
private:                                    // 思考一下有哪几个成员 建立连接肯定有 socket epoll 以及 tcp连接
    TcpAcceptor acceptor;                   // 负责TCP连接
    EventDispatcher dispatcher;             // 负责epoll事件
    std::vector<struct epoll_event> events; // 就序事件；
    thrdpool_t *pool;
    std::map<int, ConnectionInfo> connections;
    HttpRequest req;            // http请求报文；
    std::atomic<bool> _running; // 运行状态
public:
    Reactor_server(int port, int max_events = 1024, int thread_count = 10)
        : acceptor(port),
          dispatcher(max_events),
          _running(false)
    {
        pool = thrdpool_create(thread_count);
        events.resize(thread_count);
    }

    // 初始化listenfd（tcp连接,将listenfd加入epoll)
    bool init()
    {

        // 初始化TCP 只是到liten 没有接受新的连接
        if (!acceptor.init())
        {
            std::cerr << "Failed to initialize TCP acceptor" << std::endl;
            return false;
        }

        // 初始化Epoll 只是创建了一个监听 还没有加入fd
        if (!dispatcher.init())
        {
            std::cerr << "Failed to initialize Epoll manager" << std::endl;
            return false;
        }

        // 将监听socket加入epoll epoll监听fd的来连接信息的话本地都可以知道，需要传好多参数很麻烦
        if (!dispatcher.add_fd(acceptor.getListenFd(), EPOLLIN | EPOLLET))
        {

            std::cerr << "Failed to add listen fd to epoll" << std::endl;
            return false;
        }

        std::cout << "Server initialized successfully" << std::endl;
        return true;
    }
    // 里面是事件循环 处理连接 epoll_wait
    void start()
    {
        _running = true;
        std::cout << "server started" << std::endl;

        // 事件分发循环_________________________________________________________
        int epoll_fd = dispatcher.get_epollFd();
        if (epoll_fd < 0)
        {
            perror("dispatch_ not_initialize");
            return;
        }

        while (_running)
        {
            int max_events = 10;
            int nfds = epoll_wait(epoll_fd, events.data(), max_events, 1000);
            if (nfds < 0)
            {
                if (errno == EINTR)
                {
                    continue; // 被信号中断，继续
                }
                perror("epoll_wait");
                break;
            }

            // 处理就绪事件
            for (int i = 0; i < nfds; i++)
            {
                auto event = events[i];
                // 判断一下
                if (event.data.fd == acceptor.getListenFd()) // 需要的到tcp连接的东西 那么 把他放到另一个类里面进行处理
                {
                    // 将客户端的fd加入监听
                    handle_new_connection();
                }
                else
                {
                    // 处理客户端数据
                    if (events[i].events & EPOLLIN)
                    {
                        handle_client_data(events[i].data.fd);
                    }
                    if (events[i].events & (EPOLLHUP | EPOLLERR))
                    {
                        // 连接错误或关闭
                        close_connection(events[i].data.fd);
                    }
                }
            }
        }
    }

private:
    void handle_new_connection()
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(acceptor.getListenFd(), (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            perror("accept");
            return;
        }

        // 设置非阻塞
        set_nonblocking(client_fd);

        // 创建连接信息对象
        std::string client_ip = inet_ntoa(client_addr.sin_addr);
        int client_port = ntohs(client_addr.sin_port);
        ConnectionInfo info(client_fd, client_ip, client_port);

        connections[client_fd] = info;

        // 将client_fd添加到epoll监听
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET; // 边缘触发
        ev.data.fd = client_fd;
        if (epoll_ctl(dispatcher.get_epollFd(), EPOLL_CTL_ADD, client_fd, &ev) < 0)
        {
            perror("epoll_ctl add client_fd");
            // 如果添加失败，从映射表中移除并关闭连接
            connections.erase(client_fd);
            close(client_fd);
            return;
        }

        std::cout << "新连接: " << client_ip << ":" << client_port << " fd=" << client_fd << std::endl;
    }
    void handle_client_data(int client_fd)
    {
        std::cout << "\n=== 开始处理客户端数据 fd=" << client_fd << " ===" << std::endl;
        char buffer[4096];
        ssize_t bytes_read;

        // 边缘触发模式需要读取所有数据
        while (true)
        {
            bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            std::cout << "recv返回: " << bytes_read << " 字节" << std::endl;
            if (bytes_read > 0)
            {
                buffer[bytes_read] = '\0';
                std::cout << "接收到的数据 (" << bytes_read << "字节):" << std::endl;
                // 打印原始数据（可读性处理）
                for (int i = 0; i < bytes_read; i++)
                {
                    if (buffer[i] == '\r')
                        std::cout << "\\r";
                    else if (buffer[i] == '\n')
                        std::cout << "\\n";
                    else
                        std::cout << buffer[i];
                }
                std::cout << std::endl;

                // 将数据添加到连接缓冲区
                auto it = connections.find(client_fd);
                if (it != connections.end())
                {
                    it->second.buffer.append(buffer, bytes_read);

                    // 检查是否收到完整的HTTP请求
                    std::cout << "完整http吗" << std::endl;
                    if (is_complete_request(it->second.buffer))
                    {
                        // 解析HTTP请求
                        HttpRequest req;
                        req.client_fd = client_fd;

                        if (HttpParser::parseRequest(it->second.buffer, req))
                        {
                            // 清空缓冲区，准备接收下一个请求
                            it->second.buffer.clear();

                            // 创建任务参数
                            TaskArg *task_arg = new TaskArg(req);

                            // 提交任务到线程池
                            if (thrdpool_post(pool, task_handler, task_arg) != 0)
                            {
                                std::cerr << "Failed to post task to thread pool" << std::endl;
                                delete task_arg;
                            }
                            else
                            {
                                std::cout << "Posted task to thread pool: "
                                          << req.method << " " << req.path << std::endl;
                            }
                        }
                    }
                }
            }
            else if (bytes_read == 0)
            {
                // 连接关闭
                close_connection(client_fd);
                break;
            }
            else
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // 数据读取完毕
                    break;
                }
                else
                {
                    // 读取错误
                    perror("recv");
                    std::cout << "错误" << std::endl;
                    close_connection(client_fd);
                    break;
                }
            }
        }
    }

    bool is_complete_request(const std::string &buffer)
    {
        // 简单判断：检查是否有空行分隔头部和主体，并且主体长度符合Content-Length
        size_t header_end = buffer.find("\r\n\r\n");
        if (header_end == std::string::npos)
        {
            return false;
        }

        // 如果有Content-Length，检查主体是否完整
        std::string headers_part = buffer.substr(0, header_end);
        size_t content_length_pos = headers_part.find("Content-Length:");
        if (content_length_pos != std::string::npos)
        {
            size_t line_end = headers_part.find("\r\n", content_length_pos);
            std::string length_str = headers_part.substr(
                content_length_pos + 15, line_end - content_length_pos - 15);

            try
            {
                int content_length = std::stoi(length_str);
                int body_received = buffer.length() - header_end - 4;
                return body_received >= content_length;
            }
            catch (...)
            {
                return false;
            }
        }

        // 没有Content-Length，认为头部结束就是请求结束
        return true;
    }

    void close_connection(int client_fd)
    {
        auto it = connections.find(client_fd);
        if (it != connections.end())
        {
            std::cout << "Connection closed: " << it->second.ip << ":"
                      << it->second.port << " fd=" << client_fd << std::endl;
            connections.erase(it);
        }

        epoll_ctl(dispatcher.get_epollFd(), EPOLL_CTL_DEL, client_fd, nullptr);
        close(client_fd);
    }

    void set_nonblocking(int fd)
    {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    void cleanup()
    {
        // 关闭所有客户端连接
        for (auto &conn : connections)
        {
            close(conn.first);
        }
        connections.clear();

        // 关闭监听socket
        if (acceptor.getListenFd() >= 0)
        {
            close(acceptor.getListenFd());
        }

        // // 关闭epoll
        // if (dispatcher.get_epollFd() = 0)
        // {
        //     close(dispatcher.get_epollFd());
        // }

        // 销毁线程池
        if (pool)
        {
            thrdpool_terminate(pool);
        }
    }
};
