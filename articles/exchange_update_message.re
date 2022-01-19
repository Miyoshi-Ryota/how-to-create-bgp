
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
netlinkを生で扱うのは難しいため、本PDFではライブラリを使用します。

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
Path Attributeの定義は@<href>{https://datatracker.ietf.org/doc/html/rfc4271#section-5.1,RFC 4271 5.1. Path Attribute Usage}@<fn>{path-def}に記載があります。

本実装ではUpdate MessageのPathAttibuteは、最低限の実装ということで、AS_PATH、ORIGIN, NEXT_HOPのみ実装しています。
本実装ではPath Attributeの値を矛盾なく設定するために、Phase3（LocRib -> AdjRibOutにルートを渡す際）に、以下のようにPathAttributeの変更も行っています。

 * PathAttribute AS_PATHに自分のAS番号を追加しています。
 * NextHopを自分のlocal ipに変更しています。

//footnote[update-format][https://datatracker.ietf.org/doc/html/rfc4271#section-4.3]
//footnote[path-def][https://datatracker.ietf.org/doc/html/rfc4271#section-5.1]

== Update Messageの送信の実装
広報するネットワークを指定する方法を実装します。一般的にnetworkコマンドで設定されるものです。
今回はConfigに追加し、コマンドライン引数で渡すことにしましょう。

//emlistnum[src/config.rs][Rust]{
#[derive(PartialEq, Eq, Debug, Clone, Hash, PartialOrd, Ord)]
pub struct Config {
    pub networks: Vec<Ipv4Network>,
}
//}

また、テスト時に広報するネットワークを指定するようにします。
@<chap>[integration_tests]で説明したようにhost2 -> host1にhost2-networkを広報するように指定します。

//emlistnum[tests/host2/Dockerfile][{}]{
CMD ./target/debug/mrbgpdv2 "64513 10.200.100.3 64512 10.200.100.2 passive 10.100.220.0/24"
//}

この変更により、あとはうまくUpdate Messageの送受信が完了すれば、統合テストが通るようになります。
Update Messageの送信自体は、Open Messageの送信と変わらないため書内では省略します。この実装を確認したい場合は、本実装のリポジトリの以下のPRを確認してください。

 * @<href>{https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/11,Update messageを送信可能にする #11}@<fn>{update-11}
 * @<href>{https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/13,Update messageの送信に関するバグを修正した。 #13}@<fn>{update-13}
 * @<href>{https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/14,UpdateMessageにNLRIが欠落するバグの修正 #14}@<fn>{update-14}

//footnote[update-11][https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/11]
//footnote[update-13][https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/13]
//footnote[update-14][https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/14]

== Update Messageの受信の実装
Update Messageは受信した内容に従って、ルーティングテーブルの書き込みを行います。ルーティングテーブルの書き込みは本PDF内では初なので、以下にコードを記載します。 

//emlistnum[src/routing.rs][Rust]{
impl LocRib {
    pub async fn write_to_kernel_routing_table(&self) -> Result<()> {
        let (connection, handle, _) = new_connection()?;
        tokio::spawn(connection);
        for e in &self.0 {
            for p in &e.path_attributes {
                if let PathAttribute::NextHop(gateway) = p {
                    let dest = e.network_address;
                    handle
                        .route()
                        .add()
                        .v4()
                        .destination_prefix(dest.ip(), dest.prefix())
                        .gateway(*gateway)
                        .execute()
                        .await?;
                    break;
                }
            }
        }
        Ok(())
    }
}
//}

その他のUpdate Messageの受信もOpen Messageの受信と大差ないため概ね省略します。
すべてのコードを確認したい場合は、以下のPRを参照してください。

 * @<href>{https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/15,Update messageを受信可能にする #15}@<fn>{update-received}

//footnote[update-received][https://github.com/Miyoshi-Ryota/mrbgpdv2/pull/15]

最後に@<code>{tests/run_integration_tests.sh}を実行して、統合テストが通るようになったことを確認しましょう。

//emlistnum[統合テストの実行][{}]{
mrcsce@pop-os:~/programming/rustProjects/bgp/mrbgpdv2$ ./tests/run_integration_tests.sh 
<中略>
Successfully built 09fb776025aa
Successfully tagged tests_host1:latest
Recreating tests_host2_1 ... done
Recreating tests_host1_1 ... done
PING 10.100.220.3 (10.100.220.3) 56(84) bytes of data.
64 bytes from 10.100.220.3: icmp_seq=1 ttl=64 time=0.101 ms
64 bytes from 10.100.220.3: icmp_seq=2 ttl=64 time=0.051 ms
64 bytes from 10.100.220.3: icmp_seq=3 ttl=64 time=0.059 ms
64 bytes from 10.100.220.3: icmp_seq=4 ttl=64 time=0.062 ms
64 bytes from 10.100.220.3: icmp_seq=5 ttl=64 time=0.065 ms

--- 10.100.220.3 ping statistics ---
5 packets transmitted, 5 received, 0% packet loss, time 4102ms
rtt min/avg/max/mdev = 0.051/0.067/0.101/0.017 ms
統合テストが成功しました。
mrcsce@pop-os:~/programming/rustProjects/bgp/mrbgpdv2$ git status
//}

これにて本PDFの内容は終了です。

ここまで実装した方は、その実装に肉付けすることで、異常系や実装していないPathAttributeの実装も行っていけるはずです。
RFCにも慣れ、その他のプロトコルもRFCから実装できるようになっていることと思います。

おつかれさまでした。
