
= いまさら聞けない機械学習APIの作り方 with responder

== はじめに
どうもどうもお久しぶりですこんにちは。ここ最近はGo&pythonバックエンドおじさんで機械学習もぼちぼち仕事でやり続けている@yu81です。Twitterは先にアカウント名をとられていたので@__yu81__ （アンダースコア2個ではさむ）でやってます。
今回は、モダンでイケてるPythonのフレームワークであるresponderを用いて、機械学習モデルをラップした最小のAPIを作ってみましょう。
機械学習モデルをWeb APIを通して提供する仕組みのGetting Started 的なものとして読んでもらえればと思います。

現在ではモデル作成からAPI提供までをシームレスに提供する

Amazon SageMaker （https://aws.amazon.com/jp/sagemaker/）

のようなフルマネージドのものもありますが、自分で作成してみることでバックエンド全般の理解も深まりますし、Web開発の経験がないデータサイエンティストの方などにトライしていただきたいです。

=== 想定する読者

先に述べたように、本稿は初級の内容で、前提知識は多くありません。次のような方を読者として想定しています。

 * Web アプリケーション開発にあまり親しんでいないデータサイエンティスト, データアナリスト

 * 機械学習ライブラリを利用したモデル作成からAPIの提供までの一連の流れに興味があるが、経験がなくイメージが湧かないソフトウェアエンジニア


== responder とは

今回は responder という、2018年末に公開されたWebフレームワークを使用します。（https://python-responder.org）,（https://github.com/taoufik07/responder）

特徴は色々あるのですが、機械学習モデルをホストするAPIとしては、

 * 標準で非同期処理が書きやすい

 * gRPC, WebSocket など、RESTful API 以外のインターフェースを容易に利用できるため、通信レイヤでの高速化の余地がある

 * Flaskと比較して全体的な記法に統一性があるため、フレームワークの習熟コストが高くなく機械学習部分に集中しやすい

などが挙げられます。


=== 環境構築

くどいようですが、モダンなフレームワークであるため、利用可能なPythonのバージョンは3.6以上となっています。
2系もついにEOLになりますし、大人しく最新の3系を利用しましょう。

今世界にある眠りにつかせることの出来ない幾千億の2系のコードのことはきれいさっぱり忘れましょう。これを読んでいる今のあなたには関係のないことです……

responder 公式の README にのっとり、pipenv 経由でインストールします。
Python3 はインストール済みと仮定します。

//cmd{
$ pip install pipenv
$ pipenv install responder
//}

あるいはDockerに親しんでいる方であれば、次のようなDockerfileを用いて実行環境を作ることもできるでしょう。

//cmd{
FROM debian:latest

RUN apt-get update && \
    apt-get install -y python3 python3-pip && \
    pip3 install --no-cache-dir \
    numpy \
    pandas \
    scikit-learn \
    joblib \
    responder && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*
//}

== アプリケーション作成

まずは純粋な Web アプリケーションとして responder を用いて API をスタブレベルで作成してみましょう。今回は完全にInternalな領域に配置するアプリケーションを想定しているので、APIの認証に関する処理は完全に省略しています。

=== 設計

設計というと大袈裟ですが、ディレクトリ構成くらいの話をしましょう。
ごく小さい数個のモデルを提供する程度のAPIを想定していますので、 DDD や クリーンアーキテクチャなどにしっかりのっとった構成にするのは too much 感があります。
オフラインでの学習スクリプト含めたディレクトリ構成はたとえば以下が考えられます。

//list[responder_directory][ディレクトリ構成]{
.
├── predictions
│   ├── __init__.py
│   ├── models
│   │   ├── __init__.py
│   │   ├── {モデルバイナリファイル}
│   │   └── loader.py
│   └── task.py
├── preprocess
│   ├── __init__.py
│   ├── config.py
│   └── task.py
├── server.py
└── trainer
    ├── __init__.py
    ├── config.py
    ├── task.py
    └── util.py
//}

