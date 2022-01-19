= テスト環境の作成
== 作成するテスト環境について
Dockerを用いて@<img>{test-env}のテスト用のネットワークを構築します。

//image[test-env][作成するテスト環境]{
//}


== テスト環境の構築用のファイル追加
@<img>{test-env}の環境を@<code>{docker-compose -f ./tests/docker-compose.yml build --no-cache && docker-compose -f ./tests/docker-compose.yml up -d}の一発で立ち上げられるように@<code>{テストのディレクトリ構造}のファイルを作成します。


//emlistnum[テストのディレクトリ構造][{}]{
.
└── tests
    ├── docker-compose.yml
    ├── host1
    │   └── Dockerfile
    ├── host2
    │   └── Dockerfile
    ├── run_integration_tests.sh
    └── run_unit_tests.sh
//}

それぞれのファイルの中身は以下のとおりです。

//emlistnum[tests/docker-compose][{}]{
services:
  host1:
    cap_add:
      - NET_ADMIN # NET_ADMINがないと、ルーティングテーブルの操作ができない。
    build: # Build Contextを変更して、Dockerfile, docker-compose.ymlより上位にあるファイルをCOPYできるようにしている。
      context: ../
      dockerfile: ./tests/host1/Dockerfile
    networks:
      bgp-test-network:
        ipv4_address: 10.200.100.2
      host1-network:
        ipv4_address: 10.100.210.2
    depends_on:
      - host2 # host2から起動するようにしているのは、現状の実装ではBGPのpassiveモード側から起動しないとネイバーがはれないため。
  host2:
    cap_add:
      - NET_ADMIN # NET_ADMINがないと、ルーティングテーブルの操作ができない。
    build: # Build Contextを変更して、Dockerfile, docker-compose.ymlより上位にあるファイルをCOPYできるようにしている。
      context: ../
      dockerfile: ./tests/host2/Dockerfile
    networks:
      bgp-test-network:
        ipv4_address: 10.200.100.3
      host2-network:
        ipv4_address: 10.100.220.3

networks:
  bgp-test-network: # host1, host2がピアリングするためのネットワーク
    driver: bridge
    ipam:
      config:
        - subnet: 10.200.100.0/24
  host1-network:
    driver: bridge
    ipam:
      config:
        - subnet: 10.100.210.0/24
  host2-network: # host2 -> host1にアドバタイズするネットワーク
    driver: bridge
    ipam:
      config:
        - subnet: 10.100.220.0/24
//}


//emlistnum[tests/host1/Dockerfile][{}]{
FROM rust

WORKDIR /mrbgpdv2
COPY . .
RUN rustup default nightly
RUN cargo build
RUN apt-get update
RUN apt-get -y install iputils-ping
CMD ./target/debug/mrbgpdv2 "64512 10.200.100.2 64513 10.200.100.3 active"
//}

//emlistnum[tests/host2/Dockerfile][{}]{
FROM rust

WORKDIR /mrbgpdv2
COPY . .
RUN rustup default nightly
RUN cargo build
RUN apt-get update
RUN apt-get -y install iputils-ping
CMD ./target/debug/mrbgpdv2 "64513 10.200.100.3 64512 10.200.100.2 passive"
//}

== configをコマンドライン引数から作成するようにmain関数を修正する。
コマンドライン引数によってconfigを渡すことができるようにmain関数を修正します。

//emlistnum[tests/host2/Dockerfile][{}]{
use std::env;

#[tokio::main]
async fn main() {
    let config =
        env::args()
            .skip(1)
            .fold("".to_owned(), |mut acc, s| {
                acc += &(s.to_owned() + " ");
                acc
            });
    let config = config.trim_end();
    let configs = vec![Config::from_str(&config).unwrap()];
}

== 単体テスト、統合テスト用のスクリプトの作成
これらの環境を用いてテストを行うスクリプトを追加します。
以下のように、環境構築、テスト実行をshell scriptにします。

//emlistnum[tests/run_integration_tests.sh][{}]{
#!/bin/bash
docker-compose -f ./tests/docker-compose.yml build --no-cache
docker-compose -f ./tests/docker-compose.yml up -d
HOST_2_LOOPBACK_IP=10.100.220.3
docker-compose -f ./tests/docker-compose.yml exec -T host1 ping -c 5 $HOST_2_LOOPBACK_IP

# docker-compose execの終了コードは実行したコマンド、
# ここでは`ping -c 5 $HOST_2_LOOPBACK_IP`のものである。
# そのため、BGPでルートを交換し、pingが通れば0, それ以外の場合は1である。
TEST_RESULT=$?
if [ $TEST_RESULT -eq 0 ]; then
    printf "\e[32m%s\e[m\n" "統合テストが成功しました。"
else
    printf "\e[31m%s\e[m\n" "統合テストが失敗しました。"
fi

exit $TEST_RESULT
//}


//emlistnum[tests/run_unit_tests.sh][{}]{
#!/bin/bash
docker-compose -f ./tests/docker-compose.yml build --no-cache
docker-compose -f ./tests/docker-compose.yml up -d
docker-compose -f ./tests/docker-compose.yml exec -T host2 cargo test -- --test-threads=1 --nocapture
//}

@<code>{tests/run_integration_tests.sh}を実行することで、以下のテストが実行されます。
bgp-test-networkを用いて、host1, host2はBGP Peerを確立します。host2はhost2-networkの情報をhost1に広報します。
これよりhost1はhost2-networkをルーティングテーブルに追加し、host2-network宛の通信が可能になります。
host1 -> host2-network宛の通信が成功したら統合テスト成功です。

@<code>{tests/run_unit_tests.sh}を実行することで、このDocker上で、@<code>{cargo test}が実行されます。
これはLinuxを使用していれば、ローカルでcargo testを実行するだけで済むため必要ありません。
