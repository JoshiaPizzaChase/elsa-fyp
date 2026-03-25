#!/bin/bash

POSTGRESQL_DATABASE_NAME=edux_core_db
INIT_POSTGRESQL_SCRIPT_PATH="$(dirname $0)/db_scripts/init_postgresql.sql"

INIT_QUESTDB_SCRIPT_PATH="$(dirname $0)/db_scripts/init_questdb.sql"

# drop the database if it exists to ensure a clean state
echo "Dropping PostgreSQL database if it exists..."
dropdb --if-exists $POSTGRESQL_DATABASE_NAME

# call the init_postgresql.sql
echo "Creating PostgreSQL database..."
createdb $POSTGRESQL_DATABASE_NAME

psql -d $POSTGRESQL_DATABASE_NAME -f $INIT_POSTGRESQL_SCRIPT_PATH

echo "Successfully ran postgresql init script."

# call the init_questdb.sql
questdb start

echo "Dropping QuestDB tables if they exist..."
PGPASSWORD="quest" psql -h localhost -p 8812 -U admin -d qdb -c "DROP TABLE IF EXISTS orders;"
PGPASSWORD="quest" psql -h localhost -p 8812 -U admin -d qdb -c "DROP TABLE IF EXISTS trades;"

PGPASSWORD="quest" psql -h localhost -p 8812 -U admin -d qdb -f $INIT_QUESTDB_SCRIPT_PATH

echo "Successfully ran questdb init script."
