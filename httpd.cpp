#define _CRT_SECURE_NO_WARNINGS 1
#include<stdio.h>
#include<string.h>

#include<sys/stat.h>
#include<sys/types.h>


#include<WinSock2.h>
#pragma comment(lib,"WS2_32.Lib")

#define PrintLine(str) printf("[%s -> %d] "#str" = %s \n", __func__, __LINE__, str)

//初始化网络,返回套接字
//初始化端口
int StartUp(unsigned short int* port)
{
	WSADATA data;
	int ret = WSAStartup(MAKEWORD(1,1),&data);//1.1版本的协议
	if (ret)
	{
		perror("初始化");
		exit(1);
	}

	//初始化套接字
	int server_socket = socket(PF_INET,//Win 用的套接字
		SOCK_STREAM,//数据流
		IPPROTO_TCP);
	if (server_socket == -1)
	{
		perror("套接字");
		exit(1);
	}
	int opt = 1;

	//设置端口复用
	ret = 0;
	ret = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR,
		(const char*)&opt, sizeof(opt));
	if (ret == -1)
	{
		perror("复用套接字");
		exit(1);
	}

	//设置服务端网络地址,上面都是客户端配置
	sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(*port);//本机字节序,转网络字节序
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	//绑定套接字
	ret = bind(server_socket, (sockaddr*)&server_addr,sizeof(server_addr));
	if (ret < 0)
	{
		perror("绑定套接字");
		exit(1);
	}

	//动态分配一个端口
	int namelen = sizeof(server_addr);
	if (*port == 0)
	{
		//分配的端口号存在了server_addr里了
		ret = getsockname(server_socket, (sockaddr*)&server_addr, &namelen);
		if (ret < 0)
		{
			perror("绑定套接字");
			exit(1);
		}
		*port = server_addr.sin_port;

	}

	//创建监听队列
	ret = listen(server_socket, 5);
	if (ret < 0)
	{
		perror("绑定套接字");
		exit(1);
	}

	return server_socket;
}

//从客户套接字中读取一行数据保存到buff
//返回读了多少个字符
//按照http协议的要求每一行结束返回的是两个字符\r\n
int GetLine(int client,char*buff,int size)
{
	int ret = 0;
	char c = 0;
	int i = 0;
	while (i<size && c!='\n')
	{
		ret = recv(client, &c, 1, 0);
		if (ret > 0)
		{
			if (c == '\r')
			{
				ret = recv(client, &c, 1, MSG_PEEK);//peek偷偷看一眼后面是啥,不读
				if (ret > 0 && c == '\n')
				{
					recv(client, &c, 1, 0);
				}
				else
				{
					c = '\n';
					//continue;
				}
			}
			buff[i] = c;
			i++;
		}
		else
		{
			c = '\n';
		}
	}
	//处理字符串回车符

	buff[i] = 0;
	return i;
}

//想浏览器发送一个未实现的错误报告
void Unimplement(int client)
{
	//to do
}
void not_found(int client) 
{
	char buf[1024];

	sprintf(buf, "HTTP/1.1 404 NOT FOUND\r\n");
	send(client, buf, strlen(buf), 0);

	strcpy(buf, "Server: LumouHttpd/0.1\r\n");
	send(client, buf, strlen(buf), 0);

	sprintf(buf, "Content-Type:text/html\r\n");
	send(client, buf, strlen(buf), 0);

	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	
	sprintf(buf,
		"<HTML>						\
			<TITLE>NO FOUND</TITLE> \
			<BODY>					\
			<H2>The resourse is unavailable.</H2>\
			</BODY>					\
		</HTML>");
	send(client, buf, strlen(buf), 0);
}

//发送响应包的头信息
void Headers(int client)
{
	char buff[1024]="0";

	strcpy(buff, "HTTP/1.1 200 OK\r\n");
	send(client, buff, strlen(buff), 0);

	strcpy(buff, "Server: LumouHttpd/0.1\r\n");
	send(client, buff, strlen(buff), 0);

	strcpy(buff, "Content-type:text/html\n");
	send(client, buff, strlen(buff), 0);

	strcpy(buff, "\r\n");
	send(client, buff, strlen(buff), 0);

}

