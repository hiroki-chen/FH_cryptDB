# FH_cryptDB
We have implemented frequency smoothing for deterministic or order-preserving encryption methods for CryptDB.
These following features are included in our model:
1. Freqeuncy smoothing algorithms based on pseudorandom function(s);
2. SQL Rewriter for `where` for *fhs*-encrypted columns:
  For example, if a column whose name is `fh_id` of integer type is encrypted by frequency smoothing method, and there is a SQL query `SELECT * FRROM abc WHERE fh_id = 123`, then the query would probably be rewritten to `SELECT * FROM anon_abc WHERE anon_fh_id = FH_AES(123, 1) OR anon_fh_id = FH_AES(123, 2) OR anon_fh_id = FH_AES(123, 3)`.
3. Improved Order-Preserving Encryption Scheme for *fh*-encrypted columns.

P.S. If you want to compile the project, be sure that you use GCC whose version is higher than 4.7 or so and be sure it supports -std=c++11. We strongly recommend that -std=c++0x should be abandoned because it cannot support static initialization and some features in modern C++ language.

Hint: In order to work properly, you should compile `ope.cpp` manually. Shell codes:
```shellcode
g++ -c -o -std=c++11 -fPIC -Wall ope.cpp ope.o;
g++ -shared -o ope.so ope.o
```
Then add `ope.so` to your MySQL plugin directory: `/usr/lib/mysql/plugins/*.so`.
Next, create all the functions with soname:
```sql
CREATE FUNCTION XXX RETURNS REAL SONAME 'ope.so';
```
step by step.

**Important: FH_cryptDB is non-commercial, nor deployable on any produtive platforms and systems. This is just a prototype for research and study. Any warranty for security, robustness, or any other things is not provided. Anyone or any entity who uses the prototype will be acknowledges as one who already read the notice here and if any lethal error happens, the author is free of any responsibility.**

CryptDB's website (including latest source code): http://css.csail.mit.edu/cryptdb

Wiki: https://github.com/burrows-labs/cryptdb-wiki/wiki/_pages

Convention : When this document uses syntax like
                single-quote text single-quote
                + 'some-text-here'
                + 'a second example'
             it indicates that the reader is to use the value _without_
             the single-quotes.
	     
#########################################
#########################################
########### Getting started   ###########
#########################################
#########################################
    * Requirements:
      > Ubuntu 12.04, 13.04; Not tested on a different OS.
      > ruby
    * Build CryptDB using the installation script.
      > note, this script will stop and start your mysql instance.
      > scripts/install.rb <path-to-cryptdb>
        + <path-to-cryptdb> should just be '.' if you are in the cryptdb
          directory
        + the script will likely require root privileges in order to
          install dependencies and UDFs.
    * Username/Password
        By default cryptdb uses 'root' for the username and 'letmein'
        for the password. You can change this by modifying the source.
            + Tests: test/test_utils.hh
            + Shell: main/cdb_test.cc
            + Proxy: mysqlproxy/wrapper.lua
    * Rebuildling CryptDB
        If you modify the source and want to rebuild, issue 'make' in the
        cryptdb directory.  If you change the UDFs you will also need to do
        'make install' (which will likely require root privilege).

#########################################
#########################################
####### A few ways to run CryptDB #######
#########################################
#########################################
    Set (or place in your .bashrc):
    > export EDBDIR=/full/path/to/cryptdb/	

    I. Shell
        > To Start: obj/main/cdb_test  ./shadow <some-database-name>
        > Type SQL queries that will be encrypted.

    II. Proxy
        A) To Start: 
             > /path/to/cryptdb/bins/proxy-bin/bin/mysql-proxy         \
                         --plugins=proxy --event-threads=4             \
                         --max-open-files=1024                         \
                         --proxy-lua-script=$EDBDIR/mysqlproxy/wrapper.lua \
                         --proxy-address=127.0.0.1:3307                \
                         --proxy-backend-addresses=localhost:3306

        B) Connect to CryptDB: (where root/letmein are username/password)
           mysql -u root -pletmein -h 127.0.0.1 -P 3307
        C) CREATE a database; USE it; Then type queries that will execute
           on encrypted data at the DB server!

    III. Tests
        > To Start:
            obj/test/test queries plain proxy-single


#########################################
#########################################
############ Failing queries ############
#########################################
#########################################

	    
    Note that CryptDB is not a product, but just a more advanced research
    prototype. It only has implemented a subset of SQL queries. For example,
    it supports the regular MySQL client and a variety of queries you can
    play with from this shell, Wordpress, and other apps. We did not test
    it with other MySQL clients (e.g., PHP, Java) and these likely issue
    some uncommon metadata query we did not implement. Feel free to
    implement what's missing!


    > Queries can fail for a few reasons.
      I) CryptDB can support the query, but we have not yet implemented it.
         This should throw an UNIMPLEMENTED exception.
      II) CryptDB can not support this type of query, likely for
          cryptographic reasons. The error should include a message telling
          you more about the issue.

            For example if you issue this query to the shell.
            SELECT * FROM t WHERE x + 10 < 50

            You should get feedback. You will see that the feedback
            tells you the desired security level for the operation at
            each onion, and the current security level of each argument
            for each onion.

            In the case of this query, you can see that it would like
            to compare order with onion 1 (OPE), 4 (PLAIN) or 6 (WAIT).

            But if you look at it's first child "additive", you can see
            that it would like to use the HOM onion.  Because the HOM
            onion is not supported by the order operation, the query
            fails and gives you this error message.

            Look to the CryptDB paper for more information regarding
            how MySQL operations correspond to the cryptographic onions.
      III) State corruption (ie a bug in our code), this will likely lead
           to a crash.
      IV) Invalid query; problem with the query syntax.

#########################################
#########################################
################ Misc ###################
#########################################
#########################################

    > The benchmarks in the CryptDB paper are based on an older version of
      CryptDB. We have added new features and functionality to the current
      version which we have not yet optimized (but it is part of our
      roadmap).
    > For example queries, take a look at our tests test/TestQueries.cc
    > Consider connecting directly to MySQL to see what is in the raw
      database.
    > Resetting state (e.g., after running tests)
        > Take a look at scripts/refresh.sh ; you probably want that.
        > If you create a database in cryptdb and then reset
          the state without first dropping the database from _within_
          cryptdb; you will need to drop it from the regular mysql client.
    > Modifying the UDFs.
        + If you make changes to edb/udf.cc, you must reinstall the UDFs
          before your changes will take affect.
            > sudo service mysql stop
            > sudo make install
            > sudo service mysql start
    > This new version of CryptDB is only single principal. We are in the
      process of developling a new platform that more affectively addresses
      multiple principals.
Needed to be implemented...