これらを個別に解説すると、次のようになります。

 * server.py はこのWebアプリケーションのエントリポイントとする

 * predictionsには、アプリケーション実行時にはモデルを保持し、推論対象のデータから推論結果を返すためのものを配置する。

 * preprocessには、学習そのものの実行前に必要な前処理に関連するものを配置する

 * trainerには、学習時に必要なコードを配置する

 * modelsには、モデルのバイナリやそれらを読み書きする処理を配置する

=== 実装

まずは純粋な responder の Webアプリケーションとしてのスタブレベルでの実装をしてみましょう。
基本的には公式ドキュメントと同等です。また、ここでは Iris データセットから学習したモデルを返すものを想定した命名としています。

//list[responder_server][responderを用いたコントローラー/ルーティングの実装][Python]{
import responder

api = responder.API()


@api.route("/")
def hello(req, resp):
    resp.text = "Hello world!"


@api.route("/api/prediction/iris")
class PredictionIrisResource:
    async def on_post(self, req, resp):
        r = await req.media()
        resp.media = {"results": [], "resuest": r}


if __name__ == "__main__":
    api.run()
//}

responder は関数ベースとクラスベースのルーティングを併用することが可能です。どちらの場合も @<code>{@api.route(/path/to/resource)} のようなデコレータを用いてルートすることが可能です。
GET以外のHTTPメソッドに対応する、またはひとつのURIに対して異なる HTTP メソッドによるリクエストをルートする場合は、URIに紐付けたクラスのメソッドに @<code>{on_post} のような、つまり @<code>{on_{HTTPメソッド名小文字}} を満たす名称のものを定義すればOKです。

また上記コードのように、ルートされた関数は第1引数にリクエスト、第2引数にレスポンスを受け取るようにします。メソッド終了時の第2引数のインスタンスの状態が実際のレスポンスに反映されます。
jsonとして返却するのであれば resp.media に返却するjsonと同等のdictを代入すればOKです。

responder.API クラスによるWebサーバーですが、この簡潔な記述のものが本番運用にそのまま堪えうるのか？といった疑問が湧く方もいるかもしれません。
実際のところ、この API.run の内部では uvicorn（https://www.uvicorn.org/）とstarlette（https://www.starlette.io/）が使用されています。
これらにより、複数のワーカー化の実装まで行われているため、
システムの要求やネットワーク環境によってはこれでこと足りるケースは少なくないでしょう。

== 機械学習モデルをAPIを通して動作させる

前節でコントローラー層のみのスタブ実装をしたWebサーバーに機械学習モデルとそれを呼び出すコードを加え、動作する機械学習APIを作っていきましょう。

=== データの準備

先に触れましたが、今回はIrisの推論モデルを作成し組み込みます。  
https://scikit-learn.org/stable/auto_examples/datasets/plot_iris_dataset.html

@<code>{sklearn.datasets.load_iris}関数を呼び出すことで、特徴量データ、特徴量の名称、ターゲットとなる分類、分類の名称、データセットの説明が得られます。
特徴量データとターゲットが利用できればよいので、この2つのキーのみ取り出すようにします。

=== アルゴリズム選定

今回は機械学習のAPIを作ることが主眼であるので、こだわりなくRandomForestを用います。
実装はscikit-learnのものを利用します。

本番環境を想定すると、リアルタイム処理に用いられたり、推論対象が多く1件当たりの速度が重要になってくるケースが出てきます。
そのような場合は、精度とスループットのバランスをとる必要が出てくるため、複数のアルゴリズムで精度検証した結果と推論対象1件当たりの実行速度（単体レベル、API呼び出しレベル）を双方考慮の上決定する必要があります。

=== モデルの作成

@<code>{trainer/task.py} に相当する処理を次に示します。

//list[trainer_task_py][trainer/task.py]{
from typing import List

from sklearn.ensemble import RandomForestClassifier
from sklearn.datasets import load_iris
from sklearn.model_selection import train_test_split
from sklearn.metrics import confusion_matrix, f1_score, matthews_corrcoef

import joblib


