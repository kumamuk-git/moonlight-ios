# Moonlight iOS/tvOS

## 個人版を Windows から iPad・iPhone に入れる

1. この変更を自分の GitHub リポジトリの既定ブランチに push します。AOO を含むサブモジュールの参照も含めてください。
2. GitHub の **Actions → Build unsigned IPA → Run workflow** を実行します。完了した実行の **Artifacts** から `Moonlight-unsigned.ipa` をダウンロードします。ビルドには GitHub の macOS ランナーを使います。
3. Windows で Sideloadly を開き、USB 接続した端末と IPA を選んで、自分の Apple Account で署名・インストールします。端末の案内に従って開発者を信頼し、必要に応じてデベロッパモードを有効にします。無料アカウントでは通常 7 日ごとに再署名します。[Sideloadly 公式サイト](https://sideloadly.io/)

### 変換・無変換

外付けキーボードの `International4`（変換）を Windows の `VK_CONVERT`（`0x1C`）、`International5`（無変換）を `VK_NONCONVERT`（`0x1D`）に変換します。押下と解放の両方を転送します。実機では Moonlight で Windows に接続し、日本語 IME で普段の変換・無変換の割り当てが働くかを確認できます。

### SonoBus の音を聴く

1. Sunshine と同じ Windows PC で SonoBus を起動し、送りたい音を入力に割り当てます。DAW の出力を送りたい場合は、そのバスに SonoBus プラグインを挿します。
2. SonoBus はグループから退出した状態で送信を有効にします。**Use Specific UDP Port** でポートを固定するか、接続画面に表示されるローカル UDP ポートを控えます。
3. iPad・iPhone の Moonlight 設定で **Stream Audio** をタップし、そのポートを入力して **Use SonoBus** を選び、ストリームを開始します。初期表示の `11000` は入力例です。PC と端末の間で指定した UDP ポートに到達できることが必要です。

この設定では音声を SonoBus から直接受信します。通常の Moonlight 音声へ戻すには **Stream Audio → Use Moonlight** を選び、再接続します。受信対象は 1 台の PC で、PCM と Opus に対応します。モノラルは左右両方に再生し、ステレオは左右を保持します。[SonoBus](https://github.com/sonosaurus/sonobus) / [使用している AOO](https://github.com/essej/aoo/tree/9960da7f277b2e0a4ef2725c33895559b634cab7)

### iPad の画面に解像度を合わせる

Moonlight の **Resolution → Full** でパネルの実ピクセル数を横向きで要求します。**Optimize Game Settings → Yes** を選び、Windows 側の Sunshine で対象画面を指定します。すでに有効な画面なら `dd_configuration_option = verify_only` と `dd_resolution_option = auto` に設定し、新しくセッションを開始します。Windows の対象ディスプレイでその解像度が選べることが条件です。一覧にない場合は、対応するカスタム解像度または仮想ディスプレイを用意します。[Sunshine のディスプレイ設定](https://docs.lizardbyte.dev/projects/sunshine/latest/md_docs_2configuration.html#dd_resolution_option)

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
