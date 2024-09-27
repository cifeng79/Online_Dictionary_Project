# Online_Dictionary_Project
本项目基于TCP协议的网络在线词典，项目分为服务端和客户端，用户在客户端注册、登录、查询，输入单词可看到单词解释；选择历史记录：可查看查实记录和查询时间；选择退出：可退出，服务器可以接受并处理多个客户端的请求。
## 项目背景
随着移动互联网的兴起，人们对于互联网的依赖，希望生活中的方方面面都可以通过手机、电脑解决。学习也不例外，为了解决传统的词典以及电子词典携带不便的缺点，产生了在线词典的需求。通过注册、登录后，就能轻松在线查询单词，还能保存查询的历史记录。

## 需求分析
### 项目软件框架分析
![yuque_diagram](https://github.com/user-attachments/assets/6aba7f5d-9996-4367-9bd7-f1e168761fd2)
### 通信结构体
``` c
#define N 32

#define R 1   // user - register
#define L 2   // user - login
#define Q 3   // user - query
#define H 4   // user - history

#define  DATABASE  "my.db"

// 定义通信结构体
typedef struct {
	int type;
	char name[N];
	char data[256];
	int flags;
}MSG;
```
