#pragma once

#include <string>
#include <system_error>

#include "IWriteBatch.h"
#include "IReadBatch.h"

namespace CryptoNote {

class IDataBase {

public:

  virtual ~IDataBase() {
  }

  virtual std::error_code write(IWriteBatch& batch) = 0;
  virtual std::error_code writeSync(IWriteBatch& batch) = 0;

  virtual std::error_code read(IReadBatch& batch) = 0;
};
} //namespace CryptoNote
