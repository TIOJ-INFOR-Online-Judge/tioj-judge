#include "database.h"

void Database::Init() {
  if (!db_) db_ = std::make_unique<Storage>(InitStorage());
}

void Database::UpdateTd(const std::vector<Testdata>& td) {
  Init();
  db_->replace_range(td.begin(), td.end());
}
