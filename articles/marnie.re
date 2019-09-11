= S3 + Athena + GlueCrawler を使ってAWS WAFのログの分析基盤を作ってみよう！

== 自己紹介

こんにちは、SREチームでエンジニアをしている@marnie0301です。
最近はAWS WAFをさわさわする事が多かったので、
今回は、AWS WAFのログの可視化、アラート連携について書いていきたいなと思います。

== AWS WAFとは?

AWSが提供するWAF機能を提供するマネージドサービスです。

WAF(ウェブアプリケーションファイアーウォール)はL7(アプリケーション層)
における攻撃(cf.SQLインジェクション、XSSなど)に対する検知・防御を行うことを
目的としたセキュリティ対策です。

AWS WAFはCondition（検知条件）とRule（検知したものをどうするか？)の二つの要素で構成されており、
まずHTTPヘッダやURI、ペイロード、IPアドレス等のリクエストの情報の条件をConditionに定義し、
単数〜複数のConditionに対してRule(BLOCK or COUNT)を設定することで攻撃と思われる条件にあったリクエストに対して防御をする仕組みです。

AWSWAFのRuleとConditionはデフォルトでは空になっていますので、自前でのルール構築ないし、
マーケットプレイスでセキュリティベンダーが提供するマネージドルールを購入して適用などを行なって
セキュリティ対策をしていく事になります。

* 今回の記事ではAWS WAFのログを可視化にする仕組みを構築する事が主題となりますので、
詳細なルール構築などについては割愛しています。

TODO: マネージドルールの購入URLなど

== WAFログの分析基盤を作ってみよう。

AWS WAFは経由する全てのリクエストに対して、条件に一致さえすれば
実際に攻撃リクエストだったかどうかはお構いなくアクションを執行しますので、
条件設定の不備やアプリケーションの作りによっては、攻撃ではない正常なリクエスト
についても遮断してしまいます。

このような誤検知の検知や意図した通りに攻撃に対して守備できているか？等を日々チューニング・把握しておく必要がある
となってくると、詳細なログを抽出したり、データを可視化したり、分析してみたくなったりしますよね。

AWS WAFのログはKinesisFirehoseをアウトプット先にできますので、firehoseがサポートしているsplunk,Redshiftには
容易にデータを流し込むことができますので、既にそういったサービスを使っているなら、そちらに集約してしまうという
選択肢もあります。

Redshiftの運用まではしたくないんだよなぁとか、まだSIEMは検討フェーズ、と言うケースもあると思いますので、
今回は、もう少し気楽にサーバーレスなクエリ実行基盤であるAthenaとS3をベースにして
AWS WAFのログをAWS技術スタック内で抽出・可視化できるような構成を構築していこうと思います。

=== 保管しておくだけでも後から使える

分析や可視化はユースケースありきで育てていくもの+お金がかかる物なので、AWSWAFのダッシュボードで十分でたまに
ログ見る補助があればいいな〜、くらいの分析したいニーズが浮かばないなぁということであれば、
必要な時だけs3に保管する部分だけ設定を行い、S3 Selectでファイル内走査で最低限というのも有りだと思います。
また保管さえしておけば後付でglue crawlerに食わせてathenaで分析する事も可能なので、
例えば１週間だけ保管するようにする、などからスタートしておく事も可能です。

TODO:S3 selectの画像

=== 費用に注意...

次節から紹介する構成はs3,kinesis firehose,lambda,などなどログの流量に依存した
従量課金が発生しますので、流量が多いサービスで試す際は
各コンポーネントの料金を確認した上で自己責任でお願い致します。

===  構成全体像

全体像が見えた方が理解しやすいと思いますので、
まず大雑把な全体像を図示します。

TODO:図

大雑把に言うと以下のような処理の流れになります。

1. AWS WAFの標準機能(要設定)でfirehoseにデータを出力
2. firehoseからs3にデータを転送(データ変換をする場合はlambdaに転送)
3. GlueCrawlerから対象s3Bucketを定期的にクロールしてAthenaで使うGlue データカタログを作成/更新
4. QuickSightで可視化

customLog保管側のデータ欠損・トラブルに対しての保険としてoriginalログをs3に保管していますが
この辺はお好みで。

次節から各ステップの設定方法を順次説明していきます。

== AWS WAFのログをs3に保管する

まずはなにはなくともログの保管を行います。
AWS WAFログはWAFを経由した全てのログをKinesis Firehoseを経由して
S3に吐き出す事が可能ですので、まずはログの保管の設定を行います。

