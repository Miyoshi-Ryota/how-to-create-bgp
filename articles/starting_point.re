
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
この章を終えた段階で全体像を掴むことができると思います。

本章では以下のタスクを行います。

 * プロジェクトの作成
 * 使用するライブラリ類のインストール
 * Connect Stateへ遷移することのテストを書く
 * Connect Stateへ遷移するテストを通す。
 * main関数の追加

まずはプロジェクトの作成及び使用するライブラリ類のインストールを行います。

//emlistnum[プロジェクトの作成][{}]{
cargo new <プロジェクトの名前>
//}

//emlistnum[Cargo.toml][{}]{
[package]
name = "<プロジェクトの名前>"
version = "0.1.0"
edition = "2021"
# See more keys and their definitions at https://doc.rust-lang.org/cargo/reference/manifest.html

[dependencies]
tokio = { version = "1.14.0", features = ["full"] }
thiserror = "1.0"
anyhow = "1.0"
//}


== 最初のテストの追加
次に最初にConnect Stateへ遷移することのテストを作成します。
@<code>{src/peer.rs}の通りです。

//emlistnum[src/peer.rs][Rust]{
/// [BGPのRFCで示されている実装方針](https://datatracker.ietf.org/doc/html/rfc4271#section-8)では、
/// 1つのPeerを1つのイベント駆動ステートマシンとして用いています。
/// Peer構造体はRFC内で示されている実装方針に従ったイベント駆動ステートマシンです。
#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn peer_can_transition_to_connect_state() {
        let config: Config = "64512 127.0.0.1 65413 127.0.0.2 active".parse().unwrap();
        let mut peer = Peer::new(config);
        peer.start();
        peer.next().await;
        assert_eq!(peer.state, State::Connect);
    }
}
//}

一つのPeerを一つのイベント駆動ステートマシンとして実装します。
Configは"自分のAS番号、自分のIP、対向側のAS番号、対向側のAS番号 動作モード@<fn>{動作モード}"のようにしています。

本テストはPeer構造体が以下のように動作することを期待していることを示しています。

Peerを作成します。作成時のStateはIdleです。startメソッドによって、ManualStart Eventを発生させます。
@<code>{peer.next().await;}によってEventを取り出し処理を進めます。
@<img>{bgp-fsm}の通り、Idle Stateのとき、ManualStart Eventが処理されることで、Connect Stateへ遷移します。

//footnote[動作モード][Active: 自分からBGPネイバーを構築する。Passive:自分からは開始しないが相手からきたときは受け入れる、といった設定。]


== Connect Stateへの遷移
さて、前述のテスト@<code>{src/peer.rs}が通るまで実装を進めましょう。
以下のようになります。@<code>{src/peer.rs}、@<code>{src/event.rs}、@<code>{src/event_queue.rs}、@<code>{src/state.rs}を見るとわかるように、@<code>{イベント駆動ステートマシン, テレビの実装例} と同じ実装方針になっています。今後もこの延長線上でコードが実装されます。

以下に掲載しているコードは@<code>{src/config.rs}のせいで長く見えてしまいますが、@<code>{src/config.rs}のほとんどは、単に@<code>{"64512 127.0.0.1 65413 127.0.0.2 active".parse()}でConfig構造体を作成できるようにしているだけです。

そのため、重要なコードは、@<code>{src/peer.rs}、@<code>{src/event.rs}、@<code>{src/event_queue.rs}、@<code>{src/state.rs}です。

また、現状では@<code>{src/peer.rs}のPeerのnextメソッド、handle_eventメソッドは非同期である必要がありません。しかしすぐに通信など非同期のほうが好ましい処理を追加すること、テストでも非同期であることを期待していることから現時点で非同期にしています。

//emlistnum[src/lib.rs][Rust]{
#![feature(backtrace)]
#![allow(dead_code, unused)]

mod autonomous_system_number;
mod config;
mod error;
mod event;
mod event_queue;
mod peer;
mod state;
//}

//emlistnum[src/peer.rs][Rust]{
use crate::config::Config;
use crate::event::Event;
use crate::event_queue::EventQueue;
use crate::state::State;

/// [BGPのRFCで示されている実装方針](https://datatracker.ietf.org/doc/html/rfc4271#section-8)では、
/// 1つのPeerを1つのイベント駆動ステートマシンとして用いています。
/// Peer構造体はRFC内で示されている実装方針に従ったイベント駆動ステートマシンです。
#[derive(PartialEq, Eq, Debug, Clone, Hash)]
struct Peer {
    state: State,
    event_queue: EventQueue,
    config: Config,
}

