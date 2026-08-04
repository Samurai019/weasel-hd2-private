【小狼毫 HD2 Unicode】輸入法
================================

基於 中州韻輸入法引擎／Rime Input Method Engine 等開源技術
基於上游 [rime/weasel](https://github.com/rime/weasel) 獨立改造

這是一個專為《Helldivers 2》（絕地潛兵 2）聊天框中文輸入設計的小狼毫獨立共存版。原版小狼毫在該遊戲中可以顯示候選詞，但選詞後無法將中文提交到聊天框。本版本通過將輸入法的最終確認（commit）從標準 TSF `ITfRange::SetText()` 路徑改為 `SendInput(KEYEVENTF_UNICODE)`，繞過遊戲自定義聊天控件不兼容的 TSF composition 提交鏈，實現中文正常輸入。

本版本可與官方小狼毫同時安裝、同時運行、獨立配置、獨立卸載。

主要特性
--------

- **Unicode commit 提交**：輸入法確認文本以 `SendInput(KEYEVENTF_UNICODE)` 注入，兼容 HD2 聊天框
- **獨立共存**：獨立的 TSF CLSID、Profile GUID、IPC 管道、註冊表鍵、安裝目錄、用戶數據目錄和日誌目錄
- **候選窗定位修復**：在 HD2 自定義控件報告無效文本坐標時，自動回退到文檔視圖右下角區域
- **HD2 行為開關**：語言欄「中」圖標右鍵菜單可切換兩項 HD2 行為（存儲於註冊表 `HKCU\Software\Rime\WeaselHD2`，即時生效，下次按鍵即生效）：
  - 「HD2 Unicode 提交」：關閉時恢復原始 TSF `SetText` 提交路徑並禁用 `VK_PACKET` 直通
  - 「HD2 候選定位修正」：關閉時恢復原始 `GetTextExt` 定位邏輯（無右下角兜底錨點）
- **候選定位診斷日誌**：註冊表開啟 `Hd2CandidateFixLog`（DWORD 默認關）後，每次候選定位寫一行到 `%TEMP%\rime.weasel.hd2\candidate-position.log`，記錄定位分支、`GetTextExt` 結果與最終輸出位置，便於排查候選窗定位問題
- **語言欄完整**：保留標準 TSF 輸入模式語言欄圖標和右鍵菜單

安裝與使用
----------

本品適用於 Windows 8.1 ~ Windows 11

默認安裝目錄：`C:\Program Files\Rime-hd2\weasel-hd2-<版本號>`

用戶數據目錄：`%AppData%\RimeHD2`

日誌目錄：`%TEMP%\rime.weasel.hd2`

安裝後在系統語言設置中選擇「小狼毫 HD2 Unicode」輸入法即可使用。建議搭配「朙月拼音·簡化字」方案，開箱即用。

> ⚠️ 本版本不使用官方小狼毫的自動更新通道，如需升級請手動下載新版本覆蓋安裝。

與官方小狼毫的隔離
------------------

| 隔離項 | 官方小狼毫 | 本版本 |
|--------|-----------|--------|
| TSF CLSID | 原版 | 獨立 |
| 安裝目錄 | `C:\Program Files\Rime` | `C:\Program Files\Rime-hd2` |
| 用戶數據 | `%AppData%\Rime` | `%AppData%\RimeHD2` |
| 註冊表鍵 | `Software\Rime\Weasel` | `Software\Rime\WeaselHD2` |
| IPC 管道 | `WeaselNamedPipe` | `WeaselHD2NamedPipe` |
| 日誌目錄 | `%TEMP%\rime.weasel` | `%TEMP%\rime.weasel.hd2` |
| 服務互斥體 | 原版 | 獨立 |

原版小狼毫授權與致謝
====================

式恕堂 版權所無

授權條款：GPLv3

項目主頁：https://rime.im

您可能還需要 RIME 用於其他操作系統的發行版：

  * ibus-rime、fcitx5-rime 或 fcitx-rime 用於 Linux
  * 【鼠鬚管】用於 macOS （64位）

安裝輸入法
----------

本品適用於 Windows 8.1 ~ Windows 11

初次安裝時，安裝程序將顯示「安裝選項」對話框。

若要將【小狼毫】註冊到繁體中文（臺灣）鍵盤佈局，請在「輸入語言」欄選擇「中文（臺灣）」，再點擊「安裝」按鈕。

安裝完成後，仍可由開始菜單打開「安裝選項」更改輸入語言。

使用輸入法
----------

選取輸入法指示器菜單裏的【中】字樣圖標，開始用小狼毫寫字。

可通過快捷鍵 <kbd>Ctrl+`</kbd> 或 <kbd>F4</kbd> 呼出方案選單、切換輸入方式。

定製輸入法
----------

通過 開始菜單 » 小狼毫輸入法 訪問設定工具及常用位置。

用戶詞庫、配置文件位於 `%AppData%\Rime`，可通過菜單中的「用戶文件夾」打開。高水平玩家調教 Rime 輸入法常會用到。

修改詞庫、配置文件後，須「重新部署」方可生效。

定製 Rime 的方法，請參考 Wiki [《定製指南》](https://github.com/rime/home/wiki/CustomizationGuide)。如需定製 Weasel 獨有的樣式和行為，請參考本倉庫 [Wiki 頁面](https://github.com/rime/weasel/wiki)。

致謝
----

### 輸入方案設計：

  * 【朙月拼音】系列及【八股文】詞典
    - 部分數據來源於 CC-CEDICT、Android 拼音、新酷音、opencc 等開源項目
    - 維護者：佛振、瑾昀
  * 【注音／地球拼音】
    - 維護者：佛振、瑾昀
  * 【倉頡五代】
    - 發明人：朱邦復先生
    - 碼表源自 www.chinesecj.com
    - 構詞碼表作者：惜緣

  【五笔】【粵拼】【上海／蘇州吳語】【中古漢語拼音】【國際音標】等衆多方案
  不再以安裝包預裝形式提供。可由 <https://github.com/rime/plum> 下載安裝。

### 程序設計：

  * [佛振](https://github.com/lotem)
  * [鄒旭](https://github.com/zouxu09)
  * [Xiangyan Sun](https://github.com/wishstudio)
  * [Prcuvu](https://github.com/Prcuvu)
  * [nameoverflow](https://github.com/nameoverflow)
  * [fxliang](https://github.com/fxliang)
  * [Azuk 443](https://github.com/determ1ne)

  查看更多 [代碼貢獻者](https://github.com/rime/weasel/graphs/contributors)

### 美術：

  * 圖標設計／[Patricivs](https://github.com/Patricivs)
  * 配色方案／Aben、P1461、Patricivs、skoj、佛振、五磅兔

### 本品引用了以下開源軟件：

  * [Boost C++ Libraries](http://www.boost.org/) (Boost Software License)
  * [curl](https://curl.haxx.se/) (MIT/X derivate license)
  * [google-glog](https://github.com/google/glog) (BSD 3-Clause License)
  * [Google Test](https://github.com/google/googletest) (BSD 3-Clause License)
  * [LevelDB](https://github.com/google/leveldb) (BSD 3-Clause License)
  * [librime](https://github.com/rime/librime) (BSD 3-Clause License)
  * [marisa-trie](https://github.com/s-yata/marisa-trie) (BSD 2-Clause License, LGPL 2.1)
  * [OpenCC / 開放中文轉換](https://github.com/BYVoid/OpenCC) (Apache License 2.0)
  * [plum](https://github.com/rime/plum) (GNU Lesser General Public License v3.0)
  * [WinSparkle](https://github.com/vslavik/winsparkle) (MIT License)
  * [yaml-cpp](https://github.com/jbeder/yaml-cpp) (MIT License)
  * [7-Zip](https://www.7-zip.org) (GNU LGPLv2.1+ with unRAR restriction)

問題與反饋
----------

發現程序有 bug，請到 GitHub 反饋
<https://github.com/rime/weasel/issues>

歡迎提交 pull request
<https://github.com/rime/weasel/pulls>

Rime 輸入法（不限於 Windows 平臺）功能、使用方法與配置相關的問題，請反饋到
<https://github.com/rime/home/issues>

聯繫方式
--------

技術交流，歡迎光臨 [Rime 代碼之家](https://github.com/rime/home)，或致信 Rime 開發者 <rimeime@gmail.com>

謝謝！