def fit():
    data = load_iris()
    train_data = data["data"]
    test_data = data["target"]

    train_X, test_X, train_y, test_y = train_test_split(
        train_data, test_data, test_size=0.8
    )
    clf = RandomForestClassifier()
    clf = clf.fit(train_X, train_y)
    joblib.dump(clf, "../models/iris_randomforest.joblib")
    return clf, test_X, test_y


def predict(clf, test_X: List[List[float]], test_y: List[List[float]]):
    predicted_y = clf.predict(test_X)
    return predicted_y


def evaluate(clf, test_y: List[List[float]], predicted_y: List[int]):
    matrix = confusion_matrix(test_y, predicted_y)
    print("confusion matrix: {}".format(matrix.ravel()))

    f1_score_result = f1_score(test_y, predicted_y, average="micro")
    print("F1 score: {}".format(f1_score_result))

    matthwes_c = matthews_corrcoef(test_y, predicted_y)
    print("Matthews correlation coefficient (MCC): {}".format(matthwes_c))


if __name__ == "__main__":
    clf, test_X, test_y = fit()
    predicted_y = predict(clf, test_X, test_y)
    evaluate(clf, test_y, predicted_y)
//}

このファイルでは次のことを行っています。

 * Iris データセットをロードし、学習用データ:テストデータ=1:4に分割してRandomForestで学習を実施

 * 学習済みのRandomForestモデルのインスタンスをjoblibを利用してpickle化し、ファイルに保存

 * テストデータに対して推論を行い、その結果から精度検証をいくつかの指標を用いて実行


=== モデルの永続化

@<list>{trainer_task_py} のコードでは @<code>{joblib} を用いて学習済みのモデルを保存していました。
基本的には、各々のライブラリでモデルの保存と復元を行う機構がある場合はそちらを利用するべきです。しかしscikit-learnにはそれがないため、公式ドキュメントのモデルの永続化の項目にもあるとおり、
学習済みモデルのインスタンスのpickle化（=シリアライズ）を行っています。 https://scikit-learn.org/stable/modules/model_persistence.html

モデルのpickle化による永続化は、いくつかの問題があります。すなわち、  

 * 機械学習ライブラリそのもののバージョンがpickle化/非pickle化環境の両者間で異なることによる動作が保証されない（非pickle化できないなど）

 * pickle API のバージョンミスマッチによる非pickle化の制限

これらに対応するためには、pickle化/非pickle化環境両方のライブラリ、Pythonバージョン、pickle API のバージョンが揃っていればよいので、そのような状態に持って行きやすい手法をライフサイクルに応じて選択すればよいでしょう。  

理想的には、システム的に精度検証を含めて再学習がある程度自動化されているのであれば、学習環境とAPIアプリケーション環境のランタイムを同じタイミングで更新し続けていれば、各々のバージョンをロックせずともほぼ問題は出ないでしょう。

現実問題として、そこまで自動化されていない場合や、アドホックにエンジニアやサイエンティストがモデルを作成しているような場合では、通常のアプリケーションのパッケージマネジメントと同様にすべきです。  
この場合、学習コードとともにその実行環境のDockerfileを含め、必要に応じてチームのAWSやGCPの環境上にあるAmazon ECR や Google Cloud の Container Registry に登録し、開発ルールとしてこれを用いることを徹底してしまうのが楽かも知れません。


=== 精度検証

@<list>{trainer_task_py} では最後に、テストデータを用いた推論結果と正解となるターゲットの分類データから、いくつかの指標を算出しています。

@<code>{evaluate} の最初では混同行列を算出し表示していますが、Irisの分類は3種類のため、3次の正方行列となっています。
2値分類の2次の正方行列と比べるとなじみがありませんが、A,B,C であるという推論結果と正解の3パターンずつの組み合わせになっただけではあります。

一般に精度指標は何を用いるべきかというのが議論になりますが、よく用いられるF1スコアと同時に、マシューズ相関係数も計算しています。
マシューズ相関係数は-1から+1の値をとります。これについての説明は次のURLの記事が非常によくまとまっています。

