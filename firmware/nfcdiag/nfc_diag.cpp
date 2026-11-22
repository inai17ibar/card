/*
  NFC RF 診断
  ST25R3916 の RF（電波）を出しっぱなしにし、アンテナ端子（RFI）の振幅を 0.5 秒ごとに表示する。
  - 振幅が 0 付近 → 電波が出ていない / アンテナにつながっていない（ハード寄りの問題）
  - Suica を近づけて振幅が大きく変わる場所 → そこがアンテナの位置

  書き込み:  pio run -e nfcdiag -t upload && pio device monitor -e nfcdiag
*/
#include <M5Unified.h>
#include <utility/M5IOE1_Class.hpp>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>
#include <wiring/m5_unit_unified_wiring.hpp>
#include <algorithm>
#include <cstdlib>

namespace cmd = m5::unit::st25r3916::command;
namespace rv  = m5::unit::st25r3916::regval;

namespace {
m5::unit::UnitUnified units;
m5::unit::UnitNFC unit{};
bool ready = false;
uint8_t baseline = 0;

bool measure(uint8_t& amp)
{
    if (!unit.writeDirectCommand(cmd::CMD_MEASURE_AMPLITUDE)) return false;
    delay(2);
    return unit.readADConverterOutput(amp);
}
}  // namespace

void setup()
{
    Serial.begin(115200);
    auto config          = M5.config();
    config.clear_display = false;
    M5.begin(config);

    auto& ioe1 = M5.getIOExpander(0);
    ioe1.setHighImpedance(m5::M5IOE1_Class::gpio4, false);
    ioe1.setDirection(m5::M5IOE1_Class::gpio4, true);
    ioe1.digitalWrite(m5::M5IOE1_Class::gpio4, true);
    delay(10);

    auto c      = unit.config();
    c.mode      = m5::nfc::NFC::A;
    c.using_irq = false;
    unit.config(c);
    ready = m5::unit::wiring::i2cClass(units, unit, M5.In_I2C) && units.begin();
    if (!ready) return;

    // 発振器・受信・送信をすべてオン = RF フィールドを出す
    unit.writeOperationControl(rv::en | rv::rx_en | rv::tx_en);
    delay(20);
    measure(baseline);
}

void loop()
{
    static uint32_t last = 0;
    if (millis() - last < 500) return;
    last = millis();
    if (!ready) {
        Serial.println("NFC init failed");
        return;
    }
    uint8_t amp{}, aux{}, op{};
    measure(amp);
    unit.readAuxiliaryDisplay(aux);
    unit.readOperationControl(op);
    int diff = (int)amp - (int)baseline;
    char bar[41]{};
    int n = std::min(40, std::abs(diff));
    for (int i = 0; i < n; ++i) bar[i] = '#';
    Serial.printf("amp=%3u (base %3u, diff %+4d) tx_on=%d osc_ok=%d op=0x%02X %s\n", amp, baseline, diff,
                  (aux & rv::tx_on) ? 1 : 0, (aux & rv::osc_ok) ? 1 : 0, op, bar);
}
