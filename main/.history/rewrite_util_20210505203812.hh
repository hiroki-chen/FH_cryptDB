#pragma once

#include <string>

#include <main/rewrite_main.hh>
#include <main/Analysis.hh>
#include <main/rewrite_ds.hh>

#include <sql_list.h>
#include <sql_table.h>
#include <dirent.h>

const std::string BOLD_BEGIN = "\033[1m";
const std::string RED_BEGIN = "\033[1;31m";
const std::string GREEN_BEGIN = "\033[1;92m";
const std::string COLOR_END = "\033[0m";
const std::string ENC_IDENTIFIER = "enc_";
const std::string FH_IDENTIFIER = "fh_";

Item *
rewrite(const Item &i, const EncSet &req_enc, Analysis &a);

TABLE_LIST *
rewrite_table_list(const TABLE_LIST *const t, const Analysis &a);

TABLE_LIST *
rewrite_table_list(const TABLE_LIST *const t,
                   const std::string &anon_name);

SQL_I_List<TABLE_LIST>
rewrite_table_list(const SQL_I_List<TABLE_LIST> &tlist, Analysis &a,
                   bool if_exists = false);

List<TABLE_LIST>
rewrite_table_list(List<TABLE_LIST> tll, Analysis &a);

RewritePlan *
gather(const Item &i, Analysis &a);

static std::string
deductPlainTableName(const std::string &field_name,
                     Name_resolution_context *const context,
                     Analysis &a)
{
    assert(context);

    const TABLE_LIST *current_table =
        context->first_name_resolution_table;
    do
    {
        TEST_DatabaseDiscrepancy(current_table->db, a.getDatabaseName());
        const TableMeta &tm =
            a.getTableMeta(current_table->db,
                           current_table->table_name);
        if (tm.childExists(IdentityMetaKey(field_name)))
        {
            return std::string(current_table->table_name);
        }

        current_table = current_table->next_local;
    } while (current_table != context->last_name_resolution_table);

    return deductPlainTableName(field_name, context->outer_context, a);
}

void gatherAndAddAnalysisRewritePlan(const Item &i, Analysis &a, const Item *original = nullptr);

void optimize(Item **const i, Analysis &a);

LEX *begin_transaction_lex(const std::string &dbname);

LEX *commit_transaction_lex(const std::string &dbname);

std::vector<Create_field *>
rewrite_create_field(const FieldMeta *const fm, Create_field *const f,
                     Analysis &a, bool needEnc, std::vector<rapidjson::Document> &doc);

std::vector<Key *>
rewrite_key(const TableMeta &tm, Key *const key, const Analysis &a);

bool getSaltContentForJSONDocument(rapidjson::Document &doc);

bool loadLocalTable(const std::string &path, rapidjson::Document &doc);

bool dropJson(const std::string &path);

rapidjson::Document
buildEmptyJSONForNewFHField(const std::string &dbname,
                            const std::string &table_name,
                            const std::string &field_name);

std::string
bool_to_string(bool b);

bool string_to_bool(const std::string &s);

bool isDETFunc(const Item_func &item);

bool isInequalityFunc(const Item_func &item);

List<Create_field>
createAndRewriteField(Analysis &a, const ProxyState &ps,
                      Create_field *const cf,
                      TableMeta *const tm, bool new_table,
                      List<Create_field> &rewritten_cfield_list,
                      std::string plain_table_name = "");

bool needFrequencySmoothing(const std::string &field_name);

bool needEncryption(const std::string &field_name);

std::pair<unsigned int, unsigned int>
getIntervalForItem(const unsigned int &interval_num,
                   const std::pair<unsigned int, unsigned int> &range,
                   const double &value);

std::string
getSalt(const Item &item, const double &alpha,
        const double &p, const double &interval_num,
        const std::pair<unsigned int, unsigned int> &range,
        Analysis &a);

int
chooseSalt(std::vector<std::unique_ptr<Salt>> &salts, const double &alpha,
               const unsigned int &total_salt_used,
               const unsigned int &ptext_size,
               const std::pair<unsigned int, unsigned int> &interval,
               Analysis &a, rapidjson::Document &doc);

std::vector<std::string>
getFiles(const std::string &dir);

bool
loadAllSaltsFromFile(const std::string &db_name, const TABLE_LIST *const table_list, Analysis &a);

/**
 * HACK: ONLY INVOKED BY SaltTable.
 */
bool
writeAllTablesToJsonDOM(rapidjson::Document &doc,
                             const std::pair<unsigned int, unsigned int> &interval,
                             const std::vector<std::unique_ptr<Salt>> &salt_table,
                             const std::map<double, unsigned int> &local_table);

bool
writeSaltTableToJsonDOM(rapidjson::Document &doc,
                             const std::pair<unsigned int, unsigned int> &interval,
                             const std::vector<std::unique_ptr<Salt>> &salt_table);

bool
writeLocalTableToJsonDOM(rapidjson::Document &doc,
                              const std::map<double, unsigned int> &local_table);

std::string
getRandomString(const unsigned int &length);

std::string
getSalt(std::vector<double> &params, const Item &item,
        const std::string &db_name, const std::string &table_name,
        const std::string &field_name,
        Analysis &a, rapidjson::Document &doc);

uint64_t
extractSaltLengthFromField(const std::string &field_name);

bool writeDomToFile(const rapidjson::Document &doc, const std::string &path);

