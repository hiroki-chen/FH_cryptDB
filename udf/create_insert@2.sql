CREATE FUNCTION create_doc RETURNS STRING SONAME 'edb.so';
CREATE FUNCTION cryptdb_decrypt_int_sem RETURNS INTEGER SONAME 'edb.so';
CREATE FUNCTION cryptdb_decrypt_text_sem RETURNS STRING SONAME 'edb.so';
CREATE FUNCTION cryptdb_decrypt_int_det RETURNS INTEGER SONAME 'edb.so';
CREATE FUNCTION cryptdb_decrypt_text_det RETURNS STRING SONAME 'edb.so';
CREATE FUNCTION cryptdb_func_add_set RETURNS STRING SONAME 'edb.so';
CREATE AGGREGATE FUNCTION cryptdb_agg RETURNS STRING SONAME 'edb.so';
CREATE FUNCTION cryptdb_searchSWP RETURNS INTEGER SONAME 'edb.so';
CREATE FUNCTION cryptdb_version RETURNS STRING SONAME 'edb.so';

DROP FUNCTION FHSearch;
DROP FUNCTION FHUpdate;
DROP FUNCTION FHInsert;
DROP FUNCTION FHStart;
DROP FUNCTION FHEnd;
CREATE FUNCTION FHSearch RETURNS REAL SONAME 'ope.so';
CREATE FUNCTION FHUpdate RETURNS REAL SONAME 'ope.so';
CREATE FUNCTION FHInsert RETURNS REAL SONAME 'ope.so';
CREATE FUNCTION FHStart RETURNS REAL SONAME 'ope.so';
CREATE FUNCTION FHEnd RETURNS REAL SONAME 'ope.so';

-- ! Create a procedure that supports the insert of frequency-hiding order-preserving encryption scheme.
DROP PROCEDURE pro_insert;
delimiter //
CREATE PROCEDURE pro_insert(IN pos int, IN ct varchar(128),
                            IN table_name varchar(128), encoding_column_name varchar(128), OPE_column_name varchar(128), identifier varchar(128))
BEGIN
  DECLARE cd DOUBLE DEFAULT 0;
  SET cd = FHInsert(pos, ct); -- Be aware of the function name.
  SET @query = CONCAT_WS("", "UPDATE ", table_name, " SET ", encoding_column_name, " = ", cd, " WHERE ", encoding_column_name, " = '", identifier, "'");

  PREPARE ex FROM @query;
  EXECUTE ex;

  IF cd < 1 THEN
    SET @query = CONCAT_WS(" ", "UPDATE", table_name, "SET", encoding_column_name, "= FHUpdate(", OPE_column_name, ")", "WHERE (", encoding_column_name, "> FHStart() AND", encoding_column_name, "<= FHEnd()) OR (", encoding_column_name, "= -1)");
    PREPARE ex FROM @query;
    EXECUTE ex;

  END IF;
END //
delimiter ;