#ifndef DATABASE_H_
#define DATABASE_H_

#include <sqlite_orm/sqlite_orm.h>
#include "tioj/utils.h"
#include "paths.h"

// TODO FEATURE(web-refactor): add td limits
struct Testdata {
  int testdata_id;
  int problem_id;
  int order;
  long timestamp;
  bool input_compressed;
  bool output_compressed;
};

namespace {

fs::path DatabasePath() {
  return kTestdataRoot / "db.sqlite";
}

inline auto InitStorage() {
  using namespace sqlite_orm;
  auto storage = make_storage(DatabasePath(),
      make_index("idx_testdata_problem_order", &Testdata::problem_id, &Testdata::order),
      make_table("testdata",
                 make_column("testdata_id", &Testdata::testdata_id, primary_key()),
                 make_column("problem_id", &Testdata::problem_id),
                 make_column("order", &Testdata::order),
                 make_column("timestamp", &Testdata::timestamp),
                 make_column("input_compressed", &Testdata::input_compressed, default_value(false)),
                 make_column("output_compressed", &Testdata::output_compressed, default_value(false))));
  storage.sync_schema(true);
  return storage;
}

} // namespace

class Database {
 public:
  using Storage = decltype(InitStorage());

 private:
  std::unique_ptr<Storage> db_;

 public:
  void Init();

  auto ProblemTd(int problem_id) {
    using namespace sqlite_orm;
    Init();
    return db_->iterate<Testdata>(where(c(&Testdata::problem_id) == problem_id));
  }

  void UpdateTd(const std::vector<Testdata>& td);
};

#endif  // DATABASE_H_
