from clientSide.client import OpeClient
import pymysql
from data.getData import getData
import datetime

datapath = "data/"
logpath = "log/"


def assert_right(key1, key2, numlist):
    for key in numlist:
        if int(key) < int(key1) or int(key) > int(key2):
            return False
    return True


def initial_ciphertext(cursor):
    # initialize database
    sql = """
    drop table if exists example;
    """
    cursor.execute(sql)

    sql = """
    create table example (encoding double, ciphertext varchar(128));
    """
    cursor.execute(sql)

    #####################################
    sql = """
    drop procedure if exists pro_insert;
    """
    cursor.execute(sql)
    sql = """
    drop function if exists FHInsert;
    """
    cursor.execute(sql)

    sql = """
    drop function if exists FHSearch;
    """
    cursor.execute(sql)

    sql = """
    drop function if exists FHUpdate;
    """
    cursor.execute(sql)

    sql = """
    drop function if exists FHStart;
    """
    cursor.execute(sql)

    sql = """
    drop function if exists FHEnd;
    """
    cursor.execute(sql)

    #####################################
    sql = """
    create function FHInsert RETURNS REAL SONAME 'ope.so';
    """
    cursor.execute(sql)

    sql = """
    create function FHSearch RETURNS REAL SONAME 'ope.so';
    """
    cursor.execute(sql)

    sql = """
    create function FHUpdate RETURNS REAL SONAME 'ope.so';
    """
    cursor.execute(sql)

    sql = """
    create function FHStart RETURNS REAL  SONAME 'ope.so';
    """
    cursor.execute(sql)

    sql = """
    create function FHEnd RETURNS REAL SONAME 'ope.so';
    """
    cursor.execute(sql)

    sql = """
    create procedure pro_insert(IN pos int, IN ct varchar(128)) BEGIN DECLARE i double default 0; SET i = (FHInsert(pos,ct));  insert into example values (i,ct); if i < 0 then update example set encoding = FHUpdate(ciphertext) where (encoding > FHStart() and encoding < FHEnd()) or (encoding = -1); end if;END
    """
    cursor.execute(sql)

    #####################################


def insert_ciphertext():
    opec = OpeClient(None)

    db = pymysql.connect(host="192.168.190.166", user="root",
                         database="db", password="lidongjie", ssl={'ssl': {}})
    cursor = db.cursor()
    attrList = ["year"]
    for num in [100000]:
        for attr in attrList:
            initial_ciphertext(cursor)
            print("attr " + attr)
            log_file = open(logpath + "log_lc.txt", 'a+')
            start_insert = datetime.datetime.now()
            i = 0
            for insertkey in getData(datapath + attr + str(num) + ".txt"):
                i = i+1

                # The client side will generate the position and corresponding ciphertext of a plaintext. Note that this is done on the client side!!
                pos, ciphertext = opec.insert_udf(insertkey)
                
                sql = "call pro_insert(%d,\"%s\");" % (pos, str(ciphertext))
                cursor.execute(sql)
                if i == 100 or i == 1000 or i == 10000 or i == 100000 or i == 1000000 or i == 10000000:
                    print(i)
                    end_insert = datetime.datetime.now()
                    log_file.write(
                        str(i) + " " + attr+" " + str((end_insert - start_insert).total_seconds()) + "\n")
            sql = "select * from example where encoding = -1"
            cursor.execute(sql)
            results = cursor.fetchall()
            for result in results:
                print(result)
            sql = "select count(*) from example where encoding = -2"
            cursor.execute(sql)
            results = cursor.fetchall()
            for result in results:
                print(result)
    log_file.close()
    db.close()


def initial_plaintext(cursor):
    sql = """
    drop table  if exists plaintext;
    """
    cursor.execute(sql)

    sql = """
    create table plaintext (pt varchar(128));
    """
    cursor.execute(sql)


def insert_plaintext():

    opec = OpeClient(None)

    db = pymysql.connect(host="localhost", user="root",
                         database="db", password="lidongjie", port=3306)
    cursor = db.cursor()

    attrList = ["firstname", "lastname"]
    for num in [1000000]:
        for attr in attrList:
            initial_plaintext(cursor)
            print("attr " + attr)
            log_file = open(logpath + "log_plaintext.txt", 'a+')
            start_insert = datetime.datetime.now()
            i = 0
            for insertkey in getData(datapath + attr + str(num) + ".txt"):
                i = i+1
                # print(i)
                # pos, ciphertext = opec.insert(insertkey)
                insertkey = insertkey.replace("'", " ")
                sql = "insert into plaintext values ('%s')" % (insertkey)
                cursor.execute(sql)
                if i == 100 or i == 1000 or i == 10000 or i == 100000 or i == 1000000 or i == 10000000:
                    print(i)
                    end_insert = datetime.datetime.now()
                    log_file.write(
                        str(i) + " " + attr+" " + str((end_insert - start_insert).total_seconds()) + "\n")
    log_file.close()
    db.close()


if __name__ == '__main__':
    # insert_plaintext()
    insert_ciphertext()