/**
 * This function can also load the document but you can load salt as you wish.
 */
bool getDocumentFromFileAndLoadSalt(const std::string &path, Analysis &a, rapidjson::Document &doc, bool load = true);

bool tossACoin(const double &p);

unsigned int
findPos(const std::map<double, unsigned int> &local_table, const Item &item, const Item_func::Functype &type);

Item *
rewrite_args_fh_ope(const Item_func *const item, Analysis &a);

Item *
getFHSerachUDF(const unsigned int &pos);

Item *
getNewFunc(Item *const lhs, Item *const rhs, const Item_func::Functype &type);

/**
 * When rows are deleted or updated, we must do synchronization.
 */
bool updateSaltTable(const ResType &dbres, Analysis &a, bool update = false);

bool issueSelectForDeleteOrUpdate(Analysis &a, const LEX *const lex, const ProxyState &ps, bool update = false);

/**
 * For ope.
 */
unsigned int
getPos(const Item &i, Analysis &a);

salt_type
padIV(const std::string &field_name, const salt_type &IV);

UpdateOPE *
generateUpdateOPE(const double &plaintext,
                  const std::string &ciphertext,
                  const OnionMeta &om,
                  const std::string &table_name,
                  std::map<double, unsigned int> &local_table);

Item *
encrypt_item_layers(const Item &i, onion o, const OnionMeta &om,
                    Analysis &a, rapidjson::Document &doc, uint64_t IV = 0,
                    const std::string &field_name = "");

std::string
rewriteAndGetSingleQuery(ProxyState &ps, const std::string &q,
                         SchemaInfo const &schema,
                         const std::string &default_db);

// FIXME(burrows): Generalize to support any container with next AND end
// semantics.
template <typename T>
std::string vector_join(std::vector<T> v, const std::string &delim,
                        const std::function<std::string(T)> &finalize)
{
    std::string accum;
    for (typename std::vector<T>::iterator it = v.begin();
         it != v.end(); ++it)
    {
        const std::string &element = finalize(static_cast<T>(*it));
        accum.append(element);
        accum.append(delim);
    }

    AssignOnce<std::string> output;
    if (accum.length() > 0)
    {
        output = accum.substr(0, accum.length() - delim.length());
    }
    else
    {
        output = accum;
    }

    return output.get();
}

std::string
escapeString(const std::unique_ptr<Connect> &c,
             const std::string &escape_me);

void encrypt_item_all_onions(const Item &i, const FieldMeta &fm,
                             uint64_t IV, Analysis &a, std::vector<Item *> *l,
                             rapidjson::Document &doc);

void escapeCipherOPE(const ProxyState &ps, UpdateOPE &ope);

std::vector<onion>
getOnionIndexTypes();

void typical_rewrite_insert_type(const Item &i, const FieldMeta &fm,
                                 Analysis &a, std::vector<Item *> *l);

void process_select_lex(const st_select_lex &select_lex, Analysis &a);

void process_table_list(const List<TABLE_LIST> &tll, Analysis &a);

st_select_lex *
rewrite_select_lex(const st_select_lex &select_lex, Analysis &a);

std::string
mysql_noop();

std::string
getDefaultDatabaseForConnection(const std::unique_ptr<Connect> &c);

bool retrieveDefaultDatabase(unsigned long long thread_id,
                             const std::unique_ptr<Connect> &c,
                             std::string *const out_name);

void queryPreamble(ProxyState &ps, const std::string &q,
                   std::unique_ptr<QueryRewrite> *qr,
                   std::list<std::string> *const out_queryz,
                   SchemaCache *const schema,
                   const std::string &default_db);

bool queryHandleRollback(const ProxyState &ps, const std::string &query,
                         SchemaInfo const &schema);

void prettyPrintQuery(const std::string &query);

class EpilogueResult
{
public:
    EpilogueResult(QueryAction action, const ResType &res_type)
        : action(action), res_type(res_type) {}

    const QueryAction action;
    const ResType res_type;
};

EpilogueResult
queryEpilogue(ProxyState &ps, const QueryRewrite &qr,
              const ResType &res, const std::string &query,
              const std::string &default_db, bool pp);

ResType
resultEpilogue(const ProxyState &ps, const QueryRewrite &qr,
               const ResType &res, const std::string &query,
               const std::string &default_db, bool pp);

class SchemaCache
{
public:
    SchemaCache() : no_loads(true), id(randomValue() % UINT_MAX) {}

    const SchemaInfo &getSchema(const std::unique_ptr<Connect> &conn,
                                const std::unique_ptr<Connect> &e_conn);
    void updateStaleness(const std::unique_ptr<Connect> &e_conn,
                         bool staleness);
    bool initialStaleness(const std::unique_ptr<Connect> &e_conn);
    bool cleanupStaleness(const std::unique_ptr<Connect> &e_conn);
    void lowLevelCurrentStale(const std::unique_ptr<Connect> &e_conn);
    void lowLevelCurrentUnstale(const std::unique_ptr<Connect> &e_conn);

private:
    std::unique_ptr<const SchemaInfo> schema;
    bool no_loads;
    const unsigned int id;
};

template <typename InType, typename InterimType, typename OutType>
std::function<OutType(InType in)>
fnCompose(std::function<OutType(InterimType)> outer,
          std::function<InterimType(InType)> inner)
{

    return [&outer, &inner](InType in) {
        return outer(inner(in));
    };
}
