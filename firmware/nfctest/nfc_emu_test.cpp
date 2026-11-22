/*
  NFC エミュレーション 最小テスト（切り分け用）
  公式 PaperMono NFC の Tag Emulation サンプルと同じ手順・同じ MIFARE Ultralight 64 バイトで、
  名刺 URL を返す。画面描画・タッチ・スリープ等は一切しない。

  書き込み:  pio run -e nfctest -t upload && pio device monitor -e nfctest
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>
#include <wiring/m5_unit_unified_wiring.hpp>
#include <cstring>

#include "config.h"

using namespace m5::nfc;
using namespace m5::nfc::a;

namespace {
const char* g_status = "not started";
m5::unit::UnitUnified units;
m5::unit::UnitNFC nfc{};
EmulationLayerA emulation{nfc};
PICC card{};

constexpr uint8_t kUid[7] = {0x04, 0x50, 0x41, 0x50, 0x45, 0x52, 0x01};  // 公式サンプルと同じ UID
uint8_t cardMemory[64]{};

uint8_t bcc8(const uint8_t* p, uint8_t len, uint8_t v = 0)
{
    for (uint8_t i = 0; i < len; ++i) v ^= p[i];
    return v;
}

void build_memory()
{
    std::memcpy(cardMemory, kUid, 3);
    cardMemory[3] = bcc8(kUid, 3, 0x88);
    std::memcpy(cardMemory + 4, kUid + 3, 4);
    cardMemory[8] = bcc8(kUid + 3, 4);
    // page 3: CC（NDEF 1.0, データ 48 バイト, 読み書き可）
    const uint8_t cc[4] = {0xE1, 0x10, 0x06, 0x00};
    std::memcpy(cardMemory + 12, cc, 4);
    // page 4-: NDEF URI
    const char* url  = CARD_URL_WITHOUT_SCHEME;
    const size_t ul  = std::strlen(url);
    uint8_t* p       = cardMemory + 16;
    *p++ = 0x03; *p++ = static_cast<uint8_t>(4 + 1 + ul);
    *p++ = 0xD1; *p++ = 0x01; *p++ = static_cast<uint8_t>(1 + ul); *p++ = 'U'; *p++ = 0x04;
    std::memcpy(p, url, ul); p += ul;
    *p = 0xFE;
}

const char* state_name(EmulationLayerA::State s)
{
    static const char* names[] = {"None", "Off", "Idle", "Ready", "Active", "Halt"};
    return names[static_cast<int>(s)];
}
}  // namespace

void setup()
{
    Serial.begin(115200);
    delay(100);
    auto config          = M5.config();
    config.clear_display = false;
    config.internal_rtc  = false;
    config.internal_imu  = false;
    config.internal_mic  = false;
    M5.begin(config);

    auto c      = nfc.config();
    c.emulation = true;
    c.mode      = NFC::A;
    c.using_irq = true;
    c.irq       = 6;
    nfc.config(c);

    if (!m5::unit::wiring::i2cClass(units, nfc, M5.In_I2C) || !units.begin()) {
        g_status = "NFC init failed";
        return;
    }
    build_memory();
    if (card.emulate(Type::MIFARE_Ultralight, kUid, 7) && emulation.begin(card, cardMemory, sizeof(cardMemory))) {
        g_status = "EMULATION READY";
    } else {
        g_status = "EMULATION FAILED";
    }
}

void loop()
{
    // NFC-A の応答タイミングは厳しいので、ループは短く保つ（公式サンプルのコメントより）
    units.update();
    emulation.update();
    static auto last = EmulationLayerA::State::None;
    auto s           = emulation.state();
    if (s != last) {
        last = s;
        Serial.printf("NFC-A state: %s\n", state_name(s));
    }
    // 起動直後のログはモニタ接続前に流れてしまうので、3 秒ごとに状態を出す
    static uint32_t last_print = 0;
    if (millis() - last_print > 3000) {
        last_print = millis();
        Serial.printf("[status] %s / NFC-A state: %s\n", g_status, state_name(s));
    }
    yield();
}
