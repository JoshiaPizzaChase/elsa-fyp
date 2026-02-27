#include <pqxx/pqxx>
#include <string>

namespace database {
class PostgresqlClient {
  public:
    PostgresqlClient() {
    }

    ~PostgresqlClient() {
    }

  private:
    pqxx::connection connection_string;
};

class QuestdbClient {
  public:
    QuestdbClient() {
    }

    ~QuestdbClient() {
    }

  private:

};

} // namespace database
