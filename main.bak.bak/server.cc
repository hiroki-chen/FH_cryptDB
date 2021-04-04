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
#include <main/server.hh>

int main()
{
    int sockfd_server;
    int sockfd;
    int fd_temp;
    struct sockaddr_in s_addr_in;
    struct sockaddr_in s_addr_client;
    int client_length;

    sockfd_server = socket(AF_INET,SOCK_STREAM,0);  //ipv4,TCP
    assert(sockfd_server != -1);

    //before bind(), set the attr of structure sockaddr.
    memset(&s_addr_in,0,sizeof(s_addr_in));		 //清空s_addr_in
    s_addr_in.sin_family = AF_INET;					 //表示ipv4
    s_addr_in.sin_addr.s_addr = htonl(INADDR_ANY);  //绑定默认ip trans addr from uint32_t host byte order to network byte order.
    s_addr_in.sin_port = htons(SOCK_PORT);          //绑定端口 trans port from uint16_t host byte order to network byte order.
    fd_temp = bind(sockfd_server,(const struct sockaddr *)(&s_addr_in),sizeof(s_addr_in));//绑定

    //bind出错
    if(fd_temp == -1){
        fprintf(stderr,"bind error!\n");
        exit(1);
    }

    fd_temp = listen(sockfd_server,MAX_CONN_LIMIT);//在TCP中listen把进程变成一个服务器，并指定相应的套接字为被动连接。
    if(fd_temp == -1){
        fprintf(stderr,"listen error!\n");
        exit(1);
    }

    const char* host = "localhost";
    const char* user = "root";
    const char* pwd = "letmein";
    const char* dbname = "test";
    const char* master_key = "123456";

    std::string emdbpath = "./shadow"; // 嵌入数据库路径

    // 等待客户的连接加密机的请求
    bool connect = false;
    while(!connect){
        printf("waiting for new connection...\n");
        client_length = sizeof(s_addr_client);
        //Block here. Until server accpets a new connection.
        sockfd = accept(sockfd_server,(struct sockaddr*)(&s_addr_client),(socklen_t *)(&client_length));
        if(sockfd == -1){
            fprintf(stderr,"Accept error!\n");
            continue;                               //ignore current socket ,continue while loop.
        }
        printf("A new connection occurs!\n");

        int fd_conn = *((int *)&sockfd);
        int i_recvBytes_conn;
        // 存储传输数据
        struct SizeType* sizetype_recv_conn;
        struct SizeType* sizetype_send_conn;
        char sizetype_buff_conn[SIZETYPE_SIZE]; //传输数据大小和类型的缓冲区
        char* data_recv_conn;    // 存储接收的数据
        int data_recv_size_conn;
        char* data_send_conn;   // 存储发送的数据
        int data_send_size_conn;

        printf("waiting for ConnectInfo...\n");

            //第一次交互接受客户端传输数据的大小和类型
            memset(sizetype_buff_conn, 0, SIZETYPE_SIZE);
            i_recvBytes_conn = recv(fd_conn, sizetype_buff_conn, SIZETYPE_SIZE, 0);
            if(i_recvBytes_conn == 0){
                printf("Maybe the client has closed\n");
                continue;
            }
            if(i_recvBytes_conn == -1){
                fprintf(stderr,"read error!\n");
                continue;
            }
            sizetype_recv_conn = (struct SizeType*)sizetype_buff_conn;
            printf("read from client : size: %d  type: %d\n", sizetype_recv_conn->size, sizetype_recv_conn->type);

            //第二次交互接受客户端传输数据
            data_recv_size_conn = sizetype_recv_conn->size;
            data_recv_conn = (char*)malloc(data_recv_size_conn);
            memset(data_recv_conn, 0, data_recv_size_conn);
            i_recvBytes_conn = recv(fd_conn, data_recv_conn, data_recv_size_conn, 0);
            if(i_recvBytes_conn == 0){
                printf("Maybe the client has closed\n");
                continue;
            }
            if(i_recvBytes_conn == -1){
                fprintf(stderr,"read error!\n");
                continue;
            }

            // 建立连接成功，则等待接收SQL和Result
            if(sizetype_recv_conn->type == TYPE_CONNECT){
            	// 连接信息初始化
            	struct ConnInfo* conninfo = (struct ConnInfo*)data_recv_conn;
                host = getHost(conninfo);
                user = getUser(conninfo);
                pwd = getPwd(conninfo);
                dbname = getDbname(conninfo);
                //master_key = getMasterkey(conninfo);
                master_key = "91839383801830";
                const char* conn_return = "connection established";
            	data_send_size_conn = strlen(conn_return) + 1;//sizetype_recv->size;

            	data_send_conn = (char*)malloc(data_send_size_conn);
    			memset(data_send_conn, 0, data_send_size_conn);
    			memcpy(data_send_conn, conn_return, data_send_size_conn);
    			sizetype_send_conn = prepareSizeType(data_send_size_conn, sizetype_recv_conn->type);
    			connect = true;
            }
            else{ // 首次请求不是建立连接则继续等待建立连接
                const char* conn_return = "connection not established";
            	data_send_size_conn = strlen(conn_return) + 1;//sizetype_recv->size;
            	data_send_conn = (char*)malloc(data_send_size_conn);
    			memset(data_send_conn, 0, data_send_size_conn);
    			memcpy(data_send_conn, conn_return, data_send_size_conn);
    			sizetype_send_conn = prepareSizeType(data_send_size_conn, sizetype_recv_conn->type);
            }

            // 第一次交互发送data_send_conn的大小和类型
            if(send(fd_conn, (char*)sizetype_send_conn, SIZETYPE_SIZE, 0) == -1){
            	std::cout << "发送客户端失败" << std::endl;
            	connect = false;
            	continue;
            }
            std::cout << "data_send_size_conn: " << data_send_size_conn << std::endl;
            // 第二次交互发送data_send_conn
            if(send(fd_conn, data_send_conn, data_send_size_conn, 0) == -1){
            	std::cout << "发送客户端失败" << std::endl;
            	connect = false;
            	continue;
            }
        //Clear
        printf("terminating current client_connection...\n");
        close(fd_conn);
    }

    ConnectionInfo ci(host, user, pwd);
    ProxyState proxy(ci, emdbpath, master_key); // ps=连接的状态信息，emdbpath=嵌入数据库路径
    mainConnect(proxy, dbname); // 完成连接数据库的工作


    // 等待用户的SQL和结果集
    while(1){
        printf("waiting for new connection...\n");
        client_length = sizeof(s_addr_client);
        //Block here. Until server accpets a new connection.
        sockfd = accept(sockfd_server,(struct sockaddr*)(&s_addr_client),(socklen_t *)(&client_length));
        if(sockfd == -1){
            fprintf(stderr,"Accept error!\n");
            continue;                               //ignore current socket ,continue while loop.
        }
        printf("A new connection occurs!\n");

        int fd = *((int *)&sockfd);
        int i_recvBytes;
        // 存储传输数据
        struct SizeType* sizetype_recv;
        struct SizeType* sizetype_send;
        char sizetype_buff[SIZETYPE_SIZE]; //传输数据大小和类型的缓冲区
        char* data_recv;    // 存储接收的数据
        int data_recv_size;
        char* data_send;   // 存储发送的数据
        int data_send_size;

        printf("waiting for SQL or Result...\n");

            //第一次交互接受客户端传输数据的大小和类型
            memset(sizetype_buff, 0, SIZETYPE_SIZE);
            i_recvBytes = read(fd, sizetype_buff, SIZETYPE_SIZE);
            if(i_recvBytes == 0){
                printf("Maybe the client has closed\n");
                continue;
            }
            if(i_recvBytes == -1){
                fprintf(stderr,"read error!\n");
                continue;
            }
            sizetype_recv = (struct SizeType*)sizetype_buff;
            printf("read from client : size: %d  type: %d\n", sizetype_recv->size, sizetype_recv->type);

            //第二次交互接受客户端传输数据
            data_recv_size = sizetype_recv->size;
            data_recv = (char*)malloc(data_recv_size);
            memset(data_recv, 0, data_recv_size);
            i_recvBytes = read(fd, data_recv, data_recv_size);
            if(i_recvBytes == 0){
                printf("Maybe the client has closed\n");
                continue;
            }
            if(i_recvBytes == -1){
                fprintf(stderr,"read error!\n");
                continue;
            }

            // 传输数据是QUERY
            if(sizetype_recv->type == TYPE_QUERYONE){
                std::cout << "read from client: " << data_recv << std::endl;
                struct SQL* new_query_send = RewriteSQLs(data_recv, proxy);
            	data_send_size = new_query_send->sql_size;
            	data_send = (char*)new_query_send;
//            	data_send = (char*)malloc(data_send_size);
//    			memset(data_send, 0, data_send_size);
//    			memcpy(data_send, new_query_send, data_send_size);
    			sizetype_send = prepareSizeType(data_send_size, sizetype_recv->type);
            }
            else{ // 传输数据是Result
            	std::cout << "read from client encrypt result" << std::endl;
            	printResult((struct Result*)data_recv);
            	struct Result* result_recv = CharsToResult(data_recv); // char* 转 Result*
            	char* query = ResultToQuery(CharsToResult(data_recv)); // 获得query
            	struct Result* result_send = DecryptResults(result_recv, query, proxy); // 解密结果，返回明文Result*
            	data_send_size = result_send->result_size; // 明文结果集大小
            	data_send = (char*)malloc(data_send_size); // 分配缓冲区。返回给客户端
    			memset(data_send, 0, data_send_size);
            	data_send = ResultToChars(result_send);
            	sizetype_send = prepareSizeType(data_send_size, sizetype_recv->type);
            }

            // 第一次交互发送data_send的大小和类型
            if(write(fd, (char*)sizetype_send, SIZETYPE_SIZE) == -1){
            	std::cout << "发送客户端失败" << std::endl;
            	continue;
            }
            std::cout << "data_send_size: " << data_send_size << std::endl;
            // 第二次交互发送data_send
            if(write(fd, data_send, data_send_size) == -1){
            	std::cout << "发送客户端失败" << std::endl;
            	continue;
            }

        //Clear
        printf("terminating current client_connection...\n");
        close(fd);
    }

    //Clear
    int ret = shutdown(sockfd_server,SHUT_WR); //shut down the all or part of a full-duplex connection.
    assert(ret != -1);

    printf("Server shuts down\n");
    return 0;
}

