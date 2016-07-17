#include "mpx2bits.h"

#include <complex>
#include <deque>

#include "liquid_wrappers.h"

#define FS        228000.0f
#define FC_0      57000.0f
#define IBUFLEN   4096
#define OBUFLEN   128
#define BITBUFLEN 1024

#define PI_f      3.1415926535898f
#define PI_2_f    1.5707963267949f

namespace redsea {

namespace {

  int sign(float x) {
    return (x >= 0);
  }
}

float phaseDiff(std::complex<float> a, std::complex<float> b) {
  float dph = arg(b) - arg(a);
  if (dph > M_PI)
    dph -= 2*M_PI;
  if (dph < -M_PI)
    dph += 2*M_PI;
  dph = fabs(dph) - M_PI_2;
  return dph;
}

RunningSum::RunningSum(int len) : values_(len), len_(len), i_(0), max_i_(0),
  last_max_i_(0) {
}

RunningSum::~RunningSum() {
}

float RunningSum::pushAndRead(float s) {
  sum_ += s;
  sum_ -= values_[i_];
  values_[i_] = s;

  if (fabs(sum_) > max_sum_) {
    max_i_ = i_;
    max_sum_ = fabs(sum_);
  }

  i_ = (i_ + 1) % len_;

  if (i_ == max_i_) {
    last_max_i_ = max_i_;
    max_sum_ = sum_;
    max_i_ = i_;
  }

  return sum_;
}

int RunningSum::lastMaxIndex() const {
  return last_max_i_;
}

DPSK::DPSK() : subcarr_freq_(FC_0), gain_(1.0f),
  counter_(0), tot_errs_(2), reading_frame_(0), bit_buffer_(),
  fir_lpf_(511, 2100.0f / FS),
  fir_phase_(63, 1500.0 / FS * 12),
  is_eof_(false),
  agc_(0.001f),
  nco_if_(FC_0 * 2 * PI_f / FS),
  nco_doublerate_(1187.5 * 2 * 2 * PI_f / FS),
  ph0_(0.0f), sym_delay_(wdelaycf_create(17)),
  clock_shift_(0), clock_phase_(0), last_rising_at_(0), lastbit_(0),
  running_sum_(16), symsync_(symsync_crcf_create_rnyquist(LIQUID_FIRFILT_RRC, 2,
        5, 0.5f, 32)),
  prev_dphc_(0.0f)
  {

    symsync_crcf_set_lf_bw(symsync_, 0.02f);
    symsync_crcf_set_output_rate(symsync_,2);

}

DPSK::~DPSK() {
}

void DPSK::demodulateMoreBits() {

  int16_t sample[IBUFLEN];
  int samplesread = fread(sample, sizeof(int16_t), IBUFLEN, stdin);
  if (samplesread < IBUFLEN) {
    is_eof_ = true;
    return;
  }

  for (int i = 0; i < samplesread; i++) {

    std::complex<float> sample_down = nco_if_.mixDown(sample[i]);

    fir_lpf_.push(sample_down);
    std::complex<float> sample_shaped_unnorm = fir_lpf_.execute();

    std::complex<float> sample_shaped = agc_.execute(sample_shaped_unnorm);

    std::complex<double> sq = std::pow(sample_shaped, 2);
    printf("pe:%.10f,%.10f\n",real(sq),imag(sq));

    if (numsamples_ % 12 == 0) {

      std::complex<float> sym0;
      wdelaycf_push(sym_delay_, sample_shaped);
      wdelaycf_read(sym_delay_, &sym0);
      float dph = phaseDiff(sample_shaped, sym0);
      std::complex<float> dphc(dph,0),dphc_lpf;

      //printf("pe:%f,%f\n",real(sq)*1000,imag(sq)*1000);

      fir_phase_.push(dphc);
      dphc_lpf = fir_phase_.execute();

      //printf("pe:%f,%f\n",real(dphc),imag(dphc));

      if (sign(real(dphc_lpf)) != sign(real(prev_dphc_))) {
        float fractional_zc = clock_phase_ - 1 +
          (-real(prev_dphc_)) / (real(dphc_lpf) - real(prev_dphc_));
        //printf("zc:%d,%f\n",numsamples_,fractional_zc);
        clock_shift_ += (fractional_zc > 7.0f ? -1 : 1) * 0.00003;

      }

      prev_dphc_ = dphc_lpf;

      float bval = running_sum_.pushAndRead(real(dphc_lpf));

      if (clock_phase_ == 0) {
        unsigned bit = sign(bval);
        bit_buffer_.push_back(bit);
        //printf("lmi:%d\n",running_sum_.lastMaxIndex());
      }
      //printf("g:%d,%f\n",clock_phase_,fabs(bval));

      clock_phase_ = int(clock_phase_ + 1 + clock_shift_ + .5) % 16;

    }

    nco_if_.step();

    numsamples_ ++;

  }

}

int DPSK::getNextBit() {
  while (bit_buffer_.size() < 1 && !isEOF())
    demodulateMoreBits();

  int bit = 0;

  if (bit_buffer_.size() > 0) {
    bit = bit_buffer_.front();
    bit_buffer_.pop_front();
  }

  return bit;
}

bool DPSK::isEOF() const {
  return is_eof_;
}

AsciiBits::AsciiBits() : is_eof_(false) {

}

AsciiBits::~AsciiBits() {

}

int AsciiBits::getNextBit() {
  int result = 0;
  while (result != '0' && result != '1' && result != EOF)
    result = getchar();

  if (result == EOF) {
    is_eof_ = true;
    return 0;
  }

  return (result == '1');

}

bool AsciiBits::isEOF() const {
  return is_eof_;
}

} // namespace redsea