impl Peer {
    fn new(config: Config) -> Self {
        let state = State::Idle;
        let event_queue = EventQueue::new();
        Self { state, event_queue, config }
    }

    fn start(&mut self) {
        self.event_queue.enqueue(Event::ManualStart);
    }

    async fn next(&mut self) {
        if let Some(event) = self.event_queue.dequeue() {
            self.handle_event(&event).await;
        }
    }

    async fn handle_event(&mut self, event: &Event) {
        match &self.state {
            State::Idle => {
                match event {
                    Event::ManualStart => {
                        self.state = State::Connect;
                    }
                    _ => {}
                }
            },
            _ => {}
        }
    }
}
//}

//emlistnum[src/event.rs][Rust]{
/// BGPの[RFC内 8.1 で定義されているEvent](https://datatracker.ietf.org/doc/html/rfc4271#section-8.1)を
/// 表す列挙型です。
#[derive(PartialEq, Eq, Debug, Clone, Copy, Hash, PartialOrd, Ord)]
pub enum Event {
    ManualStart,
}
//}

//emlistnum[src/event_queue.rs][Rust]{
use crate::event::Event;
use std::collections::VecDeque;

#[derive(Debug, Clone, Hash, PartialEq, Eq)]
pub struct EventQueue(VecDeque<Event>);

impl EventQueue {
    pub fn new() -> Self {
        EventQueue(VecDeque::new())
    }

    pub fn enqueue(&mut self, event: Event) {
        self.0.push_front(event);
    }

    pub fn dequeue(&mut self) -> Option<Event> {
        self.0.pop_back()
    }
}
//}

//emlistnum[src/state.rs][Rust]{
#[derive(PartialEq, Eq, Debug, Clone, Copy, Hash)]
pub enum State {
    Idle,
    Connect,
}
//}

//emlistnum[src/autonomous_system_number.rs][Rust]{
#[derive(PartialEq, Eq, Debug, Clone, Copy, Hash, PartialOrd, Ord)]
pub struct AutonomousSystemNumber(u16);

impl From<u16> for AutonomousSystemNumber {
    fn from(as_number: u16) -> Self {
        Self(as_number)
    }
}
//}

//emlistnum[src/config.rs][Rust]{
use crate::autonomous_system_number::AutonomousSystemNumber;
use crate::error::ConfigParseError;
use anyhow::{Context, Result};
use std::net::Ipv4Addr;
use std::str::FromStr;

#[derive(PartialEq, Eq, Debug, Clone, Copy, Hash, PartialOrd, Ord)]
pub struct Config {
    local_as: AutonomousSystemNumber,
    local_ip: Ipv4Addr,
    remote_as: AutonomousSystemNumber,
    remote_ip: Ipv4Addr,
    mode: Mode,
}

#[derive(PartialEq, Eq, Debug, Clone, Copy, Hash, PartialOrd, Ord)]
enum Mode {
    Passive,
    Active,
}

impl FromStr for Mode {
    type Err = ConfigParseError;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "passive" | "Passive" => Ok(Mode::Passive),
            "active" | "Active" => Ok(Mode::Active),
            _ => Err(ConfigParseError::from(anyhow::anyhow!("cannot parse {s}"))),
        }
    }
}

impl FromStr for Config {
    type Err = ConfigParseError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let config: Vec<&str> = s.split(' ').collect();
        let local_as = AutonomousSystemNumber::from(config[0].parse::<u16>().context(format!(
            "cannot parse 1st part of config, `{0}`, as as-number and config is {1}",
            config[0], s
        ))?);
        let local_ip: Ipv4Addr = config[1].parse().context(format!(
            "cannot parse 2nd part of config, `{0}`, as as-number and config is {1}",
            config[1], s
        ))?;
        let remote_as = AutonomousSystemNumber::from(config[2].parse::<u16>().context(format!(
            "cannot parse 3rd part of config, `{0}`, as as-number and config is {1}",
            config[2], s
        ))?);
        let remote_ip: Ipv4Addr = config[3].parse().context(format!(
            "cannot parse 4th part of config, `{0}`, as as-number and config is {1}",
            config[3], s
        ))?;
        let mode: Mode = config[4].parse().context(format!(
            "cannot parse 5th part of config, `{0}`, as as-number and config is {1}",
            config[4], s
        ))?;
        Ok(Self {
            local_as,
            local_ip,
            remote_as,
            remote_ip,
            mode,
        })
    }
}
//}