struct SizeType* prepareSizeType(int size_in, int type_in){
	struct SizeType* sizetype = (struct SizeType*)malloc(SIZETYPE_SIZE);
    memset(sizetype, 0, SIZETYPE_SIZE);
    sizetype->size = size_in;
    sizetype->type = type_in;
    std::cout << "prepareSizeType完毕 " << sizetype->size << std::endl;
    std::cout << "prepareSizeType完毕 " << sizetype->type << std::endl;
    return sizetype;
}

char* ResultToChars(struct Result* result){
	return (char*)result;
}

struct Result* CharsToResult(char* char_result){
	std::cout << "in CharsToResult" << std::endl;
	return (struct Result*)char_result;
}

char* ResultToQuery(struct Result* result){
	std::cout << "in ResultToQuery" << std::endl;
	return result->result;
}

const char* getHost(struct ConnInfo* conninfo){
	return (char*)(conninfo->conninfo + conninfo->host_index);
}
const char* getUser(struct ConnInfo* conninfo){
	return (char*)(conninfo->conninfo + conninfo->user_index);
}
const char* getPwd(struct ConnInfo* conninfo){
	return (char*)(conninfo->conninfo + conninfo->pwd_index);
}
const char* getDbname(struct ConnInfo* conninfo){
	return (char*)(conninfo->conninfo + conninfo->dbname_index);
}
const char* getMasterkey(struct ConnInfo* conninfo){
	return (char*)(conninfo->conninfo + conninfo->masterkey_index);
}
