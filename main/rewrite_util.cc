#include <memory>
#include <random>

#include <main/rewrite_util.hh>
#include <main/rewrite_main.hh>
#include <main/macro_util.hh>
#include <main/metadata_tables.hh>
#include <parser/lex_util.hh>
#include <parser/stringify.hh>
#include <util/enum_text.hh>

#include "parser/rapidjson/document.h"
#include "parser/rapidjson/filereadstream.h"
#include "parser/rapidjson/filewritestream.h"
#include "parser/rapidjson/writer.h"

extern CItemTypesDir itemTypes;

void
optimize(Item ** const i, Analysis &a) {
   //TODO
/*Item *i0 = itemTypes.do_optimize(*i, a);
    if (i0 != *i) {
        // item i was optimized (replaced) by i0
        if (a.itemRewritePlans.find(*i) != a.itemRewritePlans.end()) {
            a.itemRewritePlans[i0] = a.itemRewritePlans[*i];
            a.itemRewritePlans.erase(*i);
        }
        *i = i0;
    } */
}


// this function should be called at the root of a tree of items
// that should be rewritten
Item *
rewrite(const Item &i /*This is a lex's substructure*/, const EncSet &req_enc, Analysis &a)
{
    const std::unique_ptr<RewritePlan> &rp =
        constGetAssert(a.rewritePlans, &i);
    const EncSet solution = rp->es_out.intersect(req_enc);
    // FIXME: Use version that takes reason, expects 0 children,
    // and lets us indicate what our EncSet does have.
    TEST_NoAvailableEncSet(solution, i.type(), req_enc, rp->r.why,
                           std::vector<std::shared_ptr<RewritePlan> >());

    return itemTypes.do_rewrite(i, solution.chooseOne(), *rp.get(), a); //Why here.
}

TABLE_LIST *
rewrite_table_list(const TABLE_LIST * const t, const Analysis &a)
{
    // Table name can only be empty when grouping a nested join.
    assert(t->table_name || t->nested_join);
    if (t->table_name) {
        const std::string plain_name =
            std::string(t->table_name, t->table_name_length);
        // Don't use Analysis::getAnonTableName(...) as it respects
        // aliases and if a table has been aliased with 'plain_name'
        // it will give us the wrong name.
        TEST_DatabaseDiscrepancy(t->db, a.getDatabaseName());
        const std::string anon_name =
            a.translateNonAliasPlainToAnonTableName(t->db, plain_name);
        return rewrite_table_list(t, anon_name);
    } else {
        return copyWithTHD(t);
    }
}

TABLE_LIST *
rewrite_table_list(const TABLE_LIST * const t,
                   const std::string &anon_name)
{
    TABLE_LIST *const new_t = copyWithTHD(t);
    new_t->table_name = make_thd_string(anon_name);
    new_t->table_name_length = anon_name.size();
    if (false == new_t->is_alias) {
        new_t->alias = make_thd_string(anon_name);
    }
    new_t->next_local = NULL;

    return new_t;
}

// @if_exists: defaults to false; it is necessary to facilitate
//   'DROP TABLE IF EXISTS'
SQL_I_List<TABLE_LIST>
rewrite_table_list(const SQL_I_List<TABLE_LIST> &tlist, Analysis &a,
                   bool if_exists)
{
    if (!tlist.elements) {
        return SQL_I_List<TABLE_LIST>();
    }

    TABLE_LIST * tl;
    TEST_DatabaseDiscrepancy(tlist.first->db, a.getDatabaseName());
    if (if_exists && !a.nonAliasTableMetaExists(tlist.first->db,
                                                tlist.first->table_name)) {
       tl = copyWithTHD(tlist.first);
    } else {
       tl = rewrite_table_list(tlist.first, a);
    }

    const SQL_I_List<TABLE_LIST> * const new_tlist =
        oneElemListWithTHD<TABLE_LIST>(tl);

    TABLE_LIST * prev = tl;
    for (TABLE_LIST *tbl = tlist.first->next_local; tbl;
         tbl = tbl->next_local) {
        TABLE_LIST * new_tbl;
        TEST_DatabaseDiscrepancy(tbl->db, a.getDatabaseName());
        if (if_exists && !a.nonAliasTableMetaExists(tbl->db,
                                                    tbl->table_name)) {
            new_tbl = copyWithTHD(tbl);
        } else {
            new_tbl = rewrite_table_list(tbl, a);
        }

        prev->next_local = new_tbl;
        prev = new_tbl;
    }
    prev->next_local = NULL;

    return *new_tlist;
}

List<TABLE_LIST>
rewrite_table_list(List<TABLE_LIST> tll, Analysis &a)
{
    List<TABLE_LIST> * const new_tll = new List<TABLE_LIST>();

    List_iterator<TABLE_LIST> join_it(tll);

    for (;;) {
        const TABLE_LIST * const t = join_it++;
        if (!t) {
            break;
        }

        TABLE_LIST * const new_t = rewrite_table_list(t, a);
        new_tll->push_back(new_t);

        if (t->nested_join) {
            new_t->nested_join->join_list =
                rewrite_table_list(t->nested_join->join_list, a);
            return *new_tll;
        }

        if (t->on_expr) {
            new_t->on_expr = rewrite(*t->on_expr, PLAIN_EncSet, a);
        }

	/* TODO: derived tables
        if (t->derived) {
            st_select_lex_unit *u = t->derived;
            rewrite_select_lex(u->first_select(), a);
        }
	*/
    }

    return *new_tll;
}

RewritePlan *
gather(const Item &i, Analysis &a)
{
    return itemTypes.do_gather(i, a);
}

void
gatherAndAddAnalysisRewritePlan(const Item &i, Analysis &a, const Item *original)
{
	/*
	 * Where filter goes.
	 */
    LOG(cdb_v) << "calling gather for item " << i << std::endl;
    std::cout << "calling gather for item " << i << std::endl;
    std::cout << "item type: " << i.type() << std::endl;
    /*if (nullptr == original) {
    	original = &i;
    }
    */
    a.rewritePlans[&i] = std::unique_ptr<RewritePlan>(gather(i, a));
}

LEX *
begin_transaction_lex(const std::string &dbname)
{
    static const std::string query = "START TRANSACTION;";
    query_parse *const begin_parse = new query_parse(dbname, query);
    return begin_parse->lex();
}

LEX *
commit_transaction_lex(const std::string &dbname)
{
    static const std::string query = "COMMIT;";
    query_parse *const commit_parse = new query_parse(dbname, query);
    return commit_parse->lex();
}

//TODO(raluca) : figure out how to create Create_field from scratch
// and avoid this chaining and passing f as an argument
static Create_field *
get_create_field(const Analysis &a, Create_field * const f,
                 const OnionMeta &om)
{
    const std::string name = om.getAnonOnionName();
    Create_field *new_cf = f;

    // Default value is handled during INSERTion.
    auto save_default = f->def;
    f->def = NULL;

    const auto &enc_layers = a.getEncLayers(om);
    assert(enc_layers.size() > 0);
    for (auto it = enc_layers.begin(); it != enc_layers.end(); it++) {
        const Create_field * const old_cf = new_cf;
        new_cf = (*it)->newCreateField(old_cf, name);
    }

    // Restore the default so we don't memleak it.
    f->def = save_default;
    return new_cf;
}

// NOTE: The fields created here should have NULL default pointers
// as such is handled during INSERTion.
std::vector<Create_field *>
rewrite_create_field(const FieldMeta * const fm,
                     Create_field * const f, const Analysis &a, bool needEnc)
{
    LOG(cdb_v) << "in rewrite create field for " << *f;

    assert(fm->children.size() > 0);


    std::vector<Create_field *> output_cfields;
    if (!needEnc) {
    	output_cfields.push_back(f);
    } else {
    	// Don't add the default value to the schema.
    	    Item *const save_def = f->def;
    	    f->def = NULL;

    	    // create each onion column
    	    for (auto oit : fm->orderedOnionMetas()) {
    	        OnionMeta * const om = oit.second;
    	        Create_field * const new_cf = get_create_field(a, f, *om);

    	        output_cfields.push_back(new_cf);
    	    }

    	    // create salt column
    	    if (fm->getHasSalt()) {
    	        THD * const thd         = current_thd;
    	        Create_field * const f0 = f->clone(thd->mem_root);
    	        f0->field_name          = thd->strdup(fm->getSaltName().c_str());
    	        // Salt is unsigned and is not AUTO_INCREMENT.
    	        // > NOT_NULL_FLAG is useful for debugging if mysql strict mode
    	        //   (ie, STRICT_ALL_TABLES) is turned on.
    	        f0->flags               =
    	            (f0->flags | UNSIGNED_FLAG | NOT_NULL_FLAG)
    	            & ~AUTO_INCREMENT_FLAG;
    	        f0->sql_type            = MYSQL_TYPE_LONGLONG;
    	        f0->length              = 8;

    	        output_cfields.push_back(f0);
    	    }

    	    // Restore the default to the original Create_field parameter.
    	    f->def = save_def;
    }

    return output_cfields;
}

std::vector<onion>
getOnionIndexTypes()
{
    return std::vector<onion>({oOPE, oDET, oPLAIN});
}

static std::string
getOriginalKeyName(Key *const key)
{
    if (Key::PRIMARY == key->type) {
        return "PRIMARY";
    }

    const std::string out_name = convert_lex_str(key->name);
    TEST_TextMessageError(out_name.size() > 0,
                          "Non-Primary keys can not have blank name!");

    return out_name;
}

std::vector<Key *>
rewrite_key(const TableMeta &tm, Key *const key, const Analysis &a)
{
    std::vector<Key *> output_keys;

    const std::vector<onion> key_onions = getOnionIndexTypes();
    for (auto onion_it : key_onions) {
        const onion o = onion_it;
        Key *const new_key = key->clone(current_thd->mem_root);

        // Set anonymous name.
        const std::string new_name =
            a.getAnonIndexName(tm, getOriginalKeyName(key), o);
        new_key->name = string_to_lex_str(new_name);
        new_key->columns.empty();

        // Set anonymous columns.
        auto col_it =
            RiboldMYSQL::constList_iterator<Key_part_spec>(key->columns);
        for (;;) {
            const Key_part_spec *const key_part = col_it++;
            if (NULL == key_part) {
                output_keys.push_back(new_key);
                break;
            }

            Key_part_spec *const new_key_part = copyWithTHD(key_part);
            const std::string field_name =
                convert_lex_str(new_key_part->field_name);
            // > the onion may not exist; ie oPLAIN with SENSITIVE and not
            // an AUTO INCREMENT column
            const FieldMeta &fm = a.getFieldMeta(tm, field_name);
            const OnionMeta *const om = fm.getOnionMeta(o);
            if (NULL == om) {
                break;
            }

            new_key_part->field_name =
                string_to_lex_str(om->getAnonOnionName());
            new_key->columns.push_back(new_key_part);
        }
    }

    // Only create one PRIMARY KEY.
    if (Key::PRIMARY == key->type) {
        if (output_keys.size() > 0) {
            return std::vector<Key *>({output_keys.front()});
        }
    }

    return output_keys;
}

std::string
bool_to_string(bool b)
{
    if (true == b) {
        return "TRUE";
    } else {
        return "FALSE";
    }
}

bool
string_to_bool(const std::string &s)
{
    if (s == std::string("TRUE") || s == std::string("1")) {
        return true;
    } else if (s == std::string("FALSE") || s == std::string("0")) {
        return false;
    } else {
        throw "unrecognized string in string_to_bool!";
    }
}

List<Create_field>
createAndRewriteField(Analysis &a, const ProxyState &ps,
                      Create_field * const cf,
                      TableMeta *const tm, bool new_table,
                      List<Create_field> &rewritten_cfield_list,
					  std::string plain_table_name)
{
	/*
	 * judge first.
	 */

    const std::string name = std::string(cf->field_name);
    auto buildFieldMeta =
        [] (const std::string name, Create_field * const cf,
            const ProxyState &ps, TableMeta *const tm)
    {
        std::cout <<"Createing new fieldmeta now!"<<std::endl;
        return new FieldMeta(name, cf, ps.getMasterKey().get(),
                             ps.defaultSecurityRating(),
                             tm->leaseIncUniq());
    };
    std::unique_ptr<FieldMeta> fm(buildFieldMeta(name, cf, ps, tm));

    /**
     * TODO: create a direcotry for json table first.
     */
    if (0 == name.substr(0, 3).compare(FH_IDENTIFIER)) {
    	std::string dir = "";
        dir.append(a.getDatabaseName() + '/');
        dir.append(plain_table_name + '/');
        dir.append(name);
        std::string command = "mkdir -p " + dir;
        system(command.c_str());
     }

	std::string e_i = name.substr(0, 4);
	std::string fh_i = name.substr(0, 3);
	bool needEnc =
			!(e_i.compare(ENC_IDENTIFIER)) ||
			!(fh_i.compare(FH_IDENTIFIER));

    // -----------------------------
    //         Rewrite FIELD       
    // -----------------------------
    const auto new_fields = rewrite_create_field(fm.get(), cf, a, needEnc);
    rewritten_cfield_list.concat(vectorToListWithTHD(new_fields));

    // -----------------------------
    //         Update FIELD       
    // -----------------------------

    // Here we store the key name for the first time. It will be applied
    // after the Delta is read out of the database.
    if (true == new_table) {
        tm->addChild(IdentityMetaKey(name), std::move(fm));
    } else {
        a.deltas.push_back(std::unique_ptr<Delta>(
                                new CreateDelta(std::move(fm), *tm,
                                                IdentityMetaKey(name))));
        a.deltas.push_back(std::unique_ptr<Delta>(
               new ReplaceDelta(*tm,
                                a.getDatabaseMeta(a.getDatabaseName()))));
    }

    return rewritten_cfield_list;
}

//TODO: which encrypt/decrypt should handle null?

// We cannot modify this encrypt function bacause it is no use to append a single item with multiple predicates.
// This function only encrypts one single item in the predicate.
// For example, it encrypts the integer 123 from `FH_id = 123`.
Item *
encrypt_item_layers(const Item &i, onion o, const OnionMeta &om,
                    const Analysis &a, uint64_t IV) {
    assert(!RiboldMYSQL::is_null(i));

    const auto &enc_layers = a.getEncLayers(om);
    assert_s(enc_layers.size() > 0, "onion must have at least one layer");
    // We can also modify the item itself by using a pointer which points to its address.
    const Item *enc = &i;
    Item *new_enc = NULL;

    for (auto it = enc_layers.begin(); it != enc_layers.end(); it++) {
        LOG(encl) << "encrypt layer "
                  << TypeText<SECLEVEL>::toText((*it)->level()) << "\n";
        if (SECLEVEL::FHDET == (*it)->level() || SECLEVEL::FHOPE == (*it)->level()) {
        	// do something.
        	std::cout << "encrypting for FH item whose name is " << i.name << std::endl;
        	new_enc = (*it)->encrypt(*enc, IV);
        } else {
        	new_enc = (*it)->encrypt(*enc, IV);
        }
        assert(new_enc);
        enc = new_enc;
    }

    // @i is const, do we don't want the caller to modify it accidentally.
    assert(new_enc && new_enc != &i);
    return new_enc;
}

std::string
rewriteAndGetSingleQuery(const ProxyState &ps, const std::string &q,
                         SchemaInfo const &schema,
                         const std::string &default_db)
{
    const QueryRewrite qr(Rewriter::rewrite(ps, q, schema, default_db));
    assert(false == qr.output->stalesSchema());
    assert(QueryAction::VANILLA == qr.output->queryAction(ps.getConn()));

    std::list<std::string> out_queryz;
    qr.output->getQuery(&out_queryz, schema);
    assert(out_queryz.size() == 1);

    return out_queryz.back();
}

std::string
escapeString(const std::unique_ptr<Connect> &c,
             const std::string &escape_me)
{
    const unsigned int escaped_length = escape_me.size() * 2 + 1;
    std::unique_ptr<char, void (*)(void *)>
        escaped(new char[escaped_length], &operator delete []);
    c->real_escape_string(escaped.get(), escape_me.c_str(),
                          escape_me.size());

    return std::string(escaped.get());
}

void
encrypt_item_all_onions(const Item &i, const FieldMeta &fm,
                        uint64_t IV, Analysis &a, std::vector<Item*> *l)
{
    for (auto it : fm.orderedOnionMetas()) {
        const onion o = it.first->getValue();
        OnionMeta * const om = it.second;
        l->push_back(encrypt_item_layers(i, o, *om, a, IV));
    }
}

/**
 * TODO: Update salt table by tossing a coin.
 */