//emlistnum[src/error.rs][Rust]{
use thiserror::Error;

#[derive(Error, Debug)]
#[error(transparent)]
pub struct ConfigParseError {
    #[from]
    source: anyhow::Error,
}
//}


@<code>{cargo test}でテストを実行し、テストが通ることを確認しましょう。以下のように表示されればOKです。

//emlistnum[テスト結果][{}]{
mrcsce@pop-os:~/programming/rustProjects/bgp/mrbgpdv2$ cargo test
    Finished test [unoptimized + debuginfo] target(s) in 0.01s
     Running unittests (target/debug/deps/mrbgpdv2-d3ae14e12196c9a9)

running 1 test
test peer::tests::peer_can_transition_to_connect_state ... ok
}
//}


== main関数の追加

さて、main関数を追加します。@<hd>{最初のテストの追加}で示したテストとほとんどと同じです。9行目でConfig構造体から、Peer構造体を作成し、11行目でstartメソッドにてManualStartイベントを発火させます。
その後は、17-19行目で@<code>{loop}ブロックを用いて、常にnextを呼び続けています。nextメソッドはイベントが発生していればつぎの処理をし、イベントがなければそのまま、という実装になっています。無限ループさせることで、新しいイベントが発生するまで待ち続けること、新しいイベントが発生したらすぐに処理をすること、を意味しています。

//emlistnum[src/main.rs][Rust]{
use std::str::FromStr;

use mrbgpdv2::peer::Peer;
use mrbgpdv2::config::Config;

#[tokio::main]
async fn main() {
    let configs = vec![Config::from_str("64512 127.0.0.1 65413 127.0.0.2 active").unwrap()];
    let mut peers: Vec<Peer> = configs.into_iter().map(Peer::new).collect();
    for peer in &mut peers {
        peer.start();
    }

    let mut handles = vec![];
    for mut peer in peers {
        let handle = tokio::spawn(async move {
            loop {
                peer.next().await;
            }
        });
        handles.push(handle);
    }

    // main関数が勝手に終了しないようにしている。
    for handle in handles {
        handle.await;
    }
}
//}

== TCP Connectionの作成
@<img>{bgp-fsm}の通りIdle -> Connectに遷移するとき、対向機器とTCPコネクションの作成を試みます。本項ではTCPコネクションの作成の部分の実装を行います。

まずは、TCP Connectionを張っていることを確認できるようにテストを書き換えます。3行目, 6-13行を追加しました。
テストのtokio::spawnによって、別スレッドでもPeer構造体（remote_peer）を実行しています。これはネットワーク上で離れた別のマシンを模擬しています。模擬した別のマシンとTCP Connectionが張れていることを確認しています。

//emlistnum[src/main.rs][Rust]{
mod tests {
    use super::*;
    use tokio::time::{sleep, Duration};
    async fn peer_can_transition_to_connect_state() {
        let config: Config = "64512 127.0.0.1 65413 127.0.0.2 active".parse().unwrap();
        let mut peer = Peer::new(config);
        peer.start();

        tokio::spawn(async move {
            let remote_config = "64513 127.0.0.2 65412 127.0.0.1 passive".parse().unwrap();
            let mut remote_peer = Peer::new(remote_config);
            remote_peer.start();
            remote_peer.next().await;
        });

        // 先にremote_peer側の処理が進むことを保証するためのwait
        tokio::time::sleep(Duration::from_secs(1)).await;
        peer.next().await;
        assert_eq!(peer.state, State::Connect);
    }
}
//}


それでは実装に入ります。

ただしPDF上では主要なコードを紹介します。例えばコンパイラにしたがってpubキーワードを付与したり、useキーワードを使用してimportしたりするだけの差分は省略しております。またフルのコードではなく主要な追加分を記載しています。

