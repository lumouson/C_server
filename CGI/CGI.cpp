#define _CRT_SECURE_NO_WARNINGS 1
#include<stdio.h>
#include<string.h>
#include<windows.h>
//使用匿名管道实现通讯

int main()
{
	//管道两端的句柄
	HANDLE output[2];

	//Pipe's property
	SECURITY_ATTRIBUTES la;
	la.nLength = sizeof(la);
	//是否能被继承
	la.bInheritHandle = true;
	//安全描述符,0表示默认值
	la.lpSecurityDescriptor = 0;
	//创建管道
	BOOL bCreate = CreatePipe(&output[0], &output[1], &la,0);//read input
	if (bCreate == false)
	{
		MessageBox(0, "creat cgi pipe error", 0, 0);
		return 1;
	}

	//创建子进程
	char cmd[] = "ping www.baidu.com";
	//child process'property
	STARTUPINFO si = { 0 };
	si.cb = sizeof(si);
	si.hStdOutput = output[1];
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	
	PROCESS_INFORMATION pi = { 0 };
	bCreate = CreateProcess(NULL, cmd, 0, 0, TRUE, 0, 0, 0, &si, &pi);
	if (bCreate == false)
	{
		printf("创建进程失败");
		return 1;
	}
	char buff[1024] = "0";
	DWORD size;
	while (1)
	{
		//printf("请输入:");
		//gets_s(buff, sizeof(buff));

		//WriteFile(output[1], buff, strlen(buff) + 1, &size, NULL);
		//printf("已经写入%d字节\n", size);
		
		ReadFile(output[0], buff, sizeof(buff), &size, NULL);
		printf("已经读到了%d个字节[%s]\n", size, buff);

	}

	return 0;
}