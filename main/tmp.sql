create procedure MyAvg(prefix varchar(5))
begin
    declare resNumber int;
    declare i int;
    declare tmpAvg real;
    declare Qcolumn varchar(20);
    create table if not exists result(
        QNO varchar(20),
        average real
    );
    select count(*) into resNumber from information_schema.COLUMNS where TABLE_NAME = 'Score' and COLUMN_NAME like 'A%';

    set i = 1;
    while (i <= resNumber)
    do
        set tmpAvg = 0;
        set Qcolumn = prefix + i;
        select Avg(Qcolumn) into tmpAvg from yii_aoyamahiroki.Score;
        insert into result values(Qcolumn, tmpAvg);
        set i = i + 1;
    end while;
end;