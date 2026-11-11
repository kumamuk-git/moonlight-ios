# Moonlight iOS/tvOS

## 個人版を Windows から iPad・iPhone に入れる

1. fork の **Actions → Build unsigned IPA → Run workflow** を開き、ブランチ `codex/jis-conversion-keys-sonobus` を選んで実行します。
2. 完了した実行の **Artifacts** から `Moonlight-unsigned.ipa` をダウンロードします。
3. Windows で Sideloadly を開き、USB 接続した端末と IPA を選んで、自分の Apple Account で署名・インストールします。端末の案内に従って開発者を信頼し、必要に応じてデベロッパモードを有効にします。無料アカウントでは通常 7 日ごとに再署名します。[Sideloadly 公式サイト](https://sideloadly.io/)

### 配布 IPA のライセンスと対応ソース

各 Actions 実行の **Summary** と IPA 内の `Payload/Moonlight.app/Licenses/SOURCE.md` に、その版のソースとライセンスへのリンクがあります。ライセンス原文も同じフォルダーに収録しています。

### Windows で外付けキーボードを使う

Windows 側を日本語キーボード配列にすると、JIS の「ろ」「￥」「ひらがな／カタカナ」「変換」「無変換」を使えます。メニューキー、Print Screen、キーボードの音量・ミュートキーも、端末から Moonlight に届いた押下と解放を転送します。

Apple の日本語キーボードの「かな」「英数」は、Windows の IME オン・オフに対応します。既定では端末の優先言語で日本語・韓国語を判定します。端末を英語などで使っている場合は、iPad・iPhone の **設定 → アプリ → Moonlight → Keyboard Language Keys** で **Japanese (かな / 英数)** を選んでください。韓国語キーボードでは **Korean (한/영 / 한자)** を選ぶとハングル切り替え・漢字変換になります。

### SonoBus の音を聴く

1. Sunshine と同じ Windows PC で SonoBus を起動し、送りたい音を入力に割り当てます。DAW の出力を送りたい場合は、そのバスに SonoBus プラグインを挿します。
2. PC の SonoBus で **Connect → Private Group** を開き、標準サーバー（`aoo.sonobus.net`）でグループに参加し、送信を有効にします。Moonlight は最初に接続できた相手の音を再生するので、PC と端末の 2 台で使うグループを選んでください。
3. iPad・iPhone の Moonlight で **設定 → Stream Audio → SonoBus Group…** を開き、PC と同じグループ名とパスワードを入力します。パスワードのないグループは空欄で構いません。**Use SonoBus Group** を選んでストリームを開始すると、PC の参加者一覧に `Moonlight-` で始まる名前が表示されます。

UDP で直接つなぐ場合は、PC の SonoBus をグループから退出させ、送信を有効にします。Moonlight の **Stream Audio → SonoBus Direct UDP…** に PC の SonoBus が表示するローカル UDP ポートを入力し、**Use SonoBus** を選びます。

Moonlight は再接続時に同じ参加者名と端末側の UDP ポートを使います。Direct UDP では、PC から見た端末の IP アドレスやポートが変わると別の相手として表示されます。旧版で増えた接続先は自動では消えないため、不要なものは PC の SonoBus で削除してください。

この設定では音声を SonoBus から直接受信します。通常の Moonlight 音声へ戻すには **Stream Audio → Use Moonlight** を選び、再接続します。受信対象は 1 台の PC で、PCM と Opus に対応します。モノラルは左右両方に再生し、ステレオは左右を保持します。[SonoBus](https://github.com/sonosaurus/sonobus) / [使用している AOO](https://github.com/essej/aoo/tree/9960da7f277b2e0a4ef2725c33895559b634cab7)

[![CI](https://github.com/moonlight-stream/moonlight-ios/actions/workflows/ci.yml/badge.svg)](https://github.com/moonlight-stream/moonlight-ios/actions/workflows/ci.yml)

[Moonlight for iOS/tvOS](https://moonlight-stream.org) is an open source client for [Sunshine](https://github.com/LizardByte/Sunshine) and NVIDIA GameStream. Moonlight for iOS/tvOS allows you to stream your full collection of games and apps from your powerful desktop computer to your iOS device or Apple TV.

Moonlight also has a [PC client](https://github.com/moonlight-stream/moonlight-qt) and [Android client](https://github.com/moonlight-stream/moonlight-android).

Check out [the Moonlight wiki](https://github.com/moonlight-stream/moonlight-docs/wiki) for more detailed project information, setup guide, or troubleshooting steps.

[![Moonlight for iOS and tvOS](https://moonlight-stream.org/images/App_Store_Badge_135x40.svg)](https://apps.apple.com/us/app/moonlight-game-streaming/id1000551566)

## Building
* Install Xcode from the [App Store page](https://apps.apple.com/us/app/xcode/id497799835)
* Run `git clone --recursive https://github.com/moonlight-stream/moonlight-ios.git`
  *  If you've already clone the repo without `--recursive`, run `git submodule update --init --recursive`
* Open Moonlight.xcodeproj in Xcode
* To run on a real device, you will need to locally modify the signing options:
    * Click on "Moonlight" at the top of the left sidebar
    * Click on the "Signing & Capabilities" tab
    * Under "Targets", select "Moonlight" (for iOS/iPadOS) or "Moonlight TV" (for tvOS)
    * In the "Team" dropdown, select your name. If your name doesn't appear, you may need to sign into Xcode with your Apple account.
    * Change the "Bundle Identifier" to something different. You can add your name or some random letters to make it unique.
    * Now you can select your Apple device in the top bar as a target and click the Play button to run.
