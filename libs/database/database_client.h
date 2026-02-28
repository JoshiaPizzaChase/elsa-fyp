#include <pqxx/pqxx>
#include <string>

namespace database {
class DatabaseClient {
  public:
    DatabaseClient() {
    }

    ~DatabaseClient() {
    }

  private:
    pqxx::connection connection_string;
};
} // namespace database
