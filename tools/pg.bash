#!/usr/bin/env bash
export PGDATA=`mktemp -d`
pg_ctl initdb -s -o -Atrust
echo "unix_socket_directories = '$PGDATA'" >> $PGDATA/postgresql.conf
echo "listen_addresses = ''" >> $PGDATA/postgresql.conf
#echo "logging_collector = true" >> $PGDATA/postgresql.conf
#echo "log_directory = '$PGDATA'" >> $PGDATA/postgresql.conf
#echo "log_statement = 'all'" >> $PGDATA/postgresql.conf
pg_ctl -w start -s -l/dev/null
createuser -h $PGDATA -s $1
echo $PGDATA
