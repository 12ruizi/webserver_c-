#include <func.h>
#include <threadpool/ThreadPool.h>
// connetctionInfo      //__connections
struct HttpRequest
{
  int client_fd;
  std::string method;
  std::string path;
  std::string version;
  std::map<std::string, std::string> headers;
  std::string body;

  // 构建响应
  std::string buildResponse(int status_code, const std::string &status_text,
                            const std::string &content)
  {
    std::string response;
    response = version + " " + std::to_string(status_code) + " " + status_text + "\r\n";
    response += "Content-Type: text/html\r\n";
    response += "Content-Length: " + std::to_string(content.length()) + "\r\n";
    response += "Connection: keep-alive\r\n";
    response += "\r\n";
    response += content;
    return response;
  }
};
// ==================== 连接信息结构体 ====================

class HttpParser
{
public:
  static bool parseRequest(const std::string &raw, HttpRequest &req)
  {
    std::istringstream stream(raw);
    std::string line;

    // 解析请求行
    if (!std::getline(stream, line))
    {
      return false;
    }

    // 移除回车符
    if (!line.empty() && line.back() == '\r')
    {
      line.pop_back();
    }

    std::istringstream line_stream(line);
    line_stream >> req.method >> req.path >> req.version;

    // 解析头部
    while (std::getline(stream, line) && line != "\r")
    {
      if (line.back() == '\r')
      {
        line.pop_back();
      }

      if (line.empty())
        break;

      size_t colon_pos = line.find(':');
      if (colon_pos != std::string::npos)
      {
        std::string key = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 2); // 跳过": "
        req.headers[key] = value;
      }
    }

    // 解析请求体
    auto it = req.headers.find("Content-Length");
    if (it != req.headers.end())
    {
      try
      {
        int content_length = std::stoi(it->second);
        if (content_length > 0)
        {
          req.body.resize(content_length);
          stream.read(&req.body[0], content_length);
        }
      }
      catch (...)
      {
        // 忽略转换错误
      }
    }

    return true;
  }
};
struct TaskArg
{
  HttpRequest request;

  TaskArg(const HttpRequest &req) : request(req) {}
};
void task_handler(void *arg)
{
  TaskArg *task_arg = static_cast<TaskArg *>(arg);
  HttpRequest &req = task_arg->request;

  // 模拟业务处理
  std::cout << "Thread processing: " << req.method << " " << req.path
            << " from fd=" << req.client_fd << std::endl;

  // 简单的路由处理
  std::string response_content;
  if (req.path == "/")
  {
    response_content = "<html><body><h1>Welcome to Server</h1></body></html>";
  }
  else if (req.path == "/api/data")
  {
    response_content = "{\"status\":\"success\",\"data\":\"Hello World\"}";
  }
  else
  {
    response_content = "<html><body><h1>404 Not Found</h1></body></html>";
  }

  // 构建HTTP响应
  std::string response = req.buildResponse(200, "OK", response_content);

  // 发送响应
  send(req.client_fd, response.c_str(), response.length(), 0);

  // 注意：这里不要close(fd)，因为可能是持久连接
  // 清理内存
  delete task_arg;
}