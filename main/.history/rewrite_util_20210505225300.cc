#include <algorithm>
#include <memory>
#include <random>

#include <main/rewrite_util.hh>
#include <main/rewrite_main.hh>
#include <main/macro_util.hh>
#include <main/metadata_tables.hh>
#include <parser/lex_util.hh>
#include <parser/stringify.hh>
#include <util/enum_text.hh>
#include <main/CryptoHandlers.hh>

#include "parser/rapidjson/document.h"
#include "parser/rapidjson/filereadstream.h"
#include "parser/rapidjson/filewritestream.h"
#include "parser/rapidjson/writer.h"

extern CItemTypesDir itemTypes;

#define LEXSTRING(cstr)            \
    {                              \
        (char *)cstr, sizeof(cstr) \
    }

/**
 * UDF for FHSerach generation!
 *
 * https://dev.mysql.com/doc/dev/mysql-server/latest/classItem__func__udf__decimal.html
 *
 * @param position of the plaintext to be searched.
 * @return the correpsonding encoding with which the ciphertext can be located in the B+ tree on the server side.
 *
 * @see How to generate an item_udf type?
 * 		1. Define the body of udf just as follows.
 * 		2. Store all the args (i.e., the Item) in a List.
 * 		3. Call Item_func_udf_<type>(&<udf_func struct>, List<Item>);
 *
 */
static udf_func u_fhsearch = {
	LEXSTRING("FHSearch"),
	REAL_RESULT, // <- Decimal is also allowed.
	UDFTYPE_FUNCTION,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	0L,
};

void optimize(Item **const i, Analysis &a)
{
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
						   std::vector<std::shared_ptr<RewritePlan>>());

	return itemTypes.do_rewrite(i, solution.chooseOne(), *rp.get(), a); //Why here.
}

TABLE_LIST *
rewrite_table_list(const TABLE_LIST *const t, const Analysis &a)
{
	// Table name can only be empty when grouping a nested join.
	assert(t->table_name || t->nested_join);
	if (t->table_name)
	{
		const std::string plain_name =
			std::string(t->table_name, t->table_name_length);
		// Don't use Analysis::getAnonTableName(...) as it respects
		// aliases and if a table has been aliased with 'plain_name'
		// it will give us the wrong name.
		TEST_DatabaseDiscrepancy(t->db, a.getDatabaseName());
		const std::string anon_name =
			a.translateNonAliasPlainToAnonTableName(t->db, plain_name);
		return rewrite_table_list(t, anon_name);
	}
	else
	{
		return copyWithTHD(t);
	}
}

TABLE_LIST *
rewrite_table_list(const TABLE_LIST *const t,
				   const std::string &anon_name)
{
	TABLE_LIST *const new_t = copyWithTHD(t);
	new_t->table_name = make_thd_string(anon_name);
	new_t->table_name_length = anon_name.size();
	if (false == new_t->is_alias)
	{
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
	if (!tlist.elements)
	{
		return SQL_I_List<TABLE_LIST>();
	}

	TABLE_LIST *tl;
	TEST_DatabaseDiscrepancy(tlist.first->db, a.getDatabaseName());
	if (if_exists && !a.nonAliasTableMetaExists(tlist.first->db,
												tlist.first->table_name))
	{
		tl = copyWithTHD(tlist.first);
	}
	else
	{
		tl = rewrite_table_list(tlist.first, a);
	}

	const SQL_I_List<TABLE_LIST> *const new_tlist =
		oneElemListWithTHD<TABLE_LIST>(tl);

	TABLE_LIST *prev = tl;
	for (TABLE_LIST *tbl = tlist.first->next_local; tbl;
		 tbl = tbl->next_local)
	{
		TABLE_LIST *new_tbl;
		TEST_DatabaseDiscrepancy(tbl->db, a.getDatabaseName());
		if (if_exists && !a.nonAliasTableMetaExists(tbl->db,
													tbl->table_name))
		{
			new_tbl = copyWithTHD(tbl);
		}
		else
		{
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
	List<TABLE_LIST> *const new_tll = new List<TABLE_LIST>();

	List_iterator<TABLE_LIST> join_it(tll);

	for (;;)
	{
		const TABLE_LIST *const t = join_it++;
		if (!t)
		{
			break;
		}

		TABLE_LIST *const new_t = rewrite_table_list(t, a);
		new_tll->push_back(new_t);

		if (t->nested_join)
		{
			new_t->nested_join->join_list =
				rewrite_table_list(t->nested_join->join_list, a);
			return *new_tll;
		}

		if (t->on_expr)
		{
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

Item *
getNewFunc(Item *const lhs, Item *const rhs, const Item_func::Functype &type)
{
	switch(type){
	case Item_func::Functype::LT_FUNC:
	case Item_func::Functype::LE_FUNC:
		return new Item_func_le(lhs, rhs);
	case Item_func::Functype::GT_FUNC:
	case Item_func::Functype::GE_FUNC:
		return new Item_func_ge(lhs, rhs);

	default:
		UNIMPLEMENTED;
	}
}

Item *
getFHSerachUDF(const unsigned int &val)
{
	List<Item> l = List<Item>();
	Item *const arg = new Item_int((unsigned long long)val);
	l.push_back(arg);

	return new Item_func_udf_int(&u_fhsearch, l);
}

Item *
rewrite_args_fh_ope(const Item_func *const item, Analysis &a)
{
	assert(isInequalityFunc(*item));
	TEST_BadItemArgumentCount(item->type(), 2, item->argument_count());

	//Item_func *const item_func = copyWithTHD(item);

	// POS MAX AND POS MIN SHOULD BE TAKEN CARE OF.
	const std::string db_name = a.getDatabaseName();
	Item_field *const item_field = static_cast<Item_field *>(item->arguments()[0]);
	const std::string table_name = item_field->table_name;
	const std::string field_name = item_field->name;

	std::cout << "table name: " << table_name << std::endl;

	//double val = RiboldMYSQL::val_real(*(item->arguments()[1]));
	unsigned int pos = findPos(a.local_table[VariableLocator(db_name, table_name, field_name)],
							    *(item->arguments()[1]),
								item->functype());

	Item *const udf = getFHSerachUDF(pos);

	/**
	 * Rewrite the second argument.
	 *
	 * We should get the anon_onion_name for FHOPE columns.
	 */

	const FieldMeta &fm = a.getFieldMeta(db_name, table_name, field_name);
	OnionMeta *const om = fm.getOnionMeta(onion::oFHOPE);
	const std::string anon_field_name_encoding = om->getAnonOnionName() + "_encoding";
	const std::string anon_table_name = a.getAnonTableName(db_name, table_name);

	Item_field *const anon_item_field =
	            make_item_field(*item_field, anon_table_name, anon_field_name_encoding);

	return getNewFunc(anon_item_field, udf, item->functype());
}

RewritePlan *
gather(const Item &i, Analysis &a)
{
	return itemTypes.do_gather(i, a);
}

void gatherAndAddAnalysisRewritePlan(const Item &i, Analysis &a, const Item *original)
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

LEX *begin_transaction_lex(const std::string &dbname)
{
	static const std::string query = "START TRANSACTION;";
	query_parse *const begin_parse = new query_parse(dbname, query);
	return begin_parse->lex();
}

LEX *commit_transaction_lex(const std::string &dbname)
{
	static const std::string query = "COMMIT;";
	query_parse *const commit_parse = new query_parse(dbname, query);
	return commit_parse->lex();
}

//TODO(raluca) : figure out how to create Create_field from scratch
// and avoid this chaining and passing f as an argument
static Create_field *
get_create_field(const Analysis &a, Create_field *const f,
				 const OnionMeta &om)
{
	const std::string name = om.getAnonOnionName();
	Create_field *new_cf = f;

	// Default value is handled during INSERTion.
	auto save_default = f->def;
	f->def = NULL;

	const auto &enc_layers = a.getEncLayers(om);
	assert(enc_layers.size() > 0);
	for (auto it = enc_layers.begin(); it != enc_layers.end(); it++)
	{
		const Create_field *const old_cf = new_cf;
		new_cf = (*it)->newCreateField(old_cf, name);
	}

	// Restore the default so we don't memleak it.
	f->def = save_default;
	return new_cf;
}

// NOTE: The fields created here should have NULL default pointers
// as such is handled during INSERTion.
std::vector<Create_field *>
rewrite_create_field(const FieldMeta *const fm,
					 Create_field *const f, Analysis &a,
					 bool needEnc, std::vector<rapidjson::Document> &documents)
{
	LOG(cdb_v) << "in rewrite create field for " << *f;

	assert(fm->children.size() > 0);

	auto it = documents.begin();
	std::vector<Create_field *> output_cfields;
	if (!needEnc)
	{
		output_cfields.push_back(f);
	}
	else
	{
		// Don't add the default value to the schema.
		Item *const save_def = f->def;
		f->def = NULL;

		// create each onion column
		for (auto oit : fm->orderedOnionMetas())
		{
			OnionMeta *const om = oit.second;
			Create_field *const new_cf = get_create_field(a, f, *om);

			// TODO (haobin chen): ADD ANON NAME TO MAP!!
			// Get a document object and modify the "anon_field_name" attribute and rewrite it to the file.
			// This is easy because we can get the path of the json file.

			// Also, do not forget to store the key.
			// if it is oFHDET column, get the enc_layer.
			const auto &enc_layers = a.getEncLayers(*om);
			for (auto &item : enc_layers)
			{
				std::cout << item.get()->name() << std::endl;

				// If the type is fh_det, then we store the key into the file.
				if (SECLEVEL::FHDET == item.get()->level() && it != documents.end())
				{
					// And it works.
					// assert(it != documents.end()) <- If this happens, resolve discrepancy.
					rapidjson::Value &key = (*it)["AES_key"];
					const std::string rawkey = item.get()->getKeyForJSON();
					key.SetString(rawkey.c_str(), strlen(rawkey.c_str()), (*it).GetAllocator());

					rapidjson::Value &anon_field_name = (*it)["anon_field_name"];
					anon_field_name.SetString(new_cf->field_name, (*it).GetAllocator());

					// WRITE THE DOCUMENTS BACK ONTO THE DISK.
					std::string dir = "CryptDB_DATA/";
					dir.append((std::string)((*it)["db_name"].GetString()) + "/");
					dir.append((std::string)((*it)["table_name"].GetString()) + "/");
					dir.append((std::string)((*it)["field_name"].GetString()) + ".json");

					assert(writeDomToFile(*it, dir));
					it++;
				}
				else if (SECLEVEL::FHOPE == item.get()->level())
				{
					a.encoding_fields.push_back(std::unique_ptr<Create_field>(item.get()->buildCreateFieldEncoding(new_cf)));
				}
			}

			output_cfields.push_back(new_cf);
		}

		// create salt column
		if (fm->getHasSalt())
		{
			THD *const thd = current_thd;
			Create_field *const f0 = f->clone(thd->mem_root);
			f0->field_name = thd->strdup(fm->getSaltName().c_str());
			// Salt is unsigned and is not AUTO_INCREMENT.
			// > NOT_NULL_FLAG is useful for debugging if mysql strict mode
			//   (ie, STRICT_ALL_TABLES) is turned on.
			f0->flags =
				(f0->flags | UNSIGNED_FLAG | NOT_NULL_FLAG) & ~AUTO_INCREMENT_FLAG;
			f0->sql_type = MYSQL_TYPE_LONGLONG;
			f0->length = 8;

			output_cfields.push_back(f0);
		}

		// Restore the default to the original Create_field parameter.
		f->def = save_def;
	}

	/**
     * Traverse the encoding fields and append them to the end of the list.
     */

	for (auto &item : a.encoding_fields)
	{
		//Create_field * const cf = item.get();

		output_cfields.push_back(item.get());
	}

	return output_cfields;
}

/**
 * Haobin Chen.
 * This function is invoked either when a new document is created, i.e., a new field is created,
 * or when a DIRECTIVE query is issued in order to change the parameters.
 *
 * Consider two scenarios:
 * 		1. New field is created. We just create an empty object and add it to the salt array;
 * 		2. Parameters of the old field is altered by DIRECTIVE SETPARAM query. We should merge the salt.
 */
bool getSaltContentForJSONDocument(rapidjson::Document &doc)
{
	rapidjson::Value &salts = doc["salts"];
	auto allocator = doc.GetAllocator();

	try
	{
		if (!salts.IsArray())
		{
			throw std::runtime_error("INTERNAL JSON ERROR: SALTS MUST BE AN ARRAY.\n");
		}

		// Fetch the range.
		const unsigned int begin = doc["range"].GetArray()[0].GetUint();
		const unsigned int end = doc["range"].GetArray()[1].GetUint();
		const unsigned int interval_num = doc["interval_num"].GetUint();

		const unsigned int length = std::ceil((double)((end - begin + 1) * 1.0 / interval_num));

		// Case 1: empty salt content. This means we've just created a new field.
		// DO COMPUTATION!
		if (0 == salts.Size())
		{
			for (unsigned int i = begin; i + length <= end + 1; i += length)
			{
				rapidjson::Value salt_item(rapidjson::kObjectType); // But what does k mean.

				rapidjson::Value salt_begin, salt_end;
				salt_begin.SetUint(i);
				salt_end.SetUint(i + length);

				rapidjson::Value content(rapidjson::kArrayType);

				salt_item.AddMember("begin", salt_begin, allocator);
				salt_item.AddMember("end", salt_end, allocator);
				salt_item.AddMember("content", content, allocator);

				salts.PushBack(salt_item, allocator);
			}

			// Case 2: invoked by handleDirective that wants to modify the parameters.
			// TODO: Merge the salts.
			// Tricky because we may need to split the interval and allocate the salt to the divided intervals.
		}
		else
		{
		}
	}
	catch (std::runtime_error re)
	{
		std::cout << re.what();

		return false;
		// Just return false, and this will not not modify the json file.
		// (Because we cannot write to a non-array type. :P)
	}

	return true;
}

std::vector<onion>
getOnionIndexTypes()
{
	return std::vector<onion>({oOPE, oDET, oPLAIN});
}

static std::string
getOriginalKeyName(Key *const key)
{
	if (Key::PRIMARY == key->type)
	{
		return "PRIMARY";
	}

	const std::string out_name = convert_lex_str(key->name);
	TEST_TextMessageError(out_name.size() > 0,
						  "Non-Primary keys can not have blank name!");

	return out_name;
}

bool loadLocalTable(const std::string &path, rapidjson::Document &doc)
{
	return true;
}

/**
 * Haobin Chen.
 *
 * When we have either deleted or updated several rows in the table, we must check if there are any rows
 * containing salts; in other words, we have to update the salt table as well, to keep synchronized.
 *
 * If we let the salt table grow and never delete something from it, the table could grow at a relatively
 * unimaginable speed, which is not so ideal for the implementation of CryptDB.
 */
bool updateSaltTable(const ResType &dbres, Analysis &a, bool update)
{
	//printRes(dbres);
	/**
	 * TODO: Debug info, delete it after implementation.
	 */
	const unsigned int row_count = dbres.rows.size();
	std::cout << "In delete / update: we have detected " << row_count << " rows to be deleted / updated." << std::endl;

	// Un-anonymize the columns.
	for (unsigned int i = 0; i < dbres.names.size(); i++)
	{
		// TODO: maybe we should maintain somehow a table in Analysis, to look up the plain field name for FH-columns?

		/**
		 * Step 1: Check the table if it is a fh-column by looking up the table stored in Analysis.
		 * Step 2: get the length of the salt.
		 * Step 3: Traverse rows that belong to it, and modify the corresponding salt table (by calculating the interval)
		 */

		auto iter = a.anon_to_plain.find(dbres.names[i]);
		if (a.anon_to_plain.end() != iter)
		{
			// We will insert the mapping from anon to plain of the fh-columns when creating the table; thus
			// it is easy for us to get the plain table name which is definitely with prefix "fh_".

			// Get the AES key from JSON document.
			rapidjson::Document doc;
			char readBuffer[65535];
			std::string dir = "CryptDB_DATA/";
			dir.append(a.getDatabaseName() + "/");
			dir.append(a.table_name_last_used + "/");
			dir.append(iter->second + ".json");

			FILE *file = fopen(dir.c_str(), "r");

			rapidjson::FileReadStream frs(file, readBuffer, sizeof(readBuffer));
			doc.ParseStream(frs);
			//const AES_KEY *const deckey = get_AES_dec_key((std::string)(doc["AES_key"].GetString()));
			const std::string rawkey = (std::string)(doc["AES_key"].GetString());

			const unsigned int salt_length = doc["salt_length"].GetUint();

			std::vector<double> &params =
				a.variables[VariableLocator(a.getDatabaseName(), a.table_name_last_used, iter->second)];

			// TODO: UPDATE SALT TABLE.
			/**
			 * Traverse the corresponding rows.
			 */
			for (unsigned int r = 0; r < dbres.rows.size(); r++)
			{
				doc["ptext_size"] = doc["ptext_size"].GetUint() - 1;
				params[7] = doc["ptext_size"].GetUint();
				// TODO: extract salt from the encrypted value and then update salt table.
				const std::string dec = decrypt_SM4_EBC(ItemToString(*(dbres.rows[r][i].get())), rawkey);

				const unsigned int val = stoull(dec.substr(0, dec.size() - salt_length));
				const std::string salt_str = dec.substr(dec.size() - salt_length);
				//std::cout << salt_str << std::endl;
				const std::pair<unsigned int, unsigned int> interval =
					getIntervalForItem(doc["interval_num"].GetUint(),
									   std::make_pair(doc["range"][0].GetUint(), doc["range"][1].GetUint()),
									   val);

				std::vector<std::unique_ptr<Salt>> &salt_table =
					a.salt_table[Interval(interval.first, interval.second,
										  a.getDatabaseName(), a.table_name_last_used, iter->second)];
				auto lambda = [&](const std::unique_ptr<Salt> &item) {
					return item.get()->getSaltName() == salt_str;
				};

				const auto &salt_item = std::find_if(salt_table.begin(), salt_table.end(), lambda);

				if (salt_table.end() != salt_item && false == (*salt_item).get()->decrementCount())
				{
					salt_table.erase(salt_item);
					doc["total_salt_used"] = doc["total_salt_used"].GetDouble() - 1;
					params[6] = doc["total_salt_used"].GetDouble();
				}

				/**
				 * Check if the SQL query is UPDATE type.
				 */

				/*if (update) {
					auto findValueItem = [&](const std::pair<Item_field *const, Item *const> &item) {
						return 0 == iter->second.compare((item.first)->field_name);
					};

					auto field_value_pair =
							std::find_if(a.field_value_pairs.begin(), a.field_value_pairs.end(), findValueItem);

					if (a.field_value_pairs.end() != field_value_pair) {
						getSalt(params, *(field_value_pair->second), a.getDatabaseName(), a.table_name_last_used, iter->second, a, doc);
						}
					}
				}*/
				writeSaltTableToJsonDOM(doc, interval, salt_table);
				writeDomToFile(doc, dir);
			}
		}
	}

	return true;
}

/**
 * Plaintext could possibly not exist in the local_table;
 * be aware of the edge conditions.
 * 		> If there is any equality within inequality, we should transform a little bit. Say, we have
 * 		  encoding > 2, we should locate 2's position by NOT adding items that are equal to 2.
 *
 */
/*
unsigned int
findPosMin(const std::map<double, unsigned int> &local_table, const double &pmin, bool eq)
{
	// In our case it is more convenient to traverse the map
	// BUT this is definitely not the most efficient way.
	unsigned int pos_min = 0;
	unsigned int last = 0;

	for (auto item : local_table)
	{
		if (item.first >= pmin)
		{
			pos_min += item.second;
			last = item.second;
		}
		else
		{
			break;
		}
	}

	return eq ? pos_min - last : pos_min;
}

unsigned int
findPosMax(const std::map<double, unsigned int> &local_table, const double &pmax, bool eq)
{
	unsigned int pos_max = 0;
	unsigned int last = 0;
	for (auto item : local_table)
	{
		if (item.first <= pmax)
		{
			pos_max += item.second;
		}
		else
		{
			pos_max += 1;
			break;
		}
	}

	return eq ? pos_max : pos_max - last;
}
*/
unsigned int
findPos(const std::map<double, unsigned int> &local_table,
		const Item &item,
		const Item_func::Functype &type)
{
	double val = RiboldMYSQL::val_real(item);

	unsigned int pos = 0;
	unsigned int last = 0;

	for (auto item : local_table)
	{
		if (item.first <= val)
		{
			pos += item.second;
			last = item.second;
		} else {
			break;
		}
	}

	/**
	 * Adjust edge conditions
	 */
	if (Item_func::Functype::GE_FUNC == type)
	{
		pos -= last - 1;
	}
	else if (Item_func::Functype::LT_FUNC == type)
	{
		pos -= last;
	}

	return pos;
}

bool
loadAllSaltsFromFile(const std::string &db_name, const TABLE_LIST *const table_list, Analysis &a)
{
	std::string path = "CryptDB_DATA/";
	path.append(db_name + "/");
	path.append(table_list->table_name); // We do not consider alias.

	std::vector<std::string> jsons = getFiles(path);

	for (auto item : jsons)
	{
		std::string dir = path + "/";
		dir.append(item);
		std::cout << dir << std::endl;
		rapidjson::Document doc;
		getDocumentFromFileAndLoadSalt(dir, a, doc);
	}

	return true;
}

// TODO: should be this function be implemented?
unsigned int
getPos(const Item &i, Analysis &a)
{
	return 0;
}

bool issueSelectForDeleteOrUpdate(Analysis &a, const LEX *const lex, const ProxyState &ps, bool update)
{
	std::string select_clause =
		update ? generateEquivalentSelectStatementForUpdate(lex) : generateEquivalentSelectStatementForDelete(lex);

	DMLOutput *output = new DMLOutput(a.select_plain_for_update_or_delete, select_clause);
	std::unique_ptr<QueryRewrite> qr = std::unique_ptr<QueryRewrite>(new QueryRewrite(true, a.rmeta, output));
	std::unique_ptr<DBResult> dbres;

	std::cout << "select_clause: " << select_clause << std::endl;

	TEST_Sync(ps.getConn()->execute(select_clause, &dbres, qr.get()->output->multipleResultSets()),
			  "failed to execute query for delete / update subquery!!!");

	/** If SQL query within the proxy and the delete handler has not yet issued a rewritten SQL query, we should
     *  unpack the (forced) issued query.
     */
	const ResType &res = dbres.get()->unpack();

	return updateSaltTable(res, a, update);
}

std::vector<Key *>
rewrite_key(const TableMeta &tm, Key *const key, const Analysis &a)
{
	std::vector<Key *> output_keys;

	const std::vector<onion> key_onions = getOnionIndexTypes();
	for (auto onion_it : key_onions)
	{
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
		for (;;)
		{
			const Key_part_spec *const key_part = col_it++;
			if (NULL == key_part)
			{
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
			if (NULL == om)
			{
				break;
			}

			new_key_part->field_name =
				string_to_lex_str(om->getAnonOnionName());
			new_key->columns.push_back(new_key_part);
		}
	}

	// Only create one PRIMARY KEY.
	if (Key::PRIMARY == key->type)
	{
		if (output_keys.size() > 0)
		{
			return std::vector<Key *>({output_keys.front()});
		}
	}

	return output_keys;
}

std::string
bool_to_string(bool b)
{
	if (true == b)
	{
		return "TRUE";
	}
	else
	{
		return "FALSE";
	}
}

bool string_to_bool(const std::string &s)
{
	if (s == std::string("TRUE") || s == std::string("1"))
	{
		return true;
	}
	else if (s == std::string("FALSE") || s == std::string("0"))
	{
		return false;
	}
	else
	{
		throw "unrecognized string in string_to_bool!";
	}
}

bool dropJson(const std::string &path)
{
	std::string command = "rm -rf ";
	command.append(path);

	system(command.c_str());

	return true;
}

rapidjson::Document
buildEmptyJSONForNewFHField(const std::string &dbname,
							const std::string &table_name,
							const std::string &field_name)
{
	rapidjson::Document doc;
	doc.SetObject();
	auto allocator = doc.GetAllocator();
	/**
	 * Fill in the basic information.
	 */
	rapidjson::Value v;
	v.SetString(dbname.c_str(), strlen(dbname.c_str()), allocator);
	doc.AddMember("db_name", v, allocator);
	v.SetString(table_name.c_str(), strlen(table_name.c_str()), allocator);
	doc.AddMember("table_name", v, allocator);
	v.SetString(field_name.c_str(), strlen(field_name.c_str()), allocator);
	doc.AddMember("field_name", v, allocator);
	v.SetString("");
	doc.AddMember("anon_field_name", v, allocator);
	v.SetString("");
	doc.AddMember("AES_key", v, allocator);

	/**
	 * Set params to be default values.
	 */
	v.SetDouble(1.0);
	doc.AddMember("alpha", v, allocator);
	v.SetUint(2);
	doc.AddMember("interval_num", v, allocator);
	v.SetDouble(0.05);
	doc.AddMember("p", v, allocator);
	v.SetUint(extractSaltLengthFromField(field_name));
	doc.AddMember("salt_length", v, allocator);
	v.SetArray();
	v.PushBack(1, allocator).PushBack(1000, allocator);
	doc.AddMember("range", v, allocator);
	v.SetUint(0);
	doc.AddMember("total_salt_used", v, allocator);
	v.SetUint(0);
	doc.AddMember("ptext_size", v, allocator);
	v.SetArray();
	doc.AddMember("salts", v, allocator);
	v.SetArray();
	doc.AddMember("local_table", v, allocator);
	//The salt array should be computed later... Add a function to implement this.

	assert(getSaltContentForJSONDocument(doc));

	return doc;
}

List<Create_field>
createAndRewriteField(Analysis &a, const ProxyState &ps,
					  Create_field *const cf,
					  TableMeta *const tm, bool new_table,
					  List<Create_field> &rewritten_cfield_list,
					  std::string plain_table_name)
{
	/*
	 * judge first.
	 */
	std::vector<rapidjson::Document> documents;
	const std::string name = std::string(cf->field_name);
	auto buildFieldMeta =
		[](const std::string name, Create_field *const cf,
		   const ProxyState &ps, TableMeta *const tm) {
			std::cout << "Createing new fieldmeta now!" << std::endl;
			return new FieldMeta(name, cf, ps.getMasterKey().get(),
								 ps.defaultSecurityRating(),
								 tm->leaseIncUniq());
		};
	std::unique_ptr<FieldMeta> fm(buildFieldMeta(name, cf, ps, tm));

	/**
     * TODO: create a direcotry for json table first.
     */
	if (needFrequencySmoothing(name))
	{
		std::string dir = "CryptDB_DATA/";
		dir.append(a.getDatabaseName() + '/');
		dir.append(plain_table_name);
		std::string command = "mkdir -p " + dir;
		system(command.c_str());

		// TODO: ADD AN EMPTY JSON
		documents.push_back(buildEmptyJSONForNewFHField(a.getDatabaseName(), plain_table_name, name));
	}

	bool needEnc =
		needFrequencySmoothing(name) ||
		needEncryption(name);

	// -----------------------------
	//         Rewrite FIELD
	// -----------------------------
	const auto new_fields = rewrite_create_field(fm.get(), cf, a, needEnc, documents); // Write document back.
	rewritten_cfield_list.concat(vectorToListWithTHD(new_fields));

	// -----------------------------
	//         Update FIELD
	// -----------------------------

	// Here we store the key name for the first time. It will be applied
	// after the Delta is read out of the database.
	if (true == new_table)
	{
		tm->addChild(IdentityMetaKey(name), std::move(fm));
	}
	else
	{
		a.deltas.push_back(std::unique_ptr<Delta>(
			new CreateDelta(std::move(fm), *tm,
							IdentityMetaKey(name))));
		a.deltas.push_back(std::unique_ptr<Delta>(
			new ReplaceDelta(*tm,
							 a.getDatabaseMeta(a.getDatabaseName()))));
	}

	return rewritten_cfield_list;
}

salt_type
padIV(const std::string &field_name, const salt_type &IV)
{
	const unsigned int length = extractSaltLengthFromField(field_name);

	// We are sure that this IV is an integer.
	std::string str = std::to_string(IV);
	const unsigned int sl = str.size();

	assert(str.size() <= length);
	for (unsigned int i = 0; i < length - sl; i++)
	{
		str.append("0");
	}

	return std::stoul(str);
}

UpdateOPE *
generateUpdateOPE(const double &plaintext,
				  const std::string &ciphertext,
				  const OnionMeta &om,
				  const std::string &table_name,
				  std::map<double, unsigned int> &local_table)
{
	auto lambda = [&](const std::pair<double, unsigned int> &item) {
		return item.first == plaintext;
	};

	auto accu = [](const unsigned int &lhs, const std::pair<double, unsigned int> &rhs) {
		return lhs + rhs.second;
	};

	/**
	 * There are two case:
	 * 		> No such plaintext (special);
	 * 		> It contains a plaintext.
	 */

	auto iter = std::find_if(local_table.begin(), local_table.end(), lambda);

	local_table[plaintext] += 1;

	// case 1: we should insert a plaintext into the table.
	if (local_table.end() == iter)
	{
		iter = std::find_if(local_table.begin(), local_table.end(), lambda);
	}

	unsigned int distance = std::distance(local_table.begin(), iter);
	//std::cout << "distance: " <<distance << std::endl;
	const unsigned int total = std::accumulate(local_table.begin(), std::next(local_table.begin(), distance), 0, accu);
	//std::cout << "pos before this ciphertext: " << total << std::endl;
	/**
	 * Generate a random position.
	 *
	 * Be aware of the range.
	 */
	std::uniform_int_distribution<int> dist(total + 1, total + iter->second);
	std::random_device rd;
	std::mt19937 engine(rd());
	const unsigned int pos = dist(engine);
	const std::string ope_column_name = om.getAnonOnionName();

	return new UpdateOPE(pos, ciphertext, table_name, ope_column_name + "_encoding", ope_column_name, getRandomString(12));
}

//TODO: which encrypt/decrypt should handle null?

// We cannot modify this encrypt function bacause it is no use to append a single item with multiple predicates.
// This function only encrypts one single item in the predicate.
// For example, it encrypts the integer 123 from `FH_id = 123`.

// I think we should also introduce the corresponding local table into the encryption interface?
Item *
encrypt_item_layers(const Item &i, onion o, const OnionMeta &om,
					Analysis &a, rapidjson::Document &doc, uint64_t IV,
					const std::string &field_name)
{
	assert(!RiboldMYSQL::is_null(i));

	const auto &enc_layers = a.getEncLayers(om);
	assert_s(enc_layers.size() > 0, "onion must have at least one layer");
	// We can also modify the item itself by using a pointer which points to its address.
	const Item *enc = &i;
	Item *new_enc = NULL;

	for (auto it = enc_layers.begin(); it != enc_layers.end(); it++)
	{
		std::cout << "encrypt layer "
				  << TypeText<SECLEVEL>::toText((*it)->level()) << "\n";
		if (SECLEVEL::FHOPE == (*it)->level())
		{
			// set to zero.
			const std::string db_name = a.getDatabaseName();
			const std::string table_name = a.table_name_last_used;

			std::map<double, unsigned int> &lt = a.local_table[VariableLocator(db_name, table_name, field_name)];
			salt_type new_iv =
				lt.count(RiboldMYSQL::val_real(i)) == 0 ? 1 : lt[RiboldMYSQL::val_real(i)];

			std::cout << "encrypting for OPE item whose name is " << i.name << std::endl;
			new_enc = (*it)->encrypt(*enc, padIV(field_name, new_iv));
			// Then we should issue a call to pro_insert.

			bool is_null;
			const std::string anon_table_name = a.getAnonTableName(db_name, table_name);
			UpdateOPE *ope = generateUpdateOPE(RiboldMYSQL::val_real(*enc),
											   RiboldMYSQL::val_str(*new_enc, &is_null),
											   om, anon_table_name, lt);
			std::cout << *ope;
			a.updateOPEs.push_back(std::unique_ptr<UpdateOPE>(ope));

			/**
        	 * FIXME: JSON FILE synchronization failed.
        	 */
			writeLocalTableToJsonDOM(doc, lt);
		}
		else
		{
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
rewriteAndGetSingleQuery(ProxyState &ps, const std::string &q,
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
	escaped(new char[escaped_length], &operator delete[]);
	c->real_escape_string(escaped.get(), escape_me.c_str(),
						  escape_me.size());

	return std::string(escaped.get());
}

void encrypt_item_all_onions(const Item &i, const FieldMeta &fm,
							 uint64_t IV, Analysis &a, std::vector<Item *> *l,
							 rapidjson::Document &doc)
{
	for (auto it : fm.orderedOnionMetas())
	{
		const onion o = it.first->getValue();
		OnionMeta *const om = it.second;
		l->push_back(encrypt_item_layers(i, o, *om, a, doc, IV, fm.fname));
	}
}

bool writeDomToFile(const rapidjson::Document &doc, const std::string &path)
{
	char writeBuffer[65536];

	FILE *out_file = fopen(path.c_str(), "w");
	rapidjson::FileWriteStream fws(out_file, writeBuffer, sizeof(writeBuffer));

	rapidjson::Writer<rapidjson::FileWriteStream> writer(fws);
	doc.Accept(writer);
	fclose(out_file);

	return true;
}

/**
 * This will also load the local table into Analysis.
 */
bool getDocumentFromFileAndLoadSalt(const std::string &path, Analysis &a, rapidjson::Document &doc, bool load)
{
	FILE *in_file = fopen(path.c_str(), "r");
	char read_buffer[65536];
	rapidjson::FileReadStream frs(in_file, read_buffer, sizeof(read_buffer));

	// Parse the file into DOM.
	doc.ParseStream(frs);

	if (load)
	{
		a.loadSaltsFromJsonDOM(doc, "0");
		a.loadLocalTable(doc);
	}

	fclose(in_file);

	return true;
}

/**
 * TODO: Update salt table by tossing a coin.
 */
void typical_rewrite_insert_type(const Item &i, const FieldMeta &fm,
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
	const std::string &table_name = a.table_name_last_used;

	if (needFrequencySmoothing(field_name))
	{
		/**
		 * TODO: Loading local table into Analysis should also be implemented.
		 */

		std::vector<double> &params = a.variables[VariableLocator(db_name, table_name, field_name)];
		/**
		 * TODO: ADD SUPPORT FOR OPE!
		 */
		// Read from file.
		std::string dir = "CryptDB_DATA/";
		dir.append(db_name + '/');
		dir.append(table_name + "/");
		dir.append(field_name + ".json");

		rapidjson::Document doc;
		assert(getDocumentFromFileAndLoadSalt(dir, a, doc));
		salt_type IV = stoull(getSalt(params, i, db_name, table_name, field_name, a, doc));

		// TODO: write the document back!
		encrypt_item_all_onions(i, fm, IV, a, l, doc);
		assert(writeDomToFile(doc, dir));
	}
	else
	{
		rapidjson::Document doc;
		encrypt_item_all_onions(i, fm, salt, a, l, doc);
	}

	if (0 != salt)
	{
		l->push_back(new Item_int(static_cast<ulonglong>(salt)));
	}
}

bool
isDETFunc(const Item_func &item)
{
    switch (item.functype())
    {
    case Item_func::Functype::EQ_FUNC:
    case Item_func::Functype::NE_FUNC:
    case Item_func::Functype::EQUAL_FUNC:
        return true;
    default:
        return false;
    }

    return false;
}

bool
isInequalityFunc(const Item_func &item)
{
	switch (item.functype())
	{
	case Item_func::Functype::LE_FUNC:
	case Item_func::Functype::GE_FUNC:
	case Item_func::Functype::LT_FUNC:
	case Item_func::Functype::GT_FUNC:
		return true;
	default:
		return false;
	}

	return false;
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

bool needFrequencySmoothing(const std::string &field_name)
{
	return 0 == field_name.substr(0, 3).compare(FH_IDENTIFIER);
}

std::vector<std::string>
getFiles(const std::string &path)
{
	DIR *dir = opendir(path.c_str());
	std::vector<std::string> ans;

	struct dirent *entry = readdir(dir);

	while (entry != NULL)
	{
		if (entry->d_type != DT_DIR)
		{
			const std::string name = entry->d_name;

			/**
	    	 * We only need to load frequency smoothing columns and not temporary file!
	    	 */
			if (needFrequencySmoothing(name) && '~' != name.back())
			{
				ans.push_back((std::string)(entry->d_name));
			}
		}

		entry = readdir(dir);
	}

	closedir(dir);

	return ans;
}

bool needEncryption(const std::string &field_name)
{
	return 0 == field_name.substr(0, 4).compare(ENC_IDENTIFIER);
}

void escapeCipherOPE(const ProxyState &ps, UpdateOPE &ope)
{
	ope.ciphertext = escapeString(ps.getConn(), ope.ciphertext);
	ope.ciphertext = "\'" + ope.ciphertext;
	ope.ciphertext = ope.ciphertext + "\'";
}

bool retrieveDefaultDatabase(unsigned long long thread_id,
							 const std::unique_ptr<Connect> &c,
							 std::string *const out_name)
{
	out_name->clear();

	std::unique_ptr<DBResult> dbres;
	const std::string &query =
		" SELECT db FROM INFORMATION_SCHEMA.PROCESSLIST"
		"  WHERE id = " +
		std::to_string(thread_id) + ";";
	RETURN_FALSE_IF_FALSE(c->execute(query, &dbres));
	RETURN_FALSE_IF_FALSE(mysql_num_rows(dbres->n) == 1);

	const MYSQL_ROW row = mysql_fetch_row(dbres->n);
	const unsigned long *const l = mysql_fetch_lengths(dbres->n);
	*out_name = std::string(row[0], l[0]);
	return true;
}

// 获得重写后的SQL语句集
void queryPreamble(ProxyState &ps, const std::string &q,
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

	/**
     * Call pro_insert.
     */
	/*
    if (!ps.updateOPEs.empty()) {
    	// TODO: generate a call sql.
    	//std::cout << "About to generate call!\n";
    	for (auto &ope : ps.updateOPEs) {
    		escapeCipherOPE(ps, *ope.get());
    		std::ostringstream oss;
    		oss << *ope.get();
    		std::cout << oss.str() << std::endl;
    		out_queryz->push_back(oss.str());
    	}
    }
    */

	// We handle before any queries because a failed query
	// may stale the database during recovery and then
	// we'd have to handle there as well.
	schema_cache->updateStaleness(ps.getEConn(),
								  (*qr)->output->stalesSchema());

	// ASK bites again...
	// We want the embedded database to reflect the metadata for the
	// current remote connection.
	if ((*qr)->output->usesEmbeddedDB())
	{
		TEST_TextMessageError(lowLevelSetCurrentDatabase(ps.getEConn(),
														 default_db),
							  "Failed to set default embedded database!");
	}

	(*qr)->output->beforeQuery(ps.getConn(), ps.getEConn());
	(*qr)->output->getQuery(out_queryz, schema); // 将重写语句结果从*qr存入out_queryz

	// out_queryz->push_back("use xinan");
	// std::cout << "strange.\n" << std::endl;

	/*if (!ps->updateOPEs.empty()) {
    	// TODO: generate a call sql.
        std::cout << "About to generate call!\n";
    }*/
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
printEmbeddedState(const ProxyState &ps)
{
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

void prettyPrintQuery(const std::string &query)
{
	std::cout << std::endl
			  << RED_BEGIN
			  << "QUERY: " << COLOR_END << query << std::endl;
}

static void
prettyPrintQueryResult(const ResType &res)
{
	std::cout << std::endl
			  << RED_BEGIN
			  << "RESULTS: " << COLOR_END << std::endl;
	printRes(res);
	std::cout << std::endl;
}

EpilogueResult
queryEpilogue(ProxyState &ps, const QueryRewrite &qr,
			  const ResType &res, const std::string &query,
			  const std::string &default_db, bool pp)
{
	qr.output->afterQuery(ps.getEConn());

	const QueryAction action = qr.output->queryAction(ps.getConn());
	if (QueryAction::AGAIN == action)
	{
		std::unique_ptr<SchemaCache> schema_cache(new SchemaCache());
		const EpilogueResult &epi_res =
			executeQuery(ps, query, default_db, schema_cache.get(), pp);
		TEST_Sync(schema_cache->cleanupStaleness(ps.getEConn()),
				  "failed to cleanup cache after requery!");
		return epi_res;
	}

	if (pp)
	{
		printEmbeddedState(ps);
		prettyPrintQueryResult(res); // 输出解密前的结果集
	}

	if (qr.output->doDecryption())
	{
		const ResType &dec_res =
			Rewriter::decryptResults(res, qr.rmeta);
		assert(dec_res.success());
		if (pp)
		{
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
			   const std::string &default_db, bool pp)
{
	qr.output->afterQuery(ps.getEConn());

	if (pp)
	{
		printEmbeddedState(ps);
		prettyPrintQueryResult(res); // 输出解密前的结果集
	}

	if (qr.output->doDecryption())
	{
		const ResType &dec_res =
			Rewriter::decryptResults(res, qr.rmeta);
		assert(dec_res.success());
		if (pp)
		{
			prettyPrintQueryResult(dec_res); // 输出解密后的结果集
		}

		return dec_res;
	}

	return res;
}

/**
 * Toss a p-biased coin to decide if we need to generate a new salt.
 */
bool tossACoin(const double &p)
{
	std::bernoulli_distribution dist(p);
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
	VariableLocator vl(db_name, table_name, field_name);
	std::cout << "ptext size: " << doc["ptext_size"].GetUint() << std::endl;
	doc["ptext_size"] = doc["ptext_size"].GetUint() + 1;
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

	const int salt = chooseSalt(salt_table, alpha, params[6], params[7], interval, a, doc);

	if (true == tossACoin(p) || -1 == salt)
	{
		std::unique_ptr<Salt> item(new Salt(salt_length));

		salt_table.push_back(std::move(std::unique_ptr<Salt>(new Salt(salt_length))));
		std::cout << "salt_table: " << salt_table.size() << std::endl;
		doc["total_salt_used"] = ++params[6];

		writeSaltTableToJsonDOM(doc, interval, salt_table);
		return salt_table.back().get()->getSaltName();
	}

	salt_table[salt].get()->incrementCount();
	writeSaltTableToJsonDOM(doc, interval, salt_table);
	return salt_table[salt].get()->getSaltName();
}

bool writeAllTablesToJsonDOM(rapidjson::Document &doc,
							 const std::pair<unsigned int, unsigned int> &interval,
							 const std::vector<std::unique_ptr<Salt>> &salt_table,
							 const std::map<double, unsigned int> &local_table)
{
	return writeLocalTableToJsonDOM(doc, local_table) &&
		   writeSaltTableToJsonDOM(doc, interval, salt_table);
}

bool writeLocalTableToJsonDOM(rapidjson::Document &doc,
							  const std::map<double, unsigned int> &local_table)
{
	rapidjson::Document::AllocatorType &allocator = doc.GetAllocator();

	rapidjson::Value new_local_table(rapidjson::kArrayType);

	for (auto &item : local_table)
	{
		rapidjson::Value obj(rapidjson::kObjectType);
		rapidjson::Value counter;
		counter.SetUint(item.second);

		// std::cout << item.first << ", " << item.second << std::endl;

		const std::string plaintext = std::to_string(item.first);
		rapidjson::Value plain;

		// plain.SetString(rapidjson::StringRef(plaintext.c_str())); <- It will cause trouble.

		plain.SetString(plaintext.c_str(), strlen(plaintext.c_str()), allocator);

		obj.AddMember(plain, counter, allocator);

		new_local_table.PushBack(obj, allocator);
	}

	/*
	 * For debug...
	for (unsigned int i = 0; i < local_table.size(); i++) {
		rapidjson::Value obj(rapidjson::kObjectType);

		new_local_table.PushBack(obj, allocator);
	}

	rapidjson::SizeType i = 0;
	auto iter = local_table.begin();

	while (i < new_local_table.Size() && iter != local_table.end()) {
		rapidjson::Value &obj = new_local_table[i].GetObject();
		rapidjson::Value counter;
		counter.SetUint(iter->second);

		const std::string ptext = std::to_string(iter->first);
		rapidjson::Value ptext_str;
		ptext_str.SetString(ptext.c_str(), strlen(ptext.c_str()), allocator);

		obj.AddMember(ptext_str, counter, allocator);

		iter++;
		i++;
	}*/

	rapidjson::Value &local = doc["local_table"].GetArray();
	local.Clear();
	local = new_local_table;

	return true;
}

bool writeSaltTableToJsonDOM(rapidjson::Document &doc,
							 const std::pair<unsigned int, unsigned int> &interval,
							 const std::vector<std::unique_ptr<Salt>> &salt_table)
{
	rapidjson::Value &salts = doc["salts"];
	assert(salts.IsArray());
	rapidjson::Document::AllocatorType &allocator = doc.GetAllocator();

	for (rapidjson::SizeType i = 0; i < salts.Size(); i++)
	{
		rapidjson::Value &salt_object = salts[i].GetObject();

		if (interval.first == salt_object["begin"].GetUint() && interval.second == salt_object["end"].GetUint())
		{
			// Build an array and move it to the content of the salt.
			rapidjson::Value new_content(rapidjson::kArrayType);

			for (auto &item : salt_table)
			{
				rapidjson::Value salt_item(rapidjson::kObjectType);
				rapidjson::Value count;
				count.SetUint(item.get()->getCount());
				salt_item.AddMember(
					static_cast<rapidjson::GenericStringRef<char>>(item.get()->getSaltName().c_str()),
					count, allocator);

				/*
				 * Why would this StringRef cause no trouble, not just as what will happen above? It is mainly because
				 * std::unique_ptr will explicitly allocate new memory space and there is no overlapping caused, which
				 * is not the case for non-unique-ptr type! Therefore, for instance, if we call
				 *
				 * 		>> obj.AddMember(static_cast<rapidjson::GenericStringRef<char>>(something...), val, allocator),
				 *
				 * and then
				 * 		>> array.PushBack(obj, allocator),
				 *
				 * we shall find that the names of all objects in the array are the same, since they are merely reference
				 * to the same memory space. :P
				 */

				new_content.PushBack(salt_item, allocator);
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
int chooseSalt(std::vector<std::unique_ptr<Salt>> &salts, const double &alpha,
			   const unsigned int &total_salt_used,
			   const unsigned int &ptext_size,
			   const std::pair<unsigned int, unsigned int> &interval,
			   Analysis &a, rapidjson::Document &doc)
{
	/**
	 * If the salt table is empty, return -1 to notify the application to generate a new one.
	 */
	if (salts.empty())
	{
		return -1;
	}
	std::random_shuffle(salts.begin(), salts.end());

	/**
	 * Choose a salt at random and meet some requirements.
	 */
	for (unsigned int i = 0; i < salts.size(); i++)
	{
		if ((double)(salts[i].get()->getCount() * 1.0 / salts.size()) <=
			(double)(alpha * (ptext_size + 1) /*Because new item added*/ / total_salt_used))
		{
			return i;
		}
	}

	return -1; // Not fount.
}

std::pair<unsigned int, unsigned int>
getIntervalForItem(const unsigned int &interval_num,
				   const std::pair<unsigned int, unsigned int> &range,
				   const double &value)
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
	if (true == this->no_loads)
	{
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

	if (true == stale)
	{
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

void SchemaCache::updateStaleness(const std::unique_ptr<Connect> &e_conn,
								  bool staleness)
{
	if (true == staleness)
	{
		// Make everyone stale.
		lowLevelAllStale(e_conn);
	}
	else
	{
		// We are no longer stale.
		this->lowLevelCurrentUnstale(e_conn);
	}
}

bool SchemaCache::initialStaleness(const std::unique_ptr<Connect> &e_conn)
{
	const std::string seed_staleness =
		" INSERT INTO " + MetaData::Table::staleness() +
		"   (cache_id, stale) VALUES " +
		"   (" + std::to_string(this->id) + ", TRUE);";
	RETURN_FALSE_IF_FALSE(e_conn->execute(seed_staleness));

	return true;
}

bool SchemaCache::cleanupStaleness(const std::unique_ptr<Connect> &e_conn)
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

void SchemaCache::lowLevelCurrentStale(const std::unique_ptr<Connect> &e_conn)
{
	lowLevelToggleCurrentStaleness(e_conn, this->id, true);
}

void SchemaCache::lowLevelCurrentUnstale(const std::unique_ptr<Connect> &e_conn)
{
	lowLevelToggleCurrentStaleness(e_conn, this->id, false);
}
