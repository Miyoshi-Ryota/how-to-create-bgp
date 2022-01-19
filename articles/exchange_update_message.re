
= Update Messageを交換する！
ここからは、本当にルーティングテーブルを更新するのに必要な作業に入っていきましょう。
BGPでは、Peer間で、ルーティングの情報をUpdate Messageと呼ばれるメッセージによって交換し合います。

BGP Update Messageを送信する内部のプロセスは複雑です。 概要を掴みやすくするためにコードを用いた説明の前に自然言語で説明をします。

説明のためにLoc-RIB, Adj-RIB-In, Adj-RIB-Outの3つの用語を導入します。これらはBGPのRFCで用いられている用語です。

これらの用語を使用してBGPのアップデートメッセージが送信されるまでのプロセスを見ていきましょう。

//table[BGPのイベント駆動ステートマシンで登場するEventの説明][BGPのイベント駆動ステートマシンで登場するEventの説明]{
用語	説明
---------------------------------------------------
Loc-RIB	Local Routing Information Baseの略。要はただのルーティングテーブル。@<br>{}要はただのルーティングテーブルなのですべてのPeerで共有する。
Adj-RIB-In	BGPのネイバーから受信したルーティング情報。Peerごとに生成する。
Adj-RIB-Out	BGPのネイバーに送信する / したルーティング情報。Peerごとに生成する。
//}


BGPのアップデートメッセージに関するプロセスは決定プロセス、送信プロセスに分けられます。
決定プロセスでは、Adj-RIB-InのルートからLoc-RIB及びAdj-RIB-Outに格納するルートを選択します。
送信プロセスではAdj-RIB-Outの情報をピアに送信します。

もう少し詳細に踏み込みます。決定プロセスはさらに次のPhase1, Phase2, Phase3に分けられます。

 * Phase1: Adj-RIB-InのRouteの優先度合いを計算する。
 * Phase2: Phase1で計算したRouteの優先度を使用して、Loc-RIBに格納するルートを選択し、そのルートをLoc-RIBに格納する。
 * Phase3: Loc-RIBからAdj-RIB-Outに格納するルートを選択し、そのルートをAdj-RIB-Outに格納する。

送信プロセスでは、Adj-RIB-Outの情報からピアにUpdate Messageを送進します。 例えば、以前にUpdate Messageで送信したルートと同じルートは弾きます。Adj-RIB-Outから削除されたルートはピアに削除するようにwithdrawnとして送信する、などのことを行います。

@<img>{starting_point|bgp-fsm}と突き合わせて考えます。Established Stateで、
Established or LocRibChangedイベントが発火されたときに実施されるのがPhase3です。
AdjRibOutChangedイベントが発火されたときに実施されるのが送信プロセスです。
UpdateMsgイベントが発火されたときに実施されるのがPhase1です。
AdjRibInChangedイベントが発火されたときに実施されるのがPhase2です。



== ルーティングテーブルの扱い方

Linuxでルーティングテーブルを読んだり、書いたりするためには、netlinkというものを使用します。
netlinkはKernelとユーザ空間で情報をやり取りするためのものです。
netlinkを生で扱うのは難しいため、本書ではライブラリを使用します。

Cargo.tomlにライブラリ類を追記しましょう。

//emlistnum[Cargo.toml][Rust]{
rtnetlink = "0.9.0"
futures = "0.3.11"
ipnetwork = "0.18.0"
//}

以下のようなコードで、読むことがRoutingTableを読むことが出来ます。
@<code>{lookup_kernel_routing_table}メソッドにてルーティングテーブルを読んでいます。

//emlistnum[src/routing.rs][Rust]{
use std::net::{IpAddr, Ipv4Addr};

use crate::config::Config;
use anyhow::{Context, Result};
use futures::stream::{Next, TryStreamExt};
use ipnetwork::Ipv4Network;
use rtnetlink::{new_connection, Handle, IpVersion};

#[derive(Debug, PartialEq, Eq, Clone)]
struct LocRib;

impl LocRib {
    async fn new(config: &Config) -> Self {
        todo!();
    }

    async fn lookup_kernel_routing_table(
        network_address: Ipv4Network,
    ) -> Result<(Vec<(Ipv4Network)>)> {
        let (connection, handle, _) = new_connection()?;
        tokio::spawn(connection);
        let mut routes = handle.route().get(IpVersion::V4).execute();
        let mut results = vec![];
        while let Some(route) = routes.try_next().await? {
            let destination = if let Some((IpAddr::V4(addr), prefix)) = route.destination_prefix() {
                Ipv4Network::new(addr, prefix)?
            } else {
                continue;
            };

            if destination != network_address {
                continue;
            }

            results.push(destination);
        }
        Ok(results)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tokio::time::{sleep, Duration};

    #[tokio::test]
    async fn loclib_can_lookup_routing_table() {
        // 本テストの値は環境によって異なる。
        // 本実装では開発機, テスト実施機に192.168.1.0/24に属するIPが付与されていることを仮定している。
        let network = Ipv4Network::new("192.168.1.0".parse().unwrap(), 24).unwrap();
        let routes = LocRib::lookup_kernel_routing_table(network).await.unwrap();
        let expected = vec![(network, NextHop::DirectConnected)];
        assert_eq!(routes, expected);
    }
}
//}

== Update Messageの構造
Update Messageの構造は@<href>{https://datatracker.ietf.org/doc/html/rfc4271#section-4.3,RFC 4271 4.3. UPDATE Message Format}@<fn>{update-format}、
Path Attributeの定義は@<href>{https://datatracker.ietf.org/doc/html/rfc4271#section-5.1,RFC 4271 5.1. Path Attribute Usage}@<fn>{path-def}}に記載があります。

本実装ではUpdate MessageのPathAttibuteは、最低限の実装ということで、AS_PATH、ORIGIN, NEXT_HOPのみ実装しています。
本実装ではPath Attributeの値を矛盾なく設定するために、Phase3（LocRib -> AdjRibOutにルートを渡す際）に、PathAttributeの変更も行っています。

 * PathAttribute AS_PATHに自分のAS番号を追加しています。
 * NextHopを自分のlocal ipに変更しています。

//footnote[update-format][https://datatracker.ietf.org/doc/html/rfc4271#section-4.3]
//footnote[path-def][https://datatracker.ietf.org/doc/html/rfc4271#section-5.1]

== Update Messageの送信の実装
== Update Messageの受信の実装
