
= Established Stateまでの実装！
== OpenSent Stateに遷移する
OpenSent Stateまでの遷移を実装します。タスクとしては以下になります。

 * OpenSent Stateまで遷移することを確認するテストの追加
 * テストを通るようにする。
 * Open Messageを送信可能にする。

この章でもすべての差分は載せずに主要な差分を載せます。
すべての差分を確認したい場合は、@<href>{https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/4,Open sent stateに遷移できるようにする。 #4}@<fn>{pr-open-sent}, 
@<href>{https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/5,データを送信関連のバグの修正 #5}@<fn>{pr-open-sent-fix}を確認してください。

//footnote[pr-open-sent][https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/4]
//footnote[pr-open-sent-fix][https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/5]


本章では実際に通信を発生させたりする点が今までの実装と異なります。
それ以外は今までの実装の延長線上にあるため簡単にのみ説明します。

=== テストの追加
以下のように、OpenSentステートに遷移していることを確認するテストを追加します。これも今までのものと変わりません。

//emlistnum[src/peer.rs][Rust]{
#[cfg(test)]
mod tests {
    #[tokio::test]
    async fn peer_can_transition_to_open_sent_state() {
        let config: Config = "64512 127.0.0.1 65413 127.0.0.2 active".parse().unwrap();
        let mut peer = Peer::new(config);
        peer.start();

        // 別スレッドでPeer構造体を実行しています。
        // これはネットワーク上で離れた別のマシンを模擬しています。
        tokio::spawn(async move {
            let remote_config = "64513 127.0.0.2 65412 127.0.0.1 passive".parse().unwrap();
            let mut remote_peer = Peer::new(remote_config);
            remote_peer.start();
            remote_peer.next().await;
            remote_peer.next().await;
        });

        // 先にremote_peer側の処理が進むことを保証するためのwait
        tokio::time::sleep(Duration::from_secs(1)).await;
        peer.next().await;
        peer.next().await;
        assert_eq!(peer.state, State::OpenSent);
    }
}
//}

=== OpenSentステータスに遷移する。
OpenSentに遷移するまでの実装を追加しました。
主に@<code>{src/peer.rs}の9-13行目でTCP Connectionが成功したときに、TcpConnectionConfirmedのイベントを発火させるようにしたこと、
18-23行目でConnect State時にTcpConnectionConfirmedイベントが発生したときOpenSent Stateに遷移するようにしました。
つまり@<chap>{learning_background}で説明したとおりの動作を実装しました。

//emlistnum[src/event.rs][Rust]{
pub enum Event {
    // 正常系しか実装しない本実装では別のEventとして扱う意味がないため、
    // TcpConnectionConfirmedはTcpCrAckedも兼ねている。
    TcpConnectionConfirmed,
}
//}

//emlistnum[src/state.rs][Rust]{
pub enum State {
    OpenSent,
}
//}

//emlistnum[src/peer.rs][Rust]{
    async fn handle_event(&mut self, event: &Event) {
        match &self.state {
            State::Idle => match event {
                Event::ManualStart => {
                    self.tcp_connection = match self.config.mode {
                        Mode::Active => self.connect_to_remote_peer().await,
                        Mode::Passive => self.wait_connection_from_remote_peer().await,
                    }
                    if self.tcp_connection.is_some() {
                        self.event_queue.enqueue(Event::TcpConnectionConfirmed);
                    } else {
                        panic!("TCP Connectionの確立が出来ませんでした。{:?}", self.config)
                    }
                    self.state = State::Connect;
                }
                _ => {}
            },
            State::Connect => match event {
                Event::TcpConnectionConfirmed => {
                    self.state = State::OpenSent
                },
                _ => {}
            },
            _ => {}
        }
    }
//}

=== Open Messageを送信する。
@<img>{starting_point|bgp-fsm}の通り、OpenSentに遷移するとき、Open Messageを送信します。この処理を実装します。

まず使用するライブラリをインストールします。

//emlistnum[src/lib.rs][Rust]{
[dependencies]
bytes = "1"
//}

次にファイルの構造を示します。
主に本章では、@<code>{connection.rs}、@<code>{message.rs}が主要なファイルです。
@<code>{connection.rs}は、Connectionの確立やデータの送受信（この段階では送信のみ）を担当します。この@<code>{connection.rs}は前章の実装では、@<code>{src/peer.rs}内に統合されていました。本章からは送信や受信など役割が増えてくるため別モジュールとして切り出します。
@<code>{message.rs}では、BGP Messageを表すデータを扱います。@<code>{message.rs}内で定義される@<code>{enum Message}は、
From trait、TryFrom Traitを実装することで、ソフトウェア内の表現であるMessageと、実際に送受信するローレベルなBytesを表すBytesMut型を相互に変換可能にします。


==== BGP Messageを実装する。
細かい実装の前に、必要な構造体やメソッドを@<code>{todo!}マクロを用いて仮実装をして大まかな構造を示します。
大まかな実装は以下のとおりです。

//emlistnum[src/lib.rs][Rust]{
mod packets;
//}

//emlistnum[src/packets.rs][Rust]{
mod header;
pub mod message;
mod open;
//}

//emlistnum[src/error.rs][Rust]{
#[derive(Error, Debug)]
#[error(transparent)]
pub struct ConvertBytesToBgpMessageError {
    #[from]
    source: anyhow::Error,
}

#[derive(Error, Debug)]
#[error(transparent)]
pub struct ConvertBgpMessageToBytesError {
    #[from]
    source: anyhow::Error,
}
//}

//emlistnum[src/packets/message.rs][Rust]{
use bytes::BytesMut;

use crate::error::{ConvertBgpMessageToBytesError, ConvertBytesToBgpMessageError};
use crate::packets::open::OpenMessage;

pub enum Message {
    Open(OpenMessage),
}

impl TryFrom<BytesMut> for Message {
    type Error = ConvertBytesToBgpMessageError;

    fn try_from(bytes: BytesMut) -> Result<Self, Self::Error> {
        todo!();
    }
}

impl From<Message> for BytesMut {
    fn from(message: Message) -> BytesMut {
        todo!();
    }
}
//}

//emlistnum[src/packets/open.rs][Rust]{
#[derive(PartialEq, Eq, Debug, Clone, Hash)]
pub struct OpenMessage;
//}

//emlistnum[src/packets/header.rs][Rust]{
#[derive(PartialEq, Eq, Debug, Clone, Hash)]
pub struct Header;
//}

Open Messageの情報や、Bytes列としての表現は@<href>{https://datatracker.ietf.org/doc/html/rfc4271#section-4.2,4.2.  OPEN Message Format}@<fn>{message-format}を参照してください。
あとは、このRFCの情報に従って実装していきます。

残りの実装は泥臭くBytes列 <=> Message型を変換するようにしているだけです。退屈で長いため、書内では省略します。
本段階のすべての差分を確認したい場合は、@<href>{https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/4,Open sent stateに遷移できるようにする。 #4}@<fn>{pr-open-sent}, 
@<href>{https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/5,データを送信関連のバグの修正 #5}@<fn>{pr-open-sent-fix}を確認してください。


//footnote[message-format][https://datatracker.ietf.org/doc/html/rfc4271#section-4.2]



==== データの送受信を扱うConnectionを実装する。
さて、@<hd>{BGP Messageを実装する。}で実装したMessageを送信できるようにConnectionを実装していきましょう。
以下のようになります。

@<code>{struct Peer}から@<code>{struct Connection}へ、通信関連の機能を写し、送信などの機能も追加しました。

//emlistnum[src/lib.rs][Rust]{
mod connection;
//}

//emlistnum[src/connection.rs][Rust]{
use anyhow::{Context, Result};
use bytes::{BufMut, BytesMut};
use tokio::io::{self, AsyncReadExt, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};

use crate::config::{Config, Mode};
use crate::error::CreateConnectionError;
use crate::packets::message::Message;

#[derive(Debug)]
pub struct Connection {
    conn: TcpStream,
    buffer: BytesMut,
}
impl Connection {
    pub async fn connect(config: &Config) -> Result<Self, CreateConnectionError> {
        let conn = match config.mode {
            Mode::Active => Self::connect_to_remote_peer(config).await,
            Mode::Passive => Self::wait_connection_from_remote_peer(config).await,
        }?;
        let buffer = BytesMut::with_capacity(1500);
        Ok(Self { conn, buffer })
    }

    pub async fn send(&mut self, message: Message) {
        let bytes: BytesMut = message.into();
        self.conn.write_all(&bytes[..]).await;
    }

    async fn connect_to_remote_peer(config: &Config) -> Result<TcpStream> {
        let bgp_port = 179;
        TcpStream::connect((config.remote_ip, bgp_port))
            .await
            .context(format!(
                "cannot connect to remote peer {0}:{1}",
                config.remote_ip, bgp_port
            ))
    }
    async fn wait_connection_from_remote_peer(config: &Config) -> Result<TcpStream> {
        let bgp_port = 179;
        let listener = TcpListener::bind((config.local_ip, bgp_port))
            .await
            .context(format!(
                "{0}:{1}にbindすることが出来ませんでした。",
                config.local_ip, bgp_port
            ))?;
        Ok(listener
            .accept()
            .await
            .context(format!(
                "{0}:{1}にてリモートからのTCP Connectionの要求を完遂することが出来ませんでした。
                リモートからTCP Connectionの要求が来ていない可能性が高いです。",
                config.local_ip, bgp_port
            ))?
            .0)
    }
}
//}


@<code>{struct Peer}でも、先ほど作成した@<code>{Connection}を使用するように変更します。
これにて、BGP OpenMessageのやり取りが可能になりました。

//emlistnum[src/peer.rs][Rust]{
use crate::connection::Connection;
#[derive(Debug)]
pub struct Peer {
    tcp_connection: Option<Connection>,
}

impl Peer {
    async fn handle_event(&mut self, event: &Event) {
        match &self.state {
            State::Idle => match event {
                Event::ManualStart => {
                    self.tcp_connection = Connection::connect(&self.config).await.ok();
                    if self.tcp_connection.is_some() {
                        self.event_queue.enqueue(Event::TcpConnectionConfirmed);
                    } else {
                        panic!("TCP Connectionの確立が出来ませんでした。{:?}", self.config)
                    }
                    self.state = State::Connect;
                }
                _ => {}
            },
            State::Connect => match event {
                Event::TcpConnectionConfirmed => {
                    self.tcp_connection
                        .as_mut()
                        .unwrap()
                        .send(Message::new_open(
                            self.config.local_as,
                            self.config.local_ip,
                        ))
                        .await;
                    self.state = State::OpenSent
                }
                _ => {}
            },
            _ => {}
        }
    }
}
//}

===[column] Integrationテスト環境でのパケットキャプチャ方法

BGP Messageが送信できるようになったら、Wiresharkでキャプチャして自作のパケットを見てみると楽しいです。またデバッグの役にもたちます。そのため、パケットキャプチャする方法を記載します。
手順は以下のとおりです。

 * パケットをやり取りするネットワークのNETWORK IDを確認する。以下出力だと、@<code>{b57e3cce48fe}がパケットをやり取りするネットワークのNetwork IDです。
//emlistnum[Network IDの確認][{}]{
 mrcsce@pop-os:~/programming/rustProjects/bgp/mrbgpdv2$ docker network ls
NETWORK ID     NAME                     DRIVER    SCOPE
43476b51d1ff   bridge                   bridge    local
1d0807314931   host                     host      local
de9c07390175   none                     null      local
b57e3cce48fe   tests_bgp-test-network   bridge    local
86db1458e999   tests_host1-network      bridge    local
5d46f799fd96   tests_host2-network      bridge    local
mrcsce@pop-os:~/programming/rustProjects/bgp/mrbgpdv2$ 
//}

 * このとき、@<code>{br-[Network ID]}のInterfaceが存在しております。
//emlistnum[Interfaceの存在の確認][{}]{
mrcsce@pop-os:~/programming/rustProjects/bgp/mrbgpdv2$ ifconfig | grep b57e3cce48fe
br-b57e3cce48fe: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500
mrcsce@pop-os:~/programming/rustProjects/bgp/mrbgpdv2$ 
//}

 * この@<code>{br-[Network ID]}のInterfaceをWiresharkでキャプチャしながら統合テストを実行すると、BGP Messageが確認できます。
//image[wireshark][パケットキャプチャの例]{
//}

===[/column]

== OpenConfirm Stateに遷移する
次にOpenConfirm Stateに遷移するまでの実装を示します。
これまでとの違いは、データの受信・受信内容に従ってイベントの発火を実装することです。

他は今までの延長です。今までの経験を持って@<img>{starting_point|bgp-fsm}を参照すれば、実装できると思われますのでそれ以外は省略します。

=== データの受信方法
さて、データの受信を実装します。
@<code>{Connection}の@<code>{get_message}メソッドがデータの受信用のメソッドです。
このメソッドは何らかのデータ（Bytes列）を受信している場合は、そのBytes列からMessageを作成し、Some(Message)を返します。
データを受信していない場合は、不完全に受信している場合はNoneを返します。

分かりづらいところは、実際にデータを読んでいるメソッドである@<code>{read_data_from_tcp_connection}メソッドだと思います。
TcpConnectionが張られたまま && しかしデータを受信していない場合は、@<code>{Err(io::ErrorKind::WouldBlock)}を返すことに注意してください。

//emlistnum[src/connection.rs][Rust]{
impl Connection {
    pub async fn get_message(&mut self) -> Option<Message> {
        self.read_data_from_tcp_connection().await;
        let buffer = self.split_buffer_at_message_separator()?;
        Message::try_from(buffer).ok()
    }

    /// self.bufferから1つのbgp messageを表すbyteを切り出す。
    fn split_buffer_at_message_separator(&mut self) -> Option<BytesMut> {
        let index = self.get_index_of_message_separator().ok()?;
        if self.buffer.len() < index {
            // 1つのBGPメッセージ全体を表すデータが受信できていない。
            // 半端に受信されているか一切受信されていない。
            return None;
        }
        Some(self.buffer.split_to(index))
    }

    /// self.bufferのうちどこまでが1つのbgp messageを表すbytesであるか返す。
    fn get_index_of_message_separator(&self) -> Result<usize> {
        let minimum_message_length = 19;
        if self.buffer.len() < 19 {
            return Err(anyhow::anyhow!("messageのseparatorを表すデータまでbufferに入っていません。データの受信が半端であることが想定されます。"));
        }
        Ok(u16::from_be_bytes([self.buffer[16], self.buffer[17]]) as usize)
    }

    async fn read_data_from_tcp_connection(&mut self) {
        loop {
            let mut buf: Vec<u8> = vec![];
            match self.conn.try_read_buf(&mut buf) {
                Ok(0) => (),                        // TCP ConnectionがCloseされたことを意味している。
                Ok(n) => self.buffer.put(&buf[..]), // n bytesのデータを受信した。
                Err(ref e) if e.kind() == io::ErrorKind::WouldBlock => break, // 今readできるデータがないことを意味する。
                Err(e) => panic!("read data from tcp connectionでエラー{:?}が発生しました", e),
            }
        }
    }
}
//}

=== Messageの受信をイベントとして発火させる実装
さて、Messageを受信できるようになりました。@<img>{starting_point|bgp-fsm}の通り、メッセージの受信はイベントとして表現されています。
この部分の実装を行います。

@<code>{Peer}の@<code>{next}メソッドでメッセージの受信をするようにしましょう。
メッセージを受信した場合は@<code>{handle_message}メソッドによってイベントを発火しています。

受信したデータ自体は、Eventに含ませることによって、処理するときに使用できるようにします。

//emlistnum[src/peer.rs][Rust]{
impl Peer {
    pub async fn next(&mut self) {
        if let Some(event) = self.event_queue.dequeue() {
            self.handle_event(&event).await;
        }

        if let Some(conn) = &mut self.tcp_connection {
            if let Some(message) = conn.get_message().await {
                self.handle_message(message);
            }
        }
    }

    fn handle_message(&mut self, message: Message) {
        match message {
            Message::Open(open) => self.event_queue.enqueue(Event::BgpOpen(open)),
        }
    }
}
//}

//emlistnum[src/event.rs][Rust]{
use crate::packets::open::OpenMessage;

pub enum Event {
    BgpOpen(OpenMessage),
}
//}


== Established Stateに遷移する
これEstablished Stateに遷移するまでは、同じようなコードが続きます。メッセージを作成し、送信すること。メッセージを受信したらイベントを発行すること。
この繰り返しで同じようなコードが続きます。何も新しいことがないため省略します。

今までの経験と@<img>{starting_point|bgp-fsm}を参照することで自力で実装可能だと思われます。
もし実装に詰まった場合は以下を参考にしてください。

 * @<href>{https://tools.ietf.org/html/rfc4271#section-8,RFCの8. BGP Finite State Machine (FSM)}@<fn>{rfc-fsm}
 * @<href>{https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/6,本PDFの実装のリポジトリのPR: Open confirm stateに遷移する #6}@<fn>{open-confirm}
 * @<href>{https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/7,本PDFの実装のリポジトリのPR: Establishedに遷移できるようにする #7}@<fn>{established}

//footnote[rfc-fsm][https://tools.ietf.org/html/rfc4271#section-8]
//footnote[open-confirm][https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/6]
//footnote[established][https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/7]
