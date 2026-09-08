#pragma once
#include "Provisioning.h"
namespace mindflayer {
namespace provisioning {
constexpr size_t RECORD_HEADER_SIZE = 11, RECORD_CRC_SIZE = 4, RECORD_IO_SIZE = 1040;
constexpr uint8_t RECORD_VERSION = 1;
enum Copy : uint8_t { COPY_A = 0, COPY_B = 1, NO_COPY = 255 };
struct Selection {
  Copy copy;
  uint32_t generation;
};
class FlashBackend {
public:
  virtual ~FlashBackend() = default;
  virtual bool read(uint32_t, void*, size_t) = 0;
  virtual bool eraseSector(uint32_t) = 0;
  virtual bool write(uint32_t, const void*, size_t) = 0;
};
class ProvisioningStore {
public:
  explicit ProvisioningStore(FlashBackend& backend) : backend_(backend) {}
  bool load(Provisioning&, Selection* = nullptr);
  bool writeEnvelope(const uint8_t*, size_t, Selection* = nullptr);
  bool inspect(Copy, Provisioning&, uint32_t&);

private:
  FlashBackend& backend_;
  bool readRecord(Copy, Provisioning&, uint32_t&, bool = true);
};
bool generationNewer(uint32_t, uint32_t);
#ifdef ARDUINO
bool loadStored(Provisioning&, Selection* = nullptr);
bool storeAtomically(const uint8_t*, size_t, Selection* = nullptr);
#endif
} // namespace provisioning
} // namespace mindflayer