void cat(int client, FILE*resource)
{
	//一次发4k字节
	char buff[4096]="0";
	int ret = 0, count = 0;
	while (1)
	{
		ret = fread(buff, sizeof(char), 1, resource);
		if (ret <= 0)
			break;
		send(client, buff, ret, 0);
		count += ret;
	}
	printf("一共发送%d个字节", count);
}


void ServerFile(int client, const char* fileName)
{
	int numchar = 1;
	char buff[1024]="0";

	while ((numchar > 0) && strcmp(buff, "\n"))
	{
		numchar = GetLine(client, buff, sizeof(buff));
		PrintLine(buff);
	}

	FILE* resource=NULL;
	if (strcmp(fileName, "hrdocs/index.html") == 0)
	{
		resource = fopen(fileName, "r");
	}
	else
	{
		resource = fopen(fileName, "rb");
	}
	if (resource == NULL)
	{
		not_found(client);
	}

	//发送文件头信息
	Headers(client);
	//发送请求资源信息
	cat(client, resource);
	printf("资源发送完毕\n");
	fclose(resource);
	resource = NULL;
}
//处理用户请求的线程函数
DWORD WINAPI accept_request(LPVOID arg)
{
	char buff[1024]="0";
	int ret = 0;
	int client_sock = (SOCKET)arg;
	int numchar = GetLine(client_sock,buff, sizeof(buff));
	PrintLine(buff);//GET / http/1.1\n
	char method[255] ="0";

	int i = 0, j = 0;
	while (!isspace(buff[i]) && j < sizeof(method) - 1)
	{
		method[j++] = buff[i++];
	}

	method[j] = 0;
	PrintLine(method);
	//检查请求方法是否符合服务器规范
	if (_stricmp(method, "GET")&& _stricmp(method, "POST"))
	{
		Unimplement(client_sock);
		return 0;
		//有请下一位顾客
	}

	//解析资源文件路径
	char url[255]="0";
	j = 0;// / http/1.1\n
	while (isspace(buff[i]) && i < sizeof(buff))
	{
		i++;
	}
	while (!isspace(buff[i]) && (j < sizeof(url) - 1)&&i<sizeof(buff))
	{
		url[j++] = buff[i++];
	}
	url[i] = 0;
	PrintLine(url);

	//设置默认调用index.html文件
	char path[512]="0";
	sprintf(path, "htdocs%s", url);
	if (path[strlen(path) - 1] == '/')
	{
		strcat(path, "index.html");
	}

	PrintLine(path);
	//判断路径文件属性
	struct stat Status;
	ret = stat(path, &Status);
	numchar = 1;
	if (ret < 0)
	{
		//清理get包剩余数据
		while (numchar > 0 && strcmp(buff, "\n"))
		{
			numchar = GetLine(client_sock, buff, sizeof(buff));
		}
	 	not_found(client_sock);
		return 0;
	}
	//如果是目录,默认访问目录下的
	if ((Status.st_mode & S_IFMT) == S_IFDIR)
	{
		strcat(path, "index.html");
	}

	ServerFile(client_sock, path);

	closesocket(client_sock);
	return 0;
}


int main()
{
	unsigned short int port = 80;
	int server_sock = StartUp(&port);
	printf("正在监听 %d 端口\n", port);
	while (1)		//阻塞式等待用户用浏览器访问
	{
		//客户地址
		sockaddr_in client_addr;
		int client_len = sizeof(client_addr);
		int client_sock = accept(server_sock,
			(sockaddr*)&client_addr, 
			&client_len);
		if (client_sock < 0)
		{
			perror("client_sock");
			exit(1);
		}
		//使用用户端套接字与客户交互
		//创建一个新的线程
		DWORD threadId = 0;
		CreateThread(NULL, 0,
			accept_request,
			(void*)client_sock,
			0, &threadId);
	}

	//closesocket(server_sock);
	system("pause");
	return 0;
}