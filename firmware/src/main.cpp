/*
  PaperMono 電子名刺
  - 画面: 名刺画像（QR 入り）を表示。e-paper なので電源を切っても残る
  - NFC : NTAG213 をエミュレートし、名刺ページの URL を NDEF で返す
  - 省電力: AWAKE_MS 経過でディープスリープ。画面タッチで復帰

  NFC 部分は M5Unit-NFC の examples/UnitUnified/NFCA/Emulation を元にしている。
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>
#include <cstring>

#include "card_png.h"
#include "config.h"

using namespace m5::nfc;
using namespace m5::nfc::a;
using namespace m5::nfc::a::mifare;

namespace {

auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
m5::unit::UnitNFC nfc_unit{};  // 内蔵 ST25R3916（内部 I2C, 0x50）
EmulationLayerA emu_a{nfc_unit};
PICC picc{};

constexpr uint8_t uid[7] = CARD_UID;

// NTAG213: 45 ページ × 4 バイト
constexpr size_t NTAG213_PAGES = 45;
uint8_t picc_memory[NTAG213_PAGES * 4]{};

// 画面の再描画が要るかを、ディープスリープをまたいで覚えておく
RTC_DATA_ATTR uint32_t drawn_png_len = 0;

uint8_t bcc8(const uint8_t* p, uint8_t len, uint8_t init = 0)
{
    for (uint8_t i = 0; i < len; ++i) init ^= p[i];
    return init;
}

// NTAG213 のメモリイメージを組み立てる（UID / CC / NDEF URI / 設定ページ）
bool build_ntag213_memory()
{
    std::memset(picc_memory, 0, sizeof(picc_memory));

    // page 0-2: UID と BCC
    std::memcpy(picc_memory, uid, 3);
    picc_memory[3] = bcc8(uid, 3, 0x88 /* CT */);
    std::memcpy(picc_memory + 4, uid + 3, 4);
    picc_memory[8] = bcc8(uid + 3, 4);
    picc_memory[9] = 0x48;  // internal

    // page 3: Capability Container（NDEF 1.0, データ領域 144 バイト, 読み取り専用）
    const uint8_t cc[4] = {0xE1, 0x10, 0x12, 0x0F};
    std::memcpy(picc_memory + 12, cc, 4);

    // page 4-: NDEF メッセージ TLV（URI レコード, 0x04 = "https://"）
    const char* url    = CARD_URL_WITHOUT_SCHEME;
    const size_t ulen  = std::strlen(url);
    const size_t plen  = 1 + ulen;  // URI 識別コード + 本体
    const size_t rlen  = 4 + plen;  // レコードヘッダ 4 バイト + ペイロード
    if (rlen > 0xFE || 2 + rlen + 1 > 144) return false;  // 1 バイト長 TLV に収まる範囲のみ

    uint8_t* p = picc_memory + 16;
    *p++ = 0x03;                       // NDEF Message TLV
    *p++ = static_cast<uint8_t>(rlen);
    *p++ = 0xD1;                       // MB|ME|SR, TNF=Well-known
    *p++ = 0x01;                       // type length
    *p++ = static_cast<uint8_t>(plen); // payload length
    *p++ = 'U';                        // type: URI
    *p++ = 0x04;                       // "https://"
    std::memcpy(p, url, ulen);
    p += ulen;
    *p = 0xFE;                         // Terminator TLV

    // page 40-44: 動的ロック / CFG0 / CFG1 / PWD / PACK（公式サンプルと同じ初期値）
    const uint8_t tail[20] = {0x00, 0x00, 0x00, 0xBD,  // page 40
                              0x02, 0x00, 0x00, 0xFF,  // page 41 CFG0 (AUTH0=0xFF: 保護なし)
                              0x00, 0x00, 0x00, 0x00,  // page 42 CFG1
                              0x00, 0x00, 0x00, 0x00,  // page 43 PWD
                              0x00, 0x00, 0x00, 0x00}; // page 44 PACK
    std::memcpy(picc_memory + 40 * 4, tail, sizeof(tail));
    return true;
}

