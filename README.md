# Moonlight iOS/tvOS

## 個人版を Windows から iPad・iPhone に入れる

1. この変更を自分の GitHub リポジトリの既定ブランチに push します。AOO を含むサブモジュールの参照も含めてください。
2. `master` に push すると GitHub Actions が macOS でビルドします。手動の場合は **Actions → Build unsigned IPA → Run workflow** を選びます。完了した実行の **Artifacts** から `Moonlight-unsigned.ipa` をダウンロードします。
3. Windows で Sideloadly を開き、USB 接続した端末と IPA を選んで、自分の Apple Account で署名・インストールします。端末の案内に従って開発者を信頼し、必要に応じてデベロッパモードを有効にします。無料アカウントでは通常 7 日ごとに再署名します。[Sideloadly 公式サイト](https://sideloadly.io/)

### 配布 IPA のライセンスと対応ソース

**2026-11-12 配布のビルド #5** は、Moonlight iOS に SonoBus の直接受信などを追加した改造版です。この IPA に適用される Moonlight、AOO、oscpack のライセンスと、ビルドに使ったソースは以下から取得できます。

- [ビルド #5 の IPA と SHA-256](https://github.com/kumamuk-git/moonlight-ios/actions/runs/43047941066#artifacts)
- [改造版のソース（801d39d）](https://github.com/kumamuk-git/moonlight-ios/tree/801d39db1369da9bcebe07ef143cbd5223ce732d) · [この版のビルド手順](https://github.com/kumamuk-git/moonlight-ios/blob/801d39db1369da9bcebe07ef143cbd5223ce732d/.github/workflows/unsigned-ipa.yml)
- [Moonlight の GPLv3 と App Store 配布例外](https://github.com/kumamuk-git/moonlight-ios/blob/801d39db1369da9bcebe07ef143cbd5223ce732d/LICENSE.txt)
- [AOO の BSD 3条ライセンス](https://github.com/essej/aoo/blob/9960da7f277b2e0a4ef2725c33895559b634cab7/LICENSE) · [oscpack の MIT ライセンス](https://github.com/essej/aoo/blob/9960da7f277b2e0a4ef2725c33895559b634cab7/deps/oscpack/LICENSE)

依存ソースも含めてビルド #5 を取得するには、次のコマンドを使います。

```sh
git clone https://github.com/kumamuk-git/moonlight-ios.git
cd moonlight-ios
git checkout 801d39db1369da9bcebe07ef143cbd5223ce732d
git submodule update --init --recursive
```

以降のビルドでは、各 Actions 実行の **Summary** と IPA 内の `Payload/Moonlight.app/Licenses/SOURCE.md` に、その版のソースとライセンスへのリンクがあります。ライセンス原文も同じフォルダーに収録しています。

### 変換・無変換

外付けキーボードの `International4`（変換）を Windows の `VK_CONVERT`（`0x1C`）、`International5`（無変換）を `VK_NONCONVERT`（`0x1D`）に変換します。押下と解放の両方を転送します。実機では Moonlight で Windows に接続し、日本語 IME で普段の変換・無変換の割り当てが働くかを確認できます。

### SonoBus の音を聴く

1. Sunshine と同じ Windows PC で SonoBus を起動し、送りたい音を入力に割り当てます。DAW の出力を送りたい場合は、そのバスに SonoBus プラグインを挿します。
2. PC の SonoBus で **Connect → Private Group** を開き、標準サーバー（`aoo.sonobus.net`）でグループに参加し、送信を有効にします。Moonlight は最初に接続できた相手の音を再生するので、PC と端末の 2 台で使うグループを選んでください。
3. iPad・iPhone の Moonlight で **設定 → Stream Audio → SonoBus Group…** を開き、PC と同じグループ名とパスワードを入力します。パスワードのないグループは空欄で構いません。**Use SonoBus Group** を選んでストリームを開始すると、PC の参加者一覧に `Moonlight-` で始まる名前が表示されます。

UDP で直接つなぐ場合は、PC の SonoBus をグループから退出させ、送信を有効にします。Moonlight の **Stream Audio → SonoBus Direct UDP…** に PC の SonoBus が表示するローカル UDP ポートを入力し、**Use SonoBus** を選びます。

Moonlight は再接続時に同じ参加者名と端末側の UDP ポートを使います。Direct UDP では、PC から見た端末の IP アドレスやポートが変わると別の相手として表示されます。旧版で増えた接続先は自動では消えないため、不要なものは PC の SonoBus で削除してください。

この設定では音声を SonoBus から直接受信します。通常の Moonlight 音声へ戻すには **Stream Audio → Use Moonlight** を選び、再接続します。受信対象は 1 台の PC で、PCM と Opus に対応します。モノラルは左右両方に再生し、ステレオは左右を保持します。[SonoBus](https://github.com/sonosaurus/sonobus) / [使用している AOO](https://github.com/essej/aoo/tree/9960da7f277b2e0a4ef2725c33895559b634cab7)

### iPad の画面に解像度を合わせる

Moonlight の **Resolution → Full** でパネルの実ピクセル数を横向きで要求します。**Optimize Game Settings → Yes** を選び、Windows 側の Sunshine で対象画面を指定します。すでに有効な画面なら `dd_configuration_option = verify_only` と `dd_resolution_option = auto` に設定し、新しくセッションを開始します。Windows の対象ディスプレイでその解像度が選べることが条件です。一覧にない場合は、対応するカスタム解像度または仮想ディスプレイを用意します。[Sunshine のディスプレイ設定](https://docs.lizardbyte.dev/projects/sunshine/latest/md_docs_2configuration.html#dd_resolution_option)

#### 2420 × 1668 の iPad に Windows のデスクトップ全体を表示する例

改造版で **Desktop** に接続すると、最初の映像フレームが届いた時に、Windows のポインターを配信画面の中央に置きます。別のモニターにポインターを置いたまま切断しても、次の接続から iPad 上でトラックパッドを使えます。

1. Windows で Parsec Virtual Display Driver（0.37 以降）の仮想モニターを使う場合は、空いているカスタム解像度枠に幅 `2420`、高さ `1668`、リフレッシュレート `60` Hz を十進数で登録します。既存の枠は保持します。[Parsec のカスタム解像度の設定](https://support.parsec.app/hc/en-us/articles/32361359271444-VDD-Advanced-Configuration)
2. 仮想モニターを作成または再接続し、Windows の **設定 → システム → ディスプレイ** で「表示画面を拡張する」を選びます。そのモニターを選択して `2420 × 1668` に設定します。Sunshine で使う間はモニターを有効にしておきます。既存の Parsec ドライバーでモニターを作成・削除できる管理ツールの一例は [ParsecVDisplay](https://github.com/nomi-san/parsec-vdd/releases) です。
3. Sunshine の Troubleshooting ログでそのモニターの `device_id` を確認し、**Configuration → Audio/Video** の出力先（`output_name`）に指定して設定を反映します。この指定は Sunshine のほかのクライアントにも適用されます。元の画面へ戻すには出力先を以前の値に戻します。
4. iPad で **Full** と **60 FPS** を選び、既存の Desktop セッションを終了してから新規に開始します。Sunshine のログに `Capture size : 2420x1668` と出れば、Windows のデスクトップと iPad の縦横比が一致しています。目的のアプリが別画面で開いた場合は、そのウィンドウを仮想モニターへ移動します。

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
