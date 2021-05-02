 CREATE PROCEDURE remote_db.generic_prefix_currentTransactionID
   (OUT out_id VARCHAR(20))
 BEGIN
   SELECT trx_id INTO out_id FROM INFORMATION_SCHEMA.INNODB_TRX
    WHERE INFORMATION_SCHEMA.INNODB_TRX.TRX_MYSQL_THREAD_ID =
          (SELECT CONNECTION_ID())
      AND INFORMATION_SCHEMA.INNODB_TRX.TRX_STATE =
          'RUNNING';
 END

 CREATE PROCEDURE remote_db.generic_prefix_homAdditionTransaction
       (IN delete_query VARBINARY(50000),
        IN insert_query VARBINARY(50000))
 BEGIN
   DECLARE old_transaction_id VARCHAR(20);

   CALL remote_db.generic_prefix_currentTransactionID (old_transaction_id);

   IF old_transaction_id IS NULL THEN
       START TRANSACTION;
   END IF;

   SET @query = delete_query;
   PREPARE dq FROM @query;
   EXECUTE dq;

   SET @query = insert_query;
   PREPARE iq FROM @query;
   EXECUTE iq;

   IF old_transaction_id IS NULL THEN
       COMMIT;

   END IF;
 END

 CREATE PROCEDURE remote_db.generic_prefix_adjustOnion
       (IN completion_id INTEGER,
        IN adjust_query0 VARBINARY(500),
        IN adjust_query1 VARBINARY(500))
 BEGIN
   DECLARE old_transaction_id VARCHAR(20);
   DECLARE b_reissue BOOLEAN;

   CALL remote_db.generic_prefix_currentTransactionID(old_transaction_id);

   IF old_transaction_id IS NULL THEN
       SET b_reissue = TRUE;
   ELSE
       SET b_reissue = FALSE;
   END IF;

   ROLLBACK;

   START TRANSACTION;

   SET @query = adjust_query0;
   PREPARE aq0 FROM @query;
   EXECUTE aq0;

   SET @query = adjust_query1;
   PREPARE aq1 FROM @query;
   EXECUTE aq1;

   INSERT INTO remote_db.generic_prefix_remoteQueryCompletion
      (begin, complete, embedded_completion_id, reissue) VALUES
       (TRUE,  TRUE,     completion_id,          b_reissue);

   COMMIT;
 END

CREATE FUNCTION create_doc RETURNS STRING SONAME 'edb.so';
CREATE FUNCTION cryptdb_decrypt_int_sem RETURNS INTEGER SONAME 'edb.so';
CREATE FUNCTION cryptdb_decrypt_text_sem RETURNS STRING SONAME 'edb.so';
CREATE FUNCTION cryptdb_decrypt_int_det RETURNS INTEGER SONAME 'edb.so';
CREATE FUNCTION cryptdb_decrypt_text_det RETURNS STRING SONAME 'edb.so';
CREATE FUNCTION cryptdb_func_add_set RETURNS STRING SONAME 'edb.so';
CREATE AGGREGATE FUNCTION cryptdb_agg RETURNS STRING SONAME 'edb.so';
CREATE FUNCTION cryptdb_searchSWP RETURNS INTEGER SONAME 'edb.so';
CREATE FUNCTION cryptdb_version RETURNS STRING SONAME 'edb.so';

CREATE FUNCTION FHSearch RETURNS REAL SONAME 'ope.so';
CREATE FUNCTION FHUpdate RETURNS REAL SONAME 'ope.so';
CREATE FUNCTION FHInsert RETURNS REAL SONAME 'ope.so';
CREATE FUNCTION FHStart RETURNS REAL SONAME 'ope.so';
CREATE FUNCTION FHEnd RETURNS REAL SONAME 'ope.so';

-- ! Create a procedure that supports the insert of frequency-hiding order-preserving encryption scheme.
delimiter //
CREATE PROCEDURE pro_insert(IN pos int, IN ct varchar(128), IN table_name varchar(128)) BEGIN
  DECLARE cd DOUBLE DEFAULT 0;
  SET cd = FHInsert(pos, ct); -- Be aware of the function name.
  SET @query = CONCAT_WS(" ", "INSERT INTO", table_name, "VALUES", "(", cd, ", '" , ct, "')");

  PREPARE ex FROM @query;
  EXECUTE ex;

  IF cd < 1 THEN
    SET @query = CONCAT_WS(" ", "UPDATE", table_name, "SET encoding = FHUpdate(ciphertext) WHERE (encoding > FHStart() AND encoding <= FHEnd()) OR (encoding = -1)");
    PREPARE ex FROM @query;
    EXECUTE ex;

  END IF;
END //
delimiter ;