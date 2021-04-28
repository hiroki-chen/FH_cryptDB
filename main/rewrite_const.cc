/*
 * Handlers for rewriting constants, integers, strings, decimals, date, etc. 
 */

#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <set>
#include <list>
#include <algorithm>
#include <stdio.h>
#include <typeinfo>

#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>
#include <main/CryptoHandlers.hh>
#include <util/cryptdb_log.hh>
#include <util/enum_text.hh>
#include <parser/lex_util.hh>
#include "parser/rapidjson/filereadstream.h"
#include "parser/rapidjson/filewritestream.h"

// class/object names we don't care to know the name of
#define ANON                ANON_NAME(__anon_id_const)

// encrypts a constant item based on the information in a
// FIXME: @i should be const ref.
static Item *
encrypt_item(const Item &i, const OLK &olk, Analysis &a)
{
    assert(!RiboldMYSQL::is_null(i));

    // Fetch the database name, table name.

    FieldMeta * const fm = olk.key;
    const std::string db_name = a.getDatabaseName();
    const std::string field_name = fm->fname;
    const std::string& table_name = a.table_name_last_used;

    salt_type IV = 0;

    /**
     * This can be invoked by UPDATE handler.
     */

    if (needFrequencySmoothing(field_name)) {
    	if (!a.update) {
    		std::vector<double> params = a.variables[VariableLocator(db_name, table_name, field_name)];
    		    	/**
    		    	 * If there is no parameter vector, or the size is not 6, for this item, then something must have gone wrong.
    		    	 */
    		assert(8 == params.size());

    		unsigned int interval_num = params[1];
    		unsigned int left = params[4];
    		unsigned int right = params[5];
    		double val;

    		if (Item::Type::INT_ITEM == i.type()) { // Test.
    		    val = RiboldMYSQL::val_uint(i);
    		}

        	MyItem sd = MyItem(db_name, table_name, field_name, RiboldMYSQL::val_real(i));
        	std::cout << "sd: " << sd.getValue() << std::endl;
        	unsigned int pos = a.count_table[sd]++;

        	std::pair<unsigned int, unsigned int> itv =
        			getIntervalForItem(interval_num, std::make_pair(left, right), val);

        	std::vector<std::unique_ptr<Salt>>& salt_table =
        			a.salt_table[Interval(itv.first, itv.second, db_name, table_name, field_name)];
        	std::cout << "size: " << salt_table.size() << std::endl;
        	// TODO: reset count.
        	if (pos + 1 == salt_table.size()) {
        		a.count_table[MyItem(db_name, table_name, field_name, RiboldMYSQL::val_real(i))] = 0;
        	}

        	assert(pos < salt_table.size());

        	IV = stoull(salt_table[pos].get()->getSaltName());
        	std::cout << "IV: " << IV << std::endl;

            OnionMeta * const om = fm->getOnionMeta(olk.o);
            Item * const ret_i = encrypt_item_layers(i, olk.o, *om, a, IV);

            return ret_i;
    	} else {
    		std::string dir = "CryptDB_DATA/";
    		dir.append(db_name + '/');
    		dir.append(table_name + "/");
    		dir.append(field_name + ".json");

    		rapidjson::Document doc;

    		FILE *in_file = fopen(dir.c_str(), "r");
    		char read_buffer[65536];
    		rapidjson::FileReadStream frs(in_file, read_buffer, sizeof(read_buffer));

    		// Parse the file into DOM.
    		doc.ParseStream(frs);

    		std::vector<double> &params = a.variables[VariableLocator(db_name, table_name, field_name)];

    		salt_type IV = 0;
    		if (SECLEVEL::FHDET == olk.l) {
    			IV = stoull(getSalt(params, i, db_name, table_name, field_name, a, doc));
    		}

    		// TODO: write the document back!
    		// encrypt_item_all_onions(i, fm, IV, a, l);
    		assert(writeDomToFile(doc, dir));

    		if (!fm && oPLAIN == olk.o) {
    			return RiboldMYSQL::clone_item(i);
    		}
    		assert(fm);

    		const onion o = olk.o;
            //const auto it = a.salts.find(fm);

            OnionMeta * const om = fm->getOnionMeta(o);
            Item * const ret_i = encrypt_item_layers(i, o, *om, a, IV);

            return ret_i;
    	}
    }
}

static class ANON : public CItemSubtypeIT<Item_string,
                                          Item::Type::STRING_ITEM> {
    virtual RewritePlan *
    do_gather_type(const Item_string &i, Analysis &a) const
    {
        LOG(cdb_v) << " String item do_gather " << i << std::endl;
        const std::string why = "is a string constant";
        reason rsn(FULL_EncSet_Str, why, i);
        return new RewritePlan(FULL_EncSet_Str, rsn);
    }

    virtual Item * do_optimize_type(Item_string *i, Analysis & a) const {
        return i;
    }

    virtual Item *
    do_rewrite_type(const Item_string &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a) const
    {
        LOG(cdb_v) << "do_rewrite_type String item " << i << std::endl;
        return encrypt_item(i, constr, a);
    }

    virtual void
    do_rewrite_insert_type(const Item_string &i, const FieldMeta &fm,
                           Analysis &a, std::vector<Item *> *l) const
    {
        typical_rewrite_insert_type(i, fm, a, l);
    }
} ANON;

static class ANON : public CItemSubtypeIT<Item_int, Item::Type::INT_ITEM> {
    virtual RewritePlan *
    do_gather_type(const Item_int &i, Analysis &a) const
    {
        LOG(cdb_v) << "CItemSubtypeIT (L966) num do_gather " << i
                   << std::endl;
        const std::string why = "is an int constant";
        reason rsn(FULL_EncSet_Int, why, i);
        return new RewritePlan(FULL_EncSet_Int, rsn);
    }

    virtual Item * do_optimize_type(Item_int *i, Analysis & a) const
    {
        return i;
    }

    virtual Item *
    do_rewrite_type(const Item_int &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a) const
    {
        LOG(cdb_v) << "do_rewrite_type " << i << std::endl;

        return encrypt_item(i, constr, a);
    }

    virtual void
    do_rewrite_insert_type(const Item_int &i, const FieldMeta &fm,
                           Analysis &a, std::vector<Item *> *l) const
    {
    	std::cout << RiboldMYSQL::val_uint(i) << std::endl;
        typical_rewrite_insert_type(i, fm, a, l);
    }
} ANON;

static class ANON : public CItemSubtypeIT<Item_decimal,
                                          Item::Type::DECIMAL_ITEM> {
    virtual RewritePlan *
    do_gather_type(const Item_decimal &i, Analysis &a) const
    {
        LOG(cdb_v) << "CItemSubtypeIT decimal do_gather " << i
                   << std::endl;

        const std::string why = "is a decimal constant";
        reason rsn(FULL_EncSet, why, i);
        return new RewritePlan(FULL_EncSet_Int, rsn);
    }

    virtual Item * do_optimize_type(Item_decimal *i, Analysis & a) const
    {
        return i;
    }

    virtual Item *
    do_rewrite_type(const Item_decimal &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a) const
    {
        LOG(cdb_v) << "do_rewrite_type " << i << std::endl;

        return encrypt_item(i, constr, a);
/*        double n = i->val_real();
        char buf[sizeof(double) * 2];
        sprintf(buf, "%x", (unsigned int)n);
        // TODO(stephentu): Do some actual encryption of the double here
        return new Item_hex_string(buf, sizeof(buf));*/
    }

    virtual void
    do_rewrite_insert_type(const Item_decimal &i, const FieldMeta &fm,
                           Analysis &a, std::vector<Item *> *l) const
    {
        typical_rewrite_insert_type(i, fm, a, l);
    }
} ANON;
