#ifndef BLOCKSTREAM_H_
#define BLOCKSTREAM_H_

#include <map>

#include "mpx2bits.h"

namespace redsea {

enum {
  A, B, C, CI, D
};

enum eInputType {
  INPUT_MPX, INPUT_ASCIIBITS
};

class BlockStream {
  public:
  BlockStream(int input_type=INPUT_MPX);
  std::vector<uint16_t> getNextGroup();
  bool isEOF() const;

  private:
  int getNextBit();
  void uncorrectable();
  unsigned bitcount_;
  unsigned prevbitcount_;
  unsigned left_to_read_;
  uint32_t wideblock_;
  unsigned prevsync_;
  unsigned block_counter_;
  unsigned expected_offset_;
  uint16_t pi_;
  std::vector<bool> has_sync_for_;
  bool is_in_sync_;
  std::vector<uint16_t> offset_word_;
  std::vector<int> block_for_offset_;
  std::vector<uint16_t> group_data_;
  std::vector<bool> has_block_;
  std::vector<bool> block_has_errors_;
  Subcarrier subcarrier_;
  AsciiBits ascii_bits_;
  bool has_whole_group_;
  std::map<uint16_t,uint16_t> error_lookup_;
  unsigned data_length_;
  const int input_type_;

};

} // namespace redsea
#endif // BLOCKSTREAM_H_
