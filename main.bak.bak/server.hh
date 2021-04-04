#include<stdlib.h>
#include<pthread.h>
#include<sys/socket.h>
#include<sys/types.h>       //pthread_t , pthread_attr_t and so on.
#include<stdio.h>
#include<netinet/in.h>      //structure sockaddr_in
#include<arpa/inet.h>       //Func : htonl; htons; ntohl; ntohs
#include<assert.h>          //Func :assert
#include<string.h>          //Func :memset
#include<unistd.h>  //Func :close,write,read
#include<mysql/mysql.h>
#include<vector>
#include<string>
#include<memory>
#include<iostream>
#include <main/cdb_test.hh>
#include <util/util.hh>

#define SOCK_PORT 2222      //端口号
#define BUFFER_LENGTH 1024
#define MAX_CONN_LIMIT 512 // MAX connection limit，最大连接数
#define TYPE_RESULT 0      // 操作类型为结果集
#define TYPE_QUERYONE 1    // 操作类型为单SQL语句
#define TYPE_QUERYTWO 2    // 操作类型为双SQL语句
#define TYPE_CONNECT 3     // 操作类型为建立连接
#define SIZETYPE_SIZE sizeof(int) * 2

// 连接信息
struct ConnInfo {
	int  conninfo_size;   // 大小
	int  host_index;      // host下标
	int  user_index;      // user下标
	int  pwd_index;       // pwd下标
	int  dbname_index;    // dbname下标
	int  masterkey_index; // masterkey下标
	char conninfo[0];     // 五合一
//    const char* host = "localhost";     // 地址
//    const char* user = "root";          // 用户名
//    const char* pwd = "letmein";        // 密码
//    const char* dbname = "test";        // 数据库名
//    const char* master_key = "123456"; // 主密钥
};

struct SizeType* prepareSizeType(int size_in, int type_in);
//static void	  Data_handle(void * sock_fd);   //Only can be seen in the file

char* 			  ResultToChars(struct Result* result);
struct Result*   CharsToResult(char* chars);
//		ResType   ResultToResType(struct Result* result);
char*  			  ResultToQuery(struct Result* result);
//struct Result*   ResTypeToResult(ResType& restype, int& size, char* query);

void          	  printResult(struct Result* result);
void          	  printResType(const ResType& restype);

// 获得对应连接信息
const char* getHost(struct ConnInfo* conninfo);
const char* getUser(struct ConnInfo* conninfo);
const char* getPwd(struct ConnInfo* conninfo);
const char* getDbname(struct ConnInfo* conninfo);
const char* getMasterkey(struct ConnInfo* conninfo);
