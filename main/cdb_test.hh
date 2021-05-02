#include <cstdlib>
#include <cstdio>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <set>
#include <list>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

#include <main/Connect.hh>

#include <readline/readline.h>
#include <readline/history.h>

#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>
#include <parser/embedmysql.hh>
#include <parser/stringify.hh>
#include <crypto/ecjoin.hh>
#include <util/errstream.hh>
#include <util/cryptdb_log.hh>

static inline std::string user_homedir()
{
  return getenv("HOME");
}

static inline std::string user_histfile()
{
  return user_homedir() + "/.cryptdb-history";
}

//static void __write_history() {
//    write_history(user_histfile().c_str());
//}

static inline std::string &ltrim(std::string &s)
{
  s.erase(s.begin(), find_if(s.begin(), s.end(), not1(std::ptr_fun<int, int>(isspace))));
  return s;
}

static inline std::string &rtrim(std::string &s)
{
  s.erase(find_if(s.rbegin(), s.rend(), not1(std::ptr_fun<int, int>(isspace))).base(), s.end());
  return s;
}

static inline std::string &trim(std::string &s)
{
  return ltrim(rtrim(s));
}

/** returns true if should stop, to keep looping */
static bool handle_line(ProxyState &ps, const std::string &q, bool pp = true)
{
  if (q == "\\q")
  {
    std::cerr << "Goodbye!\n";
    return false;
  }

  add_history(q.c_str());

  // handle meta inputs 以:load name 标志从文件输入
  if (q.find(":load") == 0)
  {
    std::string filename = q.substr(6);
    trim(filename);
    std::cerr << RED_BEGIN << "Loading commands from file: " << filename << COLOR_END << std::endl;
    std::ifstream f(filename.c_str());
    if (!f.is_open())
    {
      std::cerr << "ERROR: cannot open file: " << filename << std::endl;
    }
    while (f.good())
    {
      std::string line;
      getline(f, line);
      if (line.empty())
        continue;
      std::cerr << GREEN_BEGIN << line << COLOR_END << std::endl;
      if (!handle_line(ps, line))
      {
        f.close();
        return false;
      }
    }
    f.close();
    return true;
  }

  static SchemaCache schema_cache;
  try
  {
    const std::string &default_db =
        getDefaultDatabaseForConnection(ps.getConn());
    /*
       *debug
       */
    std::cout << "在函数handle_line中\ndefault_db: " + default_db << std::endl;
    //      const EpilogueResult &epi_result =
    //          executeQuery(ps, q, default_db, &schema_cache, pp);

    std::list<std::string> out_queryz = RewriteQuery(ps, q, default_db, &schema_cache, pp);
    for (auto it : out_queryz)
    {
      std::cout << it << std::endl; // 输出重写后的SQL语句，以QUERY:为标识
    }
    return true;
    //      if (QueryAction::ROLLBACK == epi_result.action) {
    //          std::cout << GREEN_BEGIN << "ROLLBACK issued!" << COLOR_END
    //                    << std::endl;
    //      }
    //      return epi_result.res_type.success();
  }
  catch (const SynchronizationException &e)
  {
    std::cout << e << std::endl;
    return true;
  }
  catch (const AbstractException &e)
  {
    std::cout << e << std::endl;
    return true;
  }
  catch (const CryptDBError &e)
  {
    std::cout << "Low level error: " << e.msg << std::endl;
    return true;
  }
  catch (const std::runtime_error &e)
  {
    std::cout << "Unexpected Error: " << e.what() << std::endl;
    return false;
  }
}

static std::string handle_sql(ProxyState &ps, const std::string &q, bool pp = true)
{
  static SchemaCache schema_cache;
  try
  {
    std::cout << "in handle_sql\n"
              << std::endl;
    const std::string &default_db =
        getDefaultDatabaseForConnection(ps.getConn());
    std::cout << "in handle_sql1\n"
              << std::endl;
    std::cout << "在函数handle_sql中\ndefault_db: " + default_db << std::endl;
    std::cout << "old_query: " + q << std::endl;
    std::list<std::string> out_queryz = RewriteQuery(ps, q, default_db, &schema_cache, pp);
    if (out_queryz.size() == 1)
    {
      for (auto it : out_queryz)
      {
        return it; // 返回第一句
      }
    }
    else if (out_queryz.size() >= 2)
    {
      int i = 1;
      for (auto it : out_queryz)
      {
        if (i == 2)
          return it; // 返回第二句
        i++;
      }
    }
    return "";
  }
  catch (const SynchronizationException &e)
  {
    std::cout << e << std::endl;
    return "";
  }
  catch (const AbstractException &e)
  {
    std::cout << e << std::endl;
    return "";
  }
  catch (const CryptDBError &e)
  {
    std::cout << "Low level error: " << e.msg << std::endl;
    return "";
  }
  catch (const std::runtime_error &e)
  {
    std::cout << "Unexpected Error: " << e.what() << std::endl;
    return "";
  }
}