完全な差分やコードを確認したい場合は@<href>{https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/3,Tcp connectionを作成する部分を実装 #3}@<fn>{tcp-conn-pr}のPRを参照ください。

//footnote[tcp-conn-pr][https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/3]

@<code>{src/peer.rs}の13行目, 21-28行目, 37行目-64行目が主要な差分です。

//emlistnum[src/peer.rs][Rust]{
use tokio::net::{TcpListener, TcpStream};

pub struct Peer {
    tcp_connection: Option<TcpStream>,
}

impl Peer {
    pub fn new(config: Config) -> Self {
        Self {
            state,
            event_queue,
            config,
            tcp_connection: None,
        }
    }

    async fn handle_event(&mut self, event: &Event) {
        match &self.state {
            State::Idle => match event {
                Event::ManualStart => {
                    self.tcp_connection = match self.config.mode {
                        Mode::Active => self.connect_to_remote_peer().await,
                        Mode::Passive => self.wait_connection_from_remote_peer().await,
                    }
                    .ok();
                    self.tcp_connection.as_ref().unwrap_or_else(|| {
                        panic!("TCP Connectionの確立が出来ませんでした。{:?}", self.config)
                    });
                    self.state = State::Connect;
                }
                _ => {}
            },
            _ => {}
        }
    }

    async fn connect_to_remote_peer(&self) -> Result<TcpStream> {
        let bgp_port = 179;
        TcpStream::connect((self.config.remote_ip, bgp_port))
            .await
            .context(format!(
                "cannot connect to remote peer {0}:{1}",
                self.config.remote_ip, bgp_port
            ))
    }

    async fn wait_connection_from_remote_peer(&self) -> Result<TcpStream> {
        let bgp_port = 179;
        let listener = TcpListener::bind((self.config.local_ip, bgp_port))
            .await
            .context(format!(
                "{0}:{1}にbindすることが出来ませんでした。",
                self.config.local_ip, bgp_port
            ))?;
        Ok(listener
            .accept()
            .await
            .context(format!(
                "{0}:{1}にてリモートからのTCP Connectionの要求を完遂することが出来ませんでした。
                リモートからTCP Connectionの要求が来ていない可能性が高いです。",
                self.config.local_ip, bgp_port
            ))?
            .0)
    }
}
//}


実装が終わったらテストを実行して結果を確認しましょう。

テストを実行する際には以下2つの注意点があります。

BGPで使用する179番ポートにBindするためにはroot権限が必要になります。sudoでcargoを実行できるようにする必要があります。筆者は以下のようにcargoの設定を編集することで対応しています。
//emlistnum[~/.cargo/config,root権限で実行可能にする設定][{}]{
mrcsce@pop-os:~/programming/rustProjects/bgp/mrbgpdv2$ cat ~/.cargo/config

# nightly-x86_64-unknown-linux-gnu
[target.x86_64-unknown-linux-gnu]
runner = 'sudo -E'
mrcsce@pop-os:~/programming/rustProjects/bgp/mrbgpdv2$ 
//}

またMacOSの場合は、127.0.0.2にはバインド出来ないため、Dockerや仮想マシンなどのLinux環境を用意してください。今後もnetlinkなど、Linux固有のものが登場します。実装が進むにつれて、Rust上でのテストだけでなく、Docker、Docker-composeを用いて実際にBGP Messageのやりとりを行えているかどうかテストしたり、ルートの交換ができていることをテストするスクリプトを追加します。Linux以外を使用していて、活つ適切なLinux環境の用意の仕方がわからない場合は、先に@<chap>{integration_tests}を参照し、テスト環境を用意してください。

//emlistnum[テスト結果][{}]{
mrcsce@pop-os:~/programming/rustProjects/bgp/mrbgpdv2$ cargo test
   Compiling mrbgpdv2 v0.1.0 (/home/mrcsce/programming/rustProjects/bgp/mrbgpdv2)
    Finished test [unoptimized + debuginfo] target(s) in 1.22s
     Running unittests (target/debug/deps/mrbgpdv2-d3ae14e12196c9a9)
[sudo] mrcsce のパスワード: 

running 1 test
test peer::tests::peer_can_transition_to_connect_state ... ok

test result: ok. 1 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out; finished in 1.00s
//}


===[column] RFCと本PDFの相違点
実はRFCにもBGPを表すイベント駆動ステートマシンの細かい動作が記載されています。例えば、State Idle時、ManualStart Eventが発生したときの動作は
@<href>{https://datatracker.ietf.org/doc/html/rfc4271#section-8.2.2,RFC 4271 section-8.2.2}@<fn>{rfc4271}
に記載されています。

本書ではState Idle時、ManualStart Eventが発火したとき、StateをConnectに変更する、及びTCP Connectionを作成するだけにしています。しかしRFCではいろんなタイマーやカウンタを初期化したしています。それらのタイマーやカウンタは異常時に使用されるものなので、本書では省略しています。
また、Idleイベント時にManualState以外のEvnetが発火した時の動作も記載されていますが、同様の理由で本書では省略しています。

//footnote[rfc4271][https://datatracker.ietf.org/doc/html/rfc4271#section-8.2.2]
===[/column]
