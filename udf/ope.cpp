#include "Node.h"
#include "mysql.h" // Specify include path when compiling. I.e. g++ -c -o -I /usr/include/mysql. Ensure that you have installed MySQL.
#include <string>

std::map<string, double> update;
Node *root = nullptr;
double start_update = -1;
double end_update = -1;
extern "C"
{
    //typedef double longlong;
    //插入
    my_bool FHInsert_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
    double FHInsert(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
    //搜索
    my_bool FHSearch_init(UDF_INIT *const initid, UDF_ARGS *const args, char *const message);
    double FHSearch(UDF_INIT *const initid, UDF_ARGS *const args,
                    char *const result, unsigned long *const length,
                    char *const is_null, char *const error);

    //更新
    my_bool FHUpdate_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
    double FHUpdate(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
    //更新范围
    my_bool FHStart_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
    double FHStart(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
    my_bool FHEnd_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
    double FHEnd(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
}

static char *
getba(UDF_ARGS *const args, int i, double &len)
{
    len = args->lengths[i];
    return args->args[i];
}

/*插入*/
double FHInsert(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error)
{
    int pos = *(int *)(args->args[0]);

    double keyLen;
    char *const keyBytes = getba(args, 1, keyLen);
    const std::string cipher = std::string(keyBytes, keyLen);
    double start_update = -1;
    double end_update = -1;
    update.clear();
    return root->insert(pos, cipher);
    //return pos;
}

my_bool FHInsert_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    start_update = -1;
    end_update = -1;
    update.clear();
    if (root == nullptr)
    {
        root_initial();
    }
    return 0;
}

/*搜索*/
double
FHSearch(UDF_INIT *const initid, UDF_ARGS *const args,
         char *const result, unsigned long *const length,
         char *const is_null, char *const error)
{
    int pos = *(int *)(args->args[0]);
    return root->search(pos);
}
/*
my_bool FHSearch_init(UDF_INIT* initid, UDF_ARGS* args, char* message)
{
    return 0;
}
*/
my_bool
FHSearch_init(UDF_INIT *const initid, UDF_ARGS *const args,
              char *const message)
{

    return 0;
}

double FHUpdate(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error)
{
    double keyLen;
    char *const keyBytes = getba(args, 0, keyLen);
    const std::string cipher = std::string(keyBytes, keyLen);
    return get_update(cipher);
    // return -2;
}

my_bool FHUpdate_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    return 0;
}

double FHStart(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error)
{
    return lower_bound();
}

my_bool FHStart_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    return 0;
}

double FHEnd(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error)
{
    return upper_bound();
}

my_bool FHEnd_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    return 0;
}