static struct Result *handle_result(ProxyState &ps, const std::string &q, struct Result *result, bool pp = true)
{
  static SchemaCache schema_cache;
  try
  {
    const std::string &default_db =
        getDefaultDatabaseForConnection(ps.getConn());
    return DecryptResult(ps, q, default_db, result, &schema_cache, pp);
  }
  catch (const SynchronizationException &e)
  {
    std::cout << e << std::endl;
    struct Result *result = NULL;
    return result;
  }
  catch (const AbstractException &e)
  {
    std::cout << e << std::endl;
    struct Result *result = NULL;
    return result;
  }
  catch (const CryptDBError &e)
  {
    std::cout << "Low level error: " << e.msg << std::endl;
    struct Result *result = NULL;
    return result;
  }
  catch (const std::runtime_error &e)
  {
    std::cout << "Unexpected Error: " << e.what() << std::endl;
    struct Result *result = NULL;
    return result;
  }
}

static bool mainConnect(ProxyState &ps, std::string dbname)
{
  std::cout << "in mainConnect\n"
            << std::endl;
  const std::string create_db =
      "CREATE DATABASE IF NOT EXISTS " + std::string(dbname);
  handle_sql(ps, create_db, false);
  std::cout << "ProxyState 创建ok 开始建立连接!!!\n"
            << std::endl;
  const std::string use_db = "USE " + std::string(dbname);
  handle_sql(ps, use_db, false);
  handle_sql(ps, use_db); // 选择数据库
  return true;
}

static std::list<std::string> mainRewriteSQL(ProxyState &ps, std::string old_quety)
{
  std::string out_query = handle_sql(ps, old_quety);
  std::list<std::string> return_queryz;
  return_queryz.push_back(out_query);
  if (strncmp(toLowerCase(out_query).c_str(), " call", 5) == 0)
    return_queryz.push_back(handle_sql(ps, old_quety));
  return return_queryz;
}

static struct SQL *RewriteSQLs(std::string old_query, ProxyState &ps)
{
  std::cout << "\n连接结束 !!!\n"
            << std::endl;
  std::cout << "\n重写语句 !!!\n"
            << std::endl;
  std::cout << "\n=============\nOLD QUERY: " << old_query << std::endl;
  std::list<std::string> out_queryz = mainRewriteSQL(ps, old_query);

  int size = 0;   // SQL结构大小
  int number = 0; // 重写语句个数
  char *query[2]; // 存储重写语句
  for (auto it : out_queryz)
  {
    char *tmquery = (char *)it.data();
    query[number] = tmquery;
    number++;
    size += (strlen(tmquery) + 1);
    std::cout << "\n=============\nNEW QUERY: " << tmquery << std::endl;
  }
  size += (sizeof(struct SQL)); // SQL结构总大小
  // 初始化结构结果集
  struct SQL *sql = (struct SQL *)malloc(size);
  memset(sql, 0, size); // 清零
  sql->sql_size = size; // 大小
  sql->number = number; // 个数
  int pointer_sql = 0;
  for (int i = 0; i < number; i++)
  { // 语句
    memcpy((char *)(sql->sql + pointer_sql), query[i], strlen(query[i]) + 1);
    pointer_sql += (strlen(query[i]) + 1);
  }
  return sql;
}

static struct Result *mainDecryptResult(ProxyState &ps, std::string old_quety, struct Result *result)
{
  return handle_result(ps, old_quety, result);
}

static struct Result *DecryptResults(struct Result *result, std::string old_query, ProxyState &ps)
{
  //    ConnectionInfo ci("localhost", "root", "letmein");
  //    const std::string master_key = "2392834";
  //    std::string emdbpath = "./shadow"; // 嵌入数据库路径
  //    ProxyState ps(ci, emdbpath, master_key); // ps=连接的状态信息，emdbpath=嵌入数据库路径
  //    std::string dbname = "test";
  //	mainConnect(ps, dbname);
  std::cout << "\n连接结束 !!!\n"
            << std::endl;

  return mainDecryptResult(ps, old_query, result);
}