void
typical_rewrite_insert_type(const Item &i, const FieldMeta &fm,
                               Analysis &a, std::vector<Item *> *l)
{
	// Test if salts can be added in this function.
    const uint64_t salt = fm.getHasSalt() ? randomValue() : 0;
    //const uint64_t salt = fm.fname.substr(0, 3).compare(FH_IDENTIFIER) == 0 ? randomValue() : 0;
    // Obviously salts are added to the RND level..

    // Determine i's type;
    // Get a salt (or generate one) if fm.fname contains "fh_";
    // Update a's salt table by calling its interface.

	// Fetch its salt.

	const std::string db_name = a.getDatabaseName();
	const std::string field_name = fm.fname;
	const std::string& table_name = a.table_name_last_used;

	if (0 == fm.fname.substr(0, 3).compare(FH_IDENTIFIER)) {

		// Create a rapidjson Document object.
		rapidjson::Document doc;

		// Read from file.
		std::string dir = "CryptDB_DATA/";
		dir.append(db_name + '/');
		dir.append(table_name + "/");
		dir.append(field_name + ".json");

		FILE *fp = fopen(dir.c_str(), "r");
		char read_buffer[65536];
		rapidjson::FileReadStream frs(fp, read_buffer, sizeof(read_buffer));

		// Parse the file into DOM.
		doc.ParseStream(frs);

		a.loadSaltsFromJsonDOM(doc, "0");

		std::vector<double> &params = a.variables[VariableLocator(db_name, table_name, field_name)];
		// TODO: wrap it into a function.

		salt_type IV = stoull(getSalt(params, i, db_name, table_name, field_name, a, doc));

		// TODO: write the document back!
		encrypt_item_all_onions(i, fm, IV, a, l);
		char writeBuffer[65536];

		FILE *fo = fopen(dir.c_str(), "w");
		rapidjson::FileWriteStream fws(fo, writeBuffer, sizeof(writeBuffer));

		rapidjson::Writer<rapidjson::FileWriteStream> writer(fws);
		doc.Accept(writer);

		fclose(fp);
		fclose(fo);
	} else {
		encrypt_item_all_onions(i, fm, salt, a, l);
	}



    if (0 != salt) {
        l->push_back(new Item_int(static_cast<ulonglong>(salt)));
    }
}

std::string
mysql_noop()
{
    return "do 0;";
}

/*
 * connection ids can be longer than 32 bits
 * http://dev.mysql.com/doc/refman/5.1/en/mysql-thread-id.html
 */
static bool
getConnectionID(const std::unique_ptr<Connect> &c,
                unsigned long long *const connection_id)
{
    std::unique_ptr<DBResult> dbres;
    const std::string &query = " SELECT CONNECTION_ID();";
    RETURN_FALSE_IF_FALSE(c->execute(query, &dbres));
    RETURN_FALSE_IF_FALSE(mysql_num_rows(dbres->n) == 1);

    const MYSQL_ROW row = mysql_fetch_row(dbres->n);
    const unsigned long *const l = mysql_fetch_lengths(dbres->n);
    *connection_id =
        strtoull(std::string(row[0], l[0]).c_str(), NULL, 10);
    return true;
}


std::string
getDefaultDatabaseForConnection(const std::unique_ptr<Connect> &c)
{
    unsigned long long thread_id;
    TEST_TextMessageError(getConnectionID(c, &thread_id),
                          "failed to get connection id!");
    std::string out_name;
    TEST_TextMessageError(retrieveDefaultDatabase(thread_id, c, &out_name),
                          "failed to retrieve default database!");

    return out_name;
}

uint64_t
extractSaltLengthFromField(const std::string &field_name)
{
	auto pos = field_name.find_last_of('_', std::string::npos);

	assert(std::string::npos != pos); // The field name is incorrect.
	return stoull(field_name.substr(pos + 1, field_name.size()));
}

bool
retrieveDefaultDatabase(unsigned long long thread_id,
                        const std::unique_ptr<Connect> &c,
                        std::string *const out_name)
{
    out_name->clear();

    std::unique_ptr<DBResult> dbres;
    const std::string &query =
        " SELECT db FROM INFORMATION_SCHEMA.PROCESSLIST"
        "  WHERE id = " + std::to_string(thread_id) + ";";
    RETURN_FALSE_IF_FALSE(c->execute(query, &dbres));
    RETURN_FALSE_IF_FALSE(mysql_num_rows(dbres->n) == 1);

    const MYSQL_ROW row = mysql_fetch_row(dbres->n);
    const unsigned long *const l = mysql_fetch_lengths(dbres->n);
    *out_name = std::string(row[0], l[0]);
    return true;
}

// 获得重写后的SQL语句集
void
queryPreamble(const ProxyState &ps, const std::string &q,
              std::unique_ptr<QueryRewrite> *const qr,
              std::list<std::string> *const out_queryz,
              SchemaCache *const schema_cache,
              const std::string &default_db)
{
    const SchemaInfo &schema =
        schema_cache->getSchema(ps.getConn(), ps.getEConn());

    // Rewrite类存储重写后的SQL语句或解密后的结果
    // 通过调用Rewriter::rewrite函数对SQL语句重写，再创建一个QueryRewrite对象
    // 实际重写的操作由rewrite函数完成
    // 这里的q就是待重写的语句
    // 执行完成后，qr->output即是重写后的SQL语句
    *qr = std::unique_ptr<QueryRewrite>(
            new QueryRewrite(Rewriter::rewrite(ps, q, schema,
                                               default_db)));

    // We handle before any queries because a failed query
    // may stale the database during recovery and then
    // we'd have to handle there as well.
    schema_cache->updateStaleness(ps.getEConn(),
                                  (*qr)->output->stalesSchema());

    // ASK bites again...
    // We want the embedded database to reflect the metadata for the
    // current remote connection.
    if ((*qr)->output->usesEmbeddedDB()) {
        TEST_TextMessageError(lowLevelSetCurrentDatabase(ps.getEConn(),
                                                         default_db),
                              "Failed to set default embedded database!");
    }

    (*qr)->output->beforeQuery(ps.getConn(), ps.getEConn());
    (*qr)->output->getQuery(out_queryz, schema); // 将重写语句结果从*qr存入out_queryz

    return;
}

/*
static void
printEC(std::unique_ptr<Connect> e_conn, const std::string & command) {
    DBResult * dbres;
    assert_s(e_conn->execute(command, dbres), "command failed");
    ResType res = dbres->unpack();
    printRes(res);
}
*/


static void
printEmbeddedState(const ProxyState &ps) {
/*
    printEC(ps.e_conn, "show databases;");
    printEC(ps.e_conn, "show tables from pdb;");
    std::cout << "regular" << std::endl << std::endl;
    printEC(ps.e_conn, "select * from pdb.MetaObject;");
    std::cout << "bleeding" << std::endl << std::endl;
    printEC(ps.e_conn, "select * from pdb.BleedingMetaObject;");
    printEC(ps.e_conn, "select * from pdb.Query;");
    printEC(ps.e_conn, "select * from pdb.DeltaOutput;");
*/
}


void
prettyPrintQuery(const std::string &query)
{
    std::cout << std::endl << RED_BEGIN
              << "QUERY: " << COLOR_END << query << std::endl;
}

static void
prettyPrintQueryResult(const ResType &res)
{
    std::cout << std::endl << RED_BEGIN
              << "RESULTS: " << COLOR_END << std::endl;
    printRes(res);
    std::cout << std::endl;
}

EpilogueResult
queryEpilogue(const ProxyState &ps, const QueryRewrite &qr,
              const ResType &res, const std::string &query,
              const std::string &default_db, bool pp)
{
    qr.output->afterQuery(ps.getEConn());

    const QueryAction action = qr.output->queryAction(ps.getConn());
    if (QueryAction::AGAIN == action) {
        std::unique_ptr<SchemaCache> schema_cache(new SchemaCache());
        const EpilogueResult &epi_res =
            executeQuery(ps, query, default_db, schema_cache.get(), pp);
        TEST_Sync(schema_cache->cleanupStaleness(ps.getEConn()),
                  "failed to cleanup cache after requery!");
        return epi_res;
    }

    if (pp) {
        printEmbeddedState(ps);
        prettyPrintQueryResult(res); // 输出解密前的结果集
    }

    if (qr.output->doDecryption()) {
        const ResType &dec_res =
            Rewriter::decryptResults(res, qr.rmeta);
        assert(dec_res.success());
        if (pp) {
            prettyPrintQueryResult(dec_res); // 输出解密后的结果集
        }

        return EpilogueResult(action, dec_res);
    }

    return EpilogueResult(action, res);
}

/*
 * change
 */
ResType
resultEpilogue(const ProxyState &ps, const QueryRewrite &qr,
              const ResType &res, const std::string &query,
              const std::string &default_db, bool pp){
    qr.output->afterQuery(ps.getEConn());

    if (pp) {
        printEmbeddedState(ps);
        prettyPrintQueryResult(res); // 输出解密前的结果集
    }

    if (qr.output->doDecryption()) {
        const ResType &dec_res =
            Rewriter::decryptResults(res, qr.rmeta);
        assert(dec_res.success());
        if (pp) {
            prettyPrintQueryResult(dec_res); // 输出解密后的结果集
        }

        return dec_res;
    }

    return res;
}

/**
 * Toss a p-biased coin to decide if we need to generate a new salt.
 */
bool
tossACoin(const double &p) {
	std::bernoulli_distribution dist(1 - p);
	std::random_device rd;
	std::mt19937 engine(rd());
	return dist(engine);
}


/**
 * This function is called to get a salt either newly generated or chosen and return to the insert function.
 */
std::string
getSalt(std::vector<double> &params, const Item &item,
		const std::string &db_name, const std::string &table_name,
		const std::string &field_name,
		Analysis &a, rapidjson::Document &doc)
{
	const double val = RiboldMYSQL::val_real(item);

	// Fetch parameters from params.
	// Each field should has its alpha 0, interval_num 1, and p 2, salt_length 3, begin 4, end 5, total 6.
	const double alpha = params[0];
	const unsigned int interval_num = (unsigned int)params[1];
	const double p = params[2];
	const unsigned int salt_length = (unsigned int)params[3];
	const std::pair<unsigned int, unsigned int> range =
			std::make_pair((unsigned int)params[4], (unsigned int)(params[5]));

	auto interval = getIntervalForItem(interval_num, range, val);
	std::vector<std::unique_ptr<Salt>> &salt_table =
			a.salt_table[Interval(interval.first, interval.second, db_name, table_name, field_name)];

	if (true == tossACoin(p) || salt_table.empty()) {
		std::cout << "Need new salt" << std::endl;
		salt_table.push_back(std::unique_ptr<Salt>(new Salt(salt_length)));
		doc["total_salt_used"]= ++params[6];
	}

	return chooseSalt(salt_table, alpha, params[6], params[7], interval, a, doc);
}

bool
writeSaltTableToJsonDOM(rapidjson::Document &doc,
						const std::pair<unsigned int, unsigned int> &interval,
						const std::vector<std::unique_ptr<Salt>> &salt_table)
{
	rapidjson::Value &salts = doc["salts"];
	assert(salts.IsArray());
	rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

	for (rapidjson::SizeType i = 0; i < salts.Size(); i++) {
		rapidjson::Value &salt_object = salts[i].GetObject();

		if (interval.first == salt_object["begin"].GetUint() && interval.second == salt_object["end"].GetUint()) {
			// Build an array and move it to the content of the salt.
			rapidjson::Value new_content(rapidjson::kArrayType);

			for (auto &item : salt_table) {
				rapidjson::Value salt_item(rapidjson::kObjectType);
				rapidjson::Value count;
				count.SetUint(item.get()->getCount());
				salt_item.AddMember(
						static_cast<rapidjson::GenericStringRef<char>>(item.get()->getSaltName().c_str()),
						count, allocator); // This API is really ugly.

				new_content.PushBack(salt_item, allocator);
				std::cout << new_content.Size() << std::endl;
			}
			rapidjson::Value &salt_content = salt_object["content"];
			salt_content = new_content;

			return true;
		}
	}

	return false; // No such salts. Probably some problems happened.
}

/**
 * TODO: ADD INTERVAL
 */
std::string
chooseSalt(std::vector<std::unique_ptr<Salt>> &salts, const double &alpha,
                const unsigned int &total_salt_used,
                const unsigned int &ptext_size,
				const std::pair<unsigned int, unsigned int> &interval,
				Analysis &a, rapidjson::Document &doc)
{
	unsigned int limit = salts.size() - 1;

	std::random_device rd;
	std::mt19937 engine(rd());
	std::uniform_real_distribution<double> dist(-0.01, limit);

	unsigned int salt_index = 0;

	/**
	 * Choose a salt at random and meet some requirements.
	 */
	while (true) {
	    salt_index = std::ceil(dist(engine));

	    if ((double) (salts[salt_index].get()->getCount() * 1.0 / salts.size()) <=
	        (double) (alpha * (ptext_size + 1) /*Because new item added*/ / total_salt_used)) {
	        break;
	    }
	}

	salts[salt_index].get()->incrementCount();

	doc["ptext_size"] = doc["ptext_size"].GetUint() + 1;

	writeSaltTableToJsonDOM(doc, interval, salts);
	return salts[salt_index].get()->getSaltName();
}


std::pair<unsigned int, unsigned int>
getIntervalForItem(const unsigned int& interval_num,
					const std::pair<unsigned int, unsigned int> &range,
					const double& value)
{
	unsigned int begin = range.first, end = range.second;
	unsigned int length = std::ceil((end - begin) * 1.0 / interval_num);

	unsigned int interval_index = std::floor((value - begin) * 1.0 / length);
	return std::make_pair(begin + interval_index * length, begin + (interval_index + 1) * length);
}
static bool
lowLevelGetCurrentStaleness(const std::unique_ptr<Connect> &e_conn,
                            unsigned int cache_id)
{
    const std::string &query =
        " SELECT stale FROM " + MetaData::Table::staleness() +
        "  WHERE cache_id = " + std::to_string(cache_id) + ";";
    std::unique_ptr<DBResult> db_res;
    TEST_TextMessageError(e_conn->execute(query, &db_res),
                          "failed to get schema!");
    assert(1 == mysql_num_rows(db_res->n));

    const MYSQL_ROW row = mysql_fetch_row(db_res->n);
    const unsigned long *const l = mysql_fetch_lengths(db_res->n);
    assert(l != NULL);

    return string_to_bool(std::string(row[0], l[0]));
}

const SchemaInfo &
SchemaCache::getSchema(const std::unique_ptr<Connect> &conn,
                       const std::unique_ptr<Connect> &e_conn)
{
    if (true == this->no_loads) {
        // Use this cleanup if we can't maintain consistent states.
        /*
        TEST_TextMessageError(cleanupStaleness(e_conn),
                              "Failed to cleanup staleness for first"
                              " usage!");
        */
        TEST_TextMessageError(initialStaleness(e_conn),
                              "Failed to initialize staleness for first"
                              " usage!");
        this->no_loads = false;
    }
    const bool stale = lowLevelGetCurrentStaleness(e_conn, this->id);

    if (true == stale) {
        this->schema.reset(loadSchemaInfo(conn, e_conn));
    }

    assert(this->schema);
    return *this->schema.get();
}

static void
lowLevelAllStale(const std::unique_ptr<Connect> &e_conn)
{
    const std::string &query =
        " UPDATE " + MetaData::Table::staleness() +
        "    SET stale = TRUE;";

    TEST_TextMessageError(e_conn->execute(query),
                          "failed to all stale!");
}

void
SchemaCache::updateStaleness(const std::unique_ptr<Connect> &e_conn,
                             bool staleness)
{
    if (true == staleness) {
        // Make everyone stale.
        lowLevelAllStale(e_conn);
    } else {
        // We are no longer stale.
        this->lowLevelCurrentUnstale(e_conn);
    }
}

bool
SchemaCache::initialStaleness(const std::unique_ptr<Connect> &e_conn)
{
    const std::string seed_staleness =
        " INSERT INTO " + MetaData::Table::staleness() +
        "   (cache_id, stale) VALUES " +
        "   (" + std::to_string(this->id) + ", TRUE);";
    RETURN_FALSE_IF_FALSE(e_conn->execute(seed_staleness));

    return true;
}

bool
SchemaCache::cleanupStaleness(const std::unique_ptr<Connect> &e_conn)
{
    const std::string remove_staleness =
        " DELETE FROM " + MetaData::Table::staleness() +
        "       WHERE cache_id = " + std::to_string(this->id) + ";";
    RETURN_FALSE_IF_FALSE(e_conn->execute(remove_staleness));

    return true;
}
static void
lowLevelToggleCurrentStaleness(const std::unique_ptr<Connect> &e_conn,
                               unsigned int cache_id, bool staleness)
{
    const std::string &query =
        " UPDATE " + MetaData::Table::staleness() +
        "    SET stale = " + bool_to_string(staleness) +
        "  WHERE cache_id = " + std::to_string(cache_id) + ";";

    TEST_TextMessageError(e_conn->execute(query),
                          "failed to unstale current!");
}

void
SchemaCache::lowLevelCurrentStale(const std::unique_ptr<Connect> &e_conn)
{
    lowLevelToggleCurrentStaleness(e_conn, this->id, true);
}

void
SchemaCache::lowLevelCurrentUnstale(const std::unique_ptr<Connect> &e_conn)
{
    lowLevelToggleCurrentStaleness(e_conn, this->id, false);
}

