#include <func.h>
#include <threadpool/ThreadPool.h>
// 连接信息的结构体
// 事件处理类
class EventDispatcher
{
private:
    std::function<void(int, uint32_t)> event_callback; // 事件回调函数
    int epoll_fd;
    int max_events;

public:
    // 正常来写
    EventDispatcher(int max_events = 1024) : epoll_fd(-1), max_events(max_events)
    {
    }

    bool init()
    {

        epoll_fd = epoll_create1(EPOLL_CLOEXEC);
        if (epoll_fd < 0)
        {
            perror("epoll_creat1");
            return false;
        }
        std::cout << epoll_fd << std::endl;
        return true;
    }

    // 添加监听

    int add_fd(int fd, uint32_t events)
    {

        // 事件信息——--————————————————————————————————————————————
        struct epoll_event ev;
        bzero(&ev, 0);
        ev.events = events; // 初始化 该fd监听的事件类型
        ev.data.fd = fd;    // 将连接信息指针存储在事件当中
        // epoll初始化将 sockfd添加到监听______________________________________
        if (epoll_fd < 0)
        {
            std::cout << epoll_fd << std::endl;
        }

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0)
        { // 他底层维系的就绪队列也好，红黑树也好 就是 fd+ev
            perror("epoll_ctl_add");
            return false;
        }
        return true;
    }

    // 得到epoll_fd
    int get_epollFd()
    {
        return epoll_fd;
    }
    //---------------------------------------------------------------------

    // // 获取连接信息
    // connetctionInfo *getConnectionInfo(int fd)
    // {
    //     auto it = _connections.find(fd);
    //     if (it != _connections.end())
    //     {
    //         return &it->second;
    //     }
    //     return nullptr;
    // }
    // // 更新连接最后活动时间
    // void updata_lastActive(int fd)
    // {
    //     auto it = _connections.find(fd);
    //     if (it != _connections.end())
    //     {
    //         it->second.last_active = time(nullptr);
    //     }
    // }
    // // 获取所有连接
    // const std::unordered_map<int, connetctionInfo> &getAllConnections() const
    // {
    //     return _connections;
    // }
    //

};

   
   