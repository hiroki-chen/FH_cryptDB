#include <main/cdb_test.hh>


int main(){
//	std::cout << "Hello! This test CtyptDB!!!" << std::endl;
//
//    ConnectionInfo ci("localhost", "root", "letmein");
//    const std::string master_key = "123456";
//    std::string emdbpath = "./shadow"; // 嵌入数据库路径
//    std::string dbname = "test";
//    ProxyState ps(ci, emdbpath, master_key); // ps=连接的状态信息，emdbpath=嵌入数据库路径
//
//    std::cout << "ProxyState 创建ok 开始建立连接!!!\n" << std::endl;
//	mainConnect(ps, dbname);
//	std::cout << "\n连接结束 !!!\n" << std::endl;
//
//	std::cout << "\n重写语句 !!!\n" << std::endl;
	char sql[100];
	std::string old_query = "";
	while(1){
		std::cout << "QUERY> ";
		gets(sql);
		old_query = sql;
		std::cout << old_query;
		RewriteSQLs(old_query);
	}

////	std::string old_query = "select * from student";
//	std::cout << "\n=============\nOLD QUERY: " << old_query << std::endl;
//	std::string new_query = mainRewriteSQL(ps, dbname, old_query);
//	std::cout << "\n=============\nNEW QUERY: " << new_query << std::endl;
//	}

//	ResType restype;
//	mainDecryptResult(ps, dbname, old_query, restype);
	return 0;
}

