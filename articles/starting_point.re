
= 実装開始！
== BGPはどのようなイベント駆動ステートマシンなのか
BGPはどのようなイベント駆動ステートマシンとして表すのが良いでしょうか。

実は @<href>{https://tools.ietf.org/html/rfc4271,RFC4271-A Border Gateway Protocol 4 (BGP-4)}@<fn>{RFC}の @<href>{https://tools.ietf.org/html/rfc4271#section-8,8.  BGP Finite State Machine (FSM)}に、イベント駆動ステートマシンの定義の記載があります。

//footnote[RFC][https://tools.ietf.org/html/rfc4271]
しかし最初から @<href>{https://tools.ietf.org/html/rfc4271#section-8,8.  BGP Finite State Machine (FSM)}を参照して、完全なBGPを作成することは大変です。
そのため本書ではBGPを次の段階に分けて開発します。また正常系のみ実装していきます。

 * Connectまで遷移する実装
 * Establishedまで遷移する実装
 * Update Messageを交換する実装

本書で実装するBGPのステートマシンを図示すると@<img>{bgp-fsm}になります。

@<img>{bgp-fsm}で登場するEventはそれぞれ@<table>{BGPのイベント駆動ステートマシンで登場するEventの説明}の通りです。

@<img>{bgp-fsm}で登場する状態（State）については、RFCのとおりです。
@<href>{https://www.infraexpert.com/study/study60.html,ネットワークエンジニアとして - BGPの技術}の@<href>{https://www.infraexpert.com/study/bgpz02.html,BGP - 4つのメッセージ、6つのステータスと状態遷移}@<fn>{ネットワークエンジニアとして BGP - 4つのメッセージ、6つのステータスと状態遷移}でも説明されております。

//footnote[ネットワークエンジニアとして BGP - 4つのメッセージ、6つのステータスと状態遷移][https://www.infraexpert.com/study/bgpz02.html]


//table[BGPのイベント駆動ステートマシンで登場するEventの説明][BGPのイベント駆動ステートマシンで登場するEventの説明]{
Event名	説明
---------------------------------------------------
ManualStart	BGPの開始を指示したときに発行されるイベント。@<br>{}RFC内でも同様に定義されている。
TcpConnectionConfirmed	対向機器とTCPコネクションを確立できたときに発行されるイベント。@<br>{}RFC内でも同様に定義されている。RFCでは、@<br>{}TCP ackを受信したときのイベント、Tcp Cr Ackedと区別している。@<br>{}しかし本書では正常系しか実装しないため、TCPコネクションが@<br>{}確立された時のイベントとしては本イベントのみにしている。
BGPOpen	対向機器からOpen Messageを受信したときに発行されるイベント。@<br>{}RFC内でも同様に定義されている。
KeepAliveMsg	対向機器からKeepalive Messageを受信したときに発行されるイベント@<br>{}RFC内でも同様に定義されている。
UpdateMsg	対向機器からUpdate Messageを受信したときに発行されるイベント@<br>{}RFC内でも同様に定義されている。
Established	Established Stateに遷移したときに発行されるイベント。@<br>{}存在するほうが実装しやすいため筆者が追加した@<br>{}RFCには存在しないイベント。
LocRibChanged	LocRib@<fn>{LocRib}が変更されたときに発行されるイベント。@<br>{}存在するほうが実装しやすいため追加した@<br>{}RFCには存在しないイベント。
AdjRibInChanged	AdjRibIn@<fn>{AdjRibIn}が変更されたときに発行されるイベント。@<br>{}存在するほうが実装しやすいため追加した@<br>{}RFCには存在しないイベント。
AdjRibOutChanged	AdjRibOut@<fn>{AdjRibOut}が変更されたときに発行されるイベント。@<br>{}存在するほうが実装しやすいため追加した@<br>{}RFCには存在しないイベント。
//}

//footnote[LocRib][https://datatracker.ietf.org/doc/html/rfc4271#section-1.1で説明されている。本書でも必要になったタイミングで説明する。]
//footnote[AdjRibIn][https://datatracker.ietf.org/doc/html/rfc4271#section-1.1で説明されている。本書でも必要になったタイミングで説明する。]
//footnote[AdjRibOut][https://datatracker.ietf.org/doc/html/rfc4271#section-1.1で説明されている。本書でも必要になったタイミングで説明する。]

//image[bgp-fsm][本書で実装するBGPのステートマシン図]{
//}

== プロジェクト作成
さて、実装に入りましょう。

 * Connectまで遷移する実装を行います。

この実装を終えた段階で全体像を掴むことができると思います。

== 最初のテストの追加
== Connect Stateへの遷移
== main関数の追加