https://blog.datarobot.com/jp/matthews-correlation-coefficient

2値分類の不均衡なデータの予測において、推論結果が100%多数のクラスだとするモデルは正解率もF1スコアも高く一見よいモデルだが、リコールの値を見ると0になっており役に立たないモデルである。このときマシューズ相関係数は0になり、このモデルがランダムな推論と同等であることを示唆します。



=== APIを介した推論の実行

@<list>{responder_server} のコードを修正し、次のようにします。推論にルートされたクラスのみ修正したのでその部分のみ示します。

//list[responder_server_modified][responderを用いたAPIサーバー実装][Python]{
import responder

from predictions import task as prediction_task

api = responder.API()


@api.route("/api/prediction/iris")
class PredictionIrisResource:
    async def on_post(self, req, resp):
        r = await req.media()
        scores = prediction_task.predict_proba(r["data"])
        results = prediction_task.predict(r["data"])
        resp.media = {
            "score": scores.tolist(),
            "result": results.tolist(),
        }
//}

predictions/task.py には、モデルの推論メソッドをラップする次の実装をしておきます。

//list[predictions_task_py][predictions/task.py][Python]{
from .models import loader
from typing import List

clf = loader.load_model()


def score(test_X: List[List[float]]) -> float:
    return clf.score(test_X)


def predict(test_X: List[List[float]]) -> List[float]:
    return clf.predict(test_X)


def predict_proba(test_X: List[List[float]]) -> List[List[float]]:
    return clf.predict_proba(test_X)


//}

モデルのロードは次のようにしています。

//list[predictions_models_loader_py][predictions/models/loader.py][Python]{
import joblib


def load_model():
    return joblib.load("predictions/models/iris_randomforest.joblib")

//}

以下のコマンドでAPIサーバーを起動します。
@<code>{{python server.py}

起動を確認したら、 @<code>{POST /api/prediction/iris} エンドポイントに対してIrisの特徴量に相当するデータをリクエストしてみましょう。
Irisデータセットからいくつかのレコードをテスト用のリクエストとして実行してみましょう。
//cmd{
curl -XPOST 'http://127.0.0.1:5042/api/prediction/iris' \
-H 'Content-Type: application/json' \
--data '{"data":[[6.4,3.2,4.5,1.5],[5.8,2.7,5.1,1.9],[3.6,3.3,1.4,0.4]]}'
//}

次のようなレスポンスが返ってきます。

//cmd{
{"score": [[0.0058, 0.9828, 0.0114], [0.0002, 0.1155, 0.8843], [0.9998, 0.0002, 0.0]], 
"result": [1, 2, 0]}
//}

resultはIrisの分類は3クラスなので、0,1,2 のうちどのクラスに分類されたかが格納されています。

scoreはリクエストデータが3クラスのうちどれに属するかの確率が、各クラス毎に格納されています。もちろん各々の推論対象データごとにそれらの総和は1となります。

ここまでくれば、特定の学習済みモデルを提供する機械学習APIの正常系は実装済みといえるので、後は必要に応じてエラーハンドリングを追加したり、
関数による構造化レベルからしっかりとクラスベースで処理を整理していくという普通のアプリケーション開発のレベルに落とし込めたといってよいでしょう。

== おわりに

詰め込み版 Getting Started という感じでしたが、いかがでしたでしょうか。

機械学習モデルを作る過程は、データサイエンティストや機械学習エンジニアがさまざまなことに苦心アンド腐心しているものですが、
実際にWebアプリケーションあるいはAPIとして稼働させるというフェーズまで到達し、アプリケーションエンジニアの立場から見れば、所詮はバイナリに格納されたなんらかの計算アルゴリズムを呼び出しているに過ぎないのです。
クラウドの機械学習のAPIは利用したことはあるけど、自分でも独自のモデルを作ってアプリケーションを作ってみるとしたらどうなるだろう、と思っていた人のものづくりの最初の一歩になれば幸いです。
