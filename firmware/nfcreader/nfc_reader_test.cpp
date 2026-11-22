/*
  NFC アンテナ確認テスト（リーダーモード）
  PaperMono を「読む側」にして、Suica / PASMO などの交通系 IC カード（FeliCa）を検出する。
  これで読めればアンテナとチップは正常 → 問題はエミュレーション側に絞れる。

  書き込み:  pio run -e nfcreader -t upload && pio device monitor -e nfcreader
*/
#include <M5Unified.h>
#include <utility/M5IOE1_Class.hpp>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>
#include <wiring/m5_unit_unified_wiring.hpp>
#include <vector>

using namespace m5::nfc;
using namespace m5::nfc::f;

namespace {
m5::unit::UnitUnified units;
m5::unit::UnitNFC unit{};
NFCLayerF nfc_f{unit};
const char* g_status = "not started";
uint32_t g_found     = 0;
}  // namespace

void setup()
{
    Serial.begin(115200);
    auto config          = M5.config();
    config.clear_display = false;
    M5.begin(config);

    // PYB_NFC_EN（M5IOE1 GPIO4）を High にして NFC を有効化
    auto& ioe1 = M5.getIOExpander(0);
    ioe1.setHighImpedance(m5::M5IOE1_Class::gpio4, false);
    ioe1.setDirection(m5::M5IOE1_Class::gpio4, true);
    ioe1.digitalWrite(m5::M5IOE1_Class::gpio4, true);
    delay(10);

    auto c      = unit.config();
    c.mode      = NFC::F;
    c.using_irq = true;
    c.irq       = 6;
    unit.config(c);

    g_status = (m5::unit::wiring::i2cClass(units, unit, M5.In_I2C) && units.begin()) ? "READER READY" : "NFC init failed";
}

void loop()
{
    units.update();
    std::vector<PICC> piccs;
    if (nfc_f.detect(piccs)) {
        for (auto&& p : piccs) {
            ++g_found;
            Serial.printf(">>> FOUND FeliCa IDm:%s type:%s\n", p.idmAsString().c_str(), p.typeAsString().c_str());
        }
        M5.Speaker.tone(3000, 30);
        delay(300);
    }
    static uint32_t last = 0;
    if (millis() - last > 3000) {
        last = millis();
        Serial.printf("[status] %s / found so far: %lu\n", g_status, (unsigned long)g_found);
    }
}