GUIから設定する場合は

* ログ保存用のS3BucketとkinesisFirehoseに処理用のストリームをあらかじめ作成。
* AWS WAFのページからログを吐き出したいACLを選択。
* Loggingのタブから処理を行うKinesisFirehoseを紐づける。

という操作で設定が可能です。
以下はterraformで定義する場合のサンプルコードとなります。

//listnum[][terraformによるAWS WAFとログの吐き出しのサンプル][java]{
```
## waf config
resource "aws_wafregional_web_acl" "sample_alb_acl" {
  name = "sample-alb-web-acl"
  metric_name = "sample"

  default_action {
    type = "ALLOW"
  }

  logging_configuration {
    log_destination = "${aws_kinesis_firehose_delivery_stream.sample_waf_log.arn}"
    # tokenなどセキュリティ上、保管したくない値を定義するとmaskして保存してくれる
    redacted_fields {
      field_to_match {
        data = "X-USER-SESSION-TOKEN"
        type = "HEADER"
      }
    }
  }

  depends_on = [
    "aws_kinesis_firehose_delivery_stream.sample_waf_log",
  ]

}

resource "aws_wafregional_web_acl_association" "app_sample_acl_association" {
  resource_arn = "${aws_lb.sample_alb.arn}" # WAFをかぶせたいALBのarn
  web_acl_id   = "${aws_wafregional_web_acl.sample_alb_acl.id}"
}

## firehose IAM

resource "aws_iam_role" "waf_log_firehose" {
  name = "waf_log_firehose"

  assume_role_policy = <<EOF
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Action": "sts:AssumeRole",
      "Principal": {
        "Service": "firehose.amazonaws.com"
      },
      "Effect": "Allow",
      "Sid": ""
    }
  ]
}
EOF
}

## log bucket
resource "aws_s3_bucket" "sample_waf_log_bucket" {
  bucket = "sample-waf-log"
  acl    = "private"
}

resource "aws_s3_bucket_policy" "waf_log_bucket_policy" {
  bucket = "${aws_s3_bucket.sample_waf_log_bucket.id}"

  policy = <<POLICY
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Sid": "AddPerm",
            "Effect": "Allow",
            "Principal": {
                "AWS": "arn:aws:iam::********:role/waf_log_firehose"
            },
            "Action": "s3:*",
            "Resource": "arn:aws:s3:::/*" //TODO:さすがに*は恥ずかしい
        }
    ]
}
POLICY
  depends_on = [
    "aws_s3_bucket.sample_waf_log_bucket",
    "aws_iam_role.waf_log_firehose",
  ]
}

## firehose config

resource "aws_kinesis_firehose_delivery_stream" "sample_aws_waf_log" {
  name        = "aws-waf-logs-delivery-stream" # aws-waf-logs-で名前を始める必要がある
  destination = "s3"

  s3_configuration {
    role_arn   = "${aws_iam_role.waf_log_firehose.arn}"
    bucket_arn = "${aws_s3_bucket.sample_waf_log_bucket.arn}"
    prefix     = "waf_log/"
  }

  depends_on = [
    "aws_s3_bucket.sample_waf_log_bucket",
    "aws_iam_role.waf_log_firehose"
  ]
}

//}

この状態でWAFをかぶせたALB向けに通信を行うと、以下のようにs3にログが自動で作成されていきます。

TODO: がぞうをのせる

ここまでの手順ログが保管できましたので、次節からログの可視化/分析を容易にするために
AWSAthena
手順を説明していきます。

== データを変換したい場合の話をここに書く

lambdaでsourceIP吐き出すとか、もろもろ。

== Glueの話を書く

hive形式にふれるか、partitionの話とかかな。

ここにアテナでクエリうってうまーーーな感じの画像でも載せるか

== ログの可視化

quicksightとかログの投げ方とかを書く

AWSにおけるAthenaの説明いれる？
パーティションしないとathenaが遅い+高い

TODO: アテナ,Glue,quickInsights

== アラーティング

TODO: cloudwatchで楽できないの？とかlambdaでやる？とか


== まとめ

AWS WAFとAWSコンポーネントの連携でログの抽出、可視化する方法・構成について紹介させて頂きました。
ただ分析と可視化をどのレベルまで実現するかは、あくまで実運用とセットだと思いますので、
まずはログを保管して、有事に見える状態になっているだけでもだいぶ安心感が出るかな〜と思います。