void draw_card(bool force)
{
    // スリープ復帰時は画面がそのまま残っているので描き直さない（パネル寿命のため）
    if (!force && drawn_png_len == CARD_PNG_LEN) return;
    lcd.setRotation(1);  // 横向き 800x480
    lcd.setEpdMode(epd_mode_t::epd_quality);
    lcd.startWrite();
    lcd.fillScreen(TFT_WHITE);
    lcd.drawPng(CARD_PNG, CARD_PNG_LEN, 0, 0);
    lcd.endWrite();
    lcd.waitDisplay();
    drawn_png_len = CARD_PNG_LEN;
}

bool start_nfc()
{
    auto cfg      = nfc_unit.config();
    cfg.emulation = true;
    cfg.mode      = NFC::A;
    nfc_unit.config(cfg);

    if (!Units.add(nfc_unit, M5.In_I2C)) {
        M5_LOGE("NFC: Units.add failed (I2C 0x50 not found?)");
        return false;
    }
    if (!Units.begin()) {
        M5_LOGE("NFC: Units.begin failed");
        return false;
    }
    if (!build_ntag213_memory()) {
        M5_LOGE("NFC: URL too long for NTAG213");
        return false;
    }
    if (!picc.emulate(Type::NTAG_213, uid, sizeof(uid))) {
        M5_LOGE("NFC: picc.emulate failed");
        return false;
    }
    if (!emu_a.begin(picc, picc_memory, sizeof(picc_memory))) {
        M5_LOGE("NFC: emu_a.begin failed");
        return false;
    }

    M5.Speaker.tone(2000, 80);  // NFC 準備完了の合図
    const auto& e = emu_a.emulatePICC();
    M5_LOGI("Emulating %s UID:%s", e.typeAsString().c_str(), e.uidAsString().c_str());
    return true;
}

void go_to_sleep()
{
    M5_LOGI("deep sleep");
    lcd.waitDisplay();
    // 第1引数 0 は「スリープしない」の意味なので、タイマーなし＋タッチ復帰を明示
    M5.Power.deepSleep(m5::Power_Class::sleep_no_timer, true);
}

uint32_t awake_since = 0;

}  // namespace

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);

    // 本体の RGB LED を消灯（赤は PMIC 経由で起動時に点くことがある）
    M5.Led.setAllColor(0, 0, 0);
    M5.Led.display();

    // I2C 上のデバイスを列挙（NFC の ST25R3916 は 0x50 にいるはず）
    for (uint8_t a = 1; a < 0x7F; ++a) {
        if (M5.In_I2C.scanID(a)) M5_LOGI("I2C device: 0x%02X", a);
    }

    const bool cold_boot = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED;
    draw_card(cold_boot);

    if (!start_nfc()) {
        // NFC が使えなくても QR は表示済みなので、名刺としては機能する
        M5_LOGW("NFC unavailable; QR only");
    }
    awake_since = millis();
}

void loop()
{
    M5.update();
    Units.update();
    emu_a.update();  // 毎ループ呼ぶ必要がある

    // 操作があれば起きている時間を延長
    if (M5.Touch.getCount() || M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
        awake_since = millis();
    }
    // ボタン A 長押しで画面をリフレッシュ（残像が気になるとき用）
    if (M5.BtnA.wasHold()) {
        draw_card(true);
        awake_since = millis();
    }

    // 読み取り中はスリープしない
    // NFC の状態変化をログに出す（かざしたとき Ready/Active になれば読まれている）
    static auto last_state = EmulationLayerA::State::None;
    if (emu_a.state() != last_state) {
        last_state = emu_a.state();
        M5_LOGI("NFC state: %d", (int)last_state);
    }

    if (emu_a.state() != EmulationLayerA::State::Active && millis() - awake_since > AWAKE_MS) {
        go_to_sleep();
    }
}
