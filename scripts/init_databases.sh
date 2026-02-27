#!/bin/bash

POSTGRESQL_DATABASE_NAME=edux_core_db
INIT_POSTGRESQL_SCRIPT_PATH="$(dirname $0)/db_scripts/init_postgresql.sql"

INIT_QUESTDB_SCRIPT_PATH="$(dirname $0)/db_scripts/init_questdb.sql"

# call the init_postgresql.sql
echo "Error may be shown if database is created already."
createdb $POSTGRESQL_DATABASE_NAME 

psql -d $POSTGRESQL_DATABASE_NAME -f $INIT_POSTGRESQL_SCRIPT_PATH

echo "Successfully ran postgresql init script."

# call the init_questdb.sql
questdb start

PGPASSWORD="quest" psql -h localhost -p 8812 -U admin -d qdb -f $INIT_QUESTDB_SCRIPT_PATH

echo "Successfully ran questdb init script."
