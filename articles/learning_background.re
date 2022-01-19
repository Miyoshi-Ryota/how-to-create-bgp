
= BGPの実装に必要な知識の学習
== BGPとは、を学ぶ参考リンク紹介
BGPとはなにか、ということについてはすでにインターネット上に良い資料が存在します。そのため、本PDFではBGPとはなにか、ということについて記載しません。
良い資料を2つ紹介します。 BGPについて詳しくない方は、どちらかを読み、本章以降に進んでください。 すべての内容を記憶する必要はありません。 なんとなく見ておくと、本章以降の理解が容易になります。

@<href>{https://www.infraexpert.com/study/study60.html,ネットワークエンジニアとして - BGPの技術}@<fn>{ネットワークエンジニアとして - BGPの技術}の次の章が参考になります。

//footnote[ネットワークエンジニアとして - BGPの技術][https://www.infraexpert.com/study/study60.html]

 * @<href>{https://www.infraexpert.com/study/bgpz01.html,BGP（ Border Gateway Protocol ）とは}
 * @<href>{https://www.infraexpert.com/study/bgpz02.html,BGP - 4つのメッセージ、6つのステータスと状態遷移}
 * @<href>{https://www.infraexpert.com/study/bgpz03.html,BGP - IBGPとEBGPの違い}
 * @<href>{https://www.infraexpert.com/study/bgpz04.html,BGP - スタブAS、トランジットAS、非トランジットAS}
 * @<href>{https://www.infraexpert.com/study/bgpz05.html,BGP - パスアトリビュート（ パス属性 ）＆ ベストパス選択}
 * @<href>{https://www.infraexpert.com/study/bgpz06.html,BGP - コンフィグ設定 - 基本設定}

@<href>{http://www5e.biglobe.ne.jp/aji/30min/index.html,30分間ネットワーキング}@<fn>{30分間ネットワーキング}の次の章が参考になります。

//footnote[30分間ネットワーキング][http://www5e.biglobe.ne.jp/aji/30min/index.html]
 * @<href>{http://www5e.biglobe.ne.jp/aji/30min/16.html,第16回 BGP4(1) AS}
 * @<href>{http://www5e.biglobe.ne.jp/aji/30min/17.html,第17回 BGP4(2) BGPピア}
 * @<href>{http://www5e.biglobe.ne.jp/aji/30min/18.html,第18回 BGP4(3) IBGPとEBGP}
 * @<href>{http://www5e.biglobe.ne.jp/aji/30min/19.html,第19回 BGP4(4) パスアトリビュート}
 * @<href>{http://www5e.biglobe.ne.jp/aji/30min/20.html,第20回 BGP4(5) ベストパス選択}

== BGPはイベント駆動ステートマシンである
== イベント駆動ステートマシンとは
イベント駆動ステートマシンとは、現在の状態（ステート）と入力（イベント）によって動作が決定するモノのモデルです。

例として、テレビをステートマシンとして表現します。 テレビの状態として、1. 電源ON、2. 電源OFFの2状態が存在し、入力としてa. 電源ボタンの押下、b. 音量増加ボタンの押下、c. 音量減少ボタンの押下の3つが存在するとします。本来のテレビはもっと多数の状態やイベントを持っていますが、ここでは例示のためにシンプルにしています。

テレビの状態が1. 電源OFFのときにa. 電源ボタンの押下が発生した場合はテレビの状態が2. 電源ONに遷移します。テレビの状態が1. 電源OFFのときに、b. 音量増加ボタンの押下、c. 音量減少ボタンの押下が発生した場合はテレビの状態は1. 電源OFFのままで何も起こりません。

テレビの状態が2. 電源ONのときにa. 電源ボタンの押下が発生した場合はテレビの状態が1. 電源OFFに遷移します。テレビの状態が2. 電源ONのときb. 音量増加ボタンの押下、c. 音量減少ボタンの押下が発生した場合は、テレビの状態は2. 電源ONのまま音量が増減します。
これを図示すると@<img>{tv_state_machine}になります。

テレビは現在の状態と、入力によって動作が決定するモノとして表現することが可能です。

このような現在の状態（ステート）と入力（イベント）によって動作が決定するモノのモデルをイベント駆動ステートマシンといいます。

//image[tv_state_machine][テレビのステートマシン図]{
//}

== イベント駆動ステートマシンの実装例
イベント駆動ステートマシンをどのよう実装すればいいのかということを学ぶため
@<hd>{イベント駆動ステートマシンとは}で例示したテレビをコードにします。

//emlistnum[イベント駆動ステートマシン, テレビの実装例][Rust]{
use rand::Rng;
use std::collections::VecDeque;
use std::thread;
use std::time::Duration;

#[derive(Debug)]
enum State {
    PowerOn,
    PowerOff,
}

#[derive(Debug)]
enum Event {
    PushedPowerButton,
    PushedVolumeIncreaseButton,
    PushedVolumeDecreaseButton,
}

struct TV {
    now_state: State,
    event_queue: EventQueue,
    volume: u8,
}

impl TV {
    pub fn new() -> Self {
        let now_state = State::PowerOff;
        let event_queue = EventQueue::new();
        let volume = 10;
        Self {
            now_state,
            event_queue,
            volume,
        }
    }

    pub fn be_pushed_power_button(&mut self) {
        self.event_queue.enqueue(Event::PushedPowerButton);
    }

    pub fn be_pushed_volume_increase_button(&mut self) {
        self.event_queue.enqueue(Event::PushedVolumeIncreaseButton);
    }

    pub fn be_pushed_volume_decrease_button(&mut self) {
        self.event_queue.enqueue(Event::PushedVolumeDecreaseButton);
    }

    pub fn handle_event(&mut self, event: Event) {
        match &self.now_state {
            &State::PowerOn => match event {
                Event::PushedPowerButton => {
                    self.now_state = State::PowerOff;
                }
                Event::PushedVolumeIncreaseButton => {
                    self.volume += 1;
                }
                Event::PushedVolumeDecreaseButton => {
                    self.volume -= 1;
                }
            },
            &State::PowerOff => match event {
                Event::PushedPowerButton => {
                    self.now_state = State::PowerOn;
                }
                _ => (),
            },
        }
    }
}

struct EventQueue(VecDeque<Event>);

impl EventQueue {
    pub fn new() -> Self {
        let d = VecDeque::new();
        EventQueue(d)
    }

    pub fn dequeue(&mut self) -> Option<Event> {
        self.0.pop_front()
    }

    pub fn enqueue(&mut self, event: Event) {
        self.0.push_back(event);
    }
}

fn push_random_button_of_tv(tv: &mut TV) {
    let mut rng = rand::thread_rng();
    match rng.gen_range(0..4) {
        1 => tv.be_pushed_power_button(),
        2 => tv.be_pushed_volume_increase_button(),
        3 => tv.be_pushed_volume_decrease_button(),
        _ => (),
    };
}

fn main() {
    let mut tv = TV::new();
    tv.be_pushed_power_button();
    loop {
        push_random_button_of_tv(&mut tv);
        if let Some(event) = tv.event_queue.dequeue() {
            println!(
                "tv information: {{ now_state={:?}, volume={} }}\ninput_event: {:?}",
                tv.now_state, tv.volume, event
            );
            tv.handle_event(event);
        }
        thread::sleep(Duration::from_secs(2));
    }
}
//}

103行目でTVのランダムなボタンを押下し、TVにEvent（入力）を送信しています。送信されたEventはイベントキュー、tv.event_queueにエンキューします。 104行目でイベントキューに保存されているEventを取り出します。 TVの現在の状態（State）はTVのインスタンスに保存されています。 109行目でEventを扱います。 49行目〜69行目を見ると分かるように、`tv.handle_event(event)`はEventとtvインスタンスに保存されている現在の状態に応じて、動作し次の状態を決定します。 それはイベント駆動ステートマシン、そのものでした。このようにしてイベント駆動ステートマシンを実装することができました。

@<code>{TV_実行時のログ}が実行時のログです。

ログの4行目を見ると、電源OFFの状態であることがわかります。 次にログの5行目を見ると、電源ボタンが押されたことがわかります。 次にログの6行目を見ると、電源ONの状態に遷移したことがわかります。 次にログの7行目を見ると、音量増加ボタンが押されたことがわかります。 次にログの8行目を見ると、電源ON状態のまま、音量が11に増加していることがわかります。

一方でログの16、17、18行目を見ると、電源OFF状態のときに音量増加ボタンが押されても、電源OFF状態のままで音量の変動もないことがわかります。

//emlistnum[TV_実行時のログ][{}]{
mrcsce@pop-os:~/programming/rustProjects/samplecode$ cargo run
    Finished dev [unoptimized + debuginfo] target(s) in 0.00s
     Running `target/debug/samplecode`
tv information: { now_state=PowerOff, volume=10 }
input_event: PushedPowerButton
tv information: { now_state=PowerOn, volume=10 }
input_event: PushedVolumeIncreaseButton
tv information: { now_state=PowerOn, volume=11 }
input_event: PushedVolumeDecreaseButton
tv information: { now_state=PowerOn, volume=10 }
input_event: PushedVolumeIncreaseButton
tv information: { now_state=PowerOn, volume=11 }
input_event: PushedVolumeDecreaseButton
tv information: { now_state=PowerOn, volume=10 }
input_event: PushedPowerButton
tv information: { now_state=PowerOff, volume=10 }
input_event: PushedVolumeIncreaseButton
tv information: { now_state=PowerOff, volume=10 }
input_event: PushedPowerButton
tv information: { now_state=PowerOn, volume=10 }
input_event: PushedVolumeIncreaseButton
tv information: { now_state=PowerOn, volume=11 }
input_event: PushedVolumeIncreaseButton
^C
mrcsce@pop-os:~/programming/rustProjects/samplecode$
//}

これは@<hd>{イベント駆動ステートマシンとは}の章で例示した通りの動作です。 例示したイベント駆動ステートマシンを実装できていることが確かめられました。 BGPのステートマシンも前述の@<code>{イベント駆動ステートマシン, テレビの実装例}に似た方針で実装していきます。

