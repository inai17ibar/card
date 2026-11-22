# card — PaperMono 電子名刺

M5Stack PaperMono（3.97" e-paper / ESP32-S3 / NFC）で作る電子名刺です。
相手は **QR を読む** か **スマホをかざす（NFC）** だけで、名刺ページ
<https://inai17ibar.github.io/card/> が開きます。

```
card/
├─ design/card.py   名刺画像の生成（ここだけ編集すれば Web と実機が同時に更新される）
├─ docs/            GitHub Pages（名刺ページ・card.png・vCard）
└─ firmware/        PlatformIO プロジェクト
```

## 名刺の内容を変える

```bash
pip install pillow qrcode
python design/card.py   # docs/card.png と firmware/include/card_png.h を再生成
```

フォントのパスは Linux（Noto Sans CJK / Inter）向けです。Mac で実行する場合は
`design/card.py` 冒頭の `CJK` / `INTER` を手元のフォントに書き換えてください。

## GitHub Pages の公開

Settings → Pages → Source: **Deploy from a branch** → `main` / `/docs`

## ファームの書き込み

```bash
cd firmware
pio run -t upload      # USB-C で接続して書き込み
pio device monitor     # ログ確認
```

### 動き

| 状態 | 挙動 |
|---|---|
| 起動 | 名刺画像を高画質モードで描画し、NFC（NTAG213 エミュレーション）を開始 |
| 起動から 3 分 | ディープスリープ（e-paper なので表示は残る、QR はそのまま読める） |
| 画面タッチ | 復帰して NFC を再開（画面は描き直さない） |
| ボタン A 長押し | 画面をリフレッシュ（残像が気になるとき） |

設定は `firmware/include/config.h`（URL・起きている時間・UID）。

### 注意

- PaperMono 対応は M5Unified / M5GFX の **develop ブランチ** を使っています。
- 公式ドキュメントによると、M5GFX の PaperMono 用波形はまだ不安定です。
  書き換え頻度を最小限にする作りにしています。
- NFC のアンテナ位置に合わせてスマホをかざしてください（読めない場合は位置をずらす）。
