= S3 + Athena + GlueCrawler を使ってAWS WAFのログ分析基盤を構築しよう!

== 自己紹介

こんにちは、SREチームでエンジニアをしている@marnie0301です。
最近はAWS WAFをさわさわする事が多かったので、
今回は、AWS WAFのログの可視化/分析基盤の構築について書いていきたいなと思います。

== AWS WAFとは?

AWSが提供するWAF機能を提供するマネージドサービスです。

WAF(ウェブアプリケーションファイアーウォール)はL7(アプリケーション層)
における攻撃(cf.SQLインジェクション、XSSなど)に対する検知・防御を行うことを
目的としたセキュリティ対策です。

AWS WAFはCondition（検知条件）とRule（検知したものをどうするか？)の二つの要素で構成されており、
まずHTTPヘッダやURI、ペイロード、IPアドレス等のリクエストの情報の条件をConditionに定義し、
単数〜複数のConditionに対してRule(BLOCK,ALLOW,COUNT)を設定することで攻撃と思われる条件にあったリクエストに対して防御をする仕組みです。

AWSWAFのRuleとConditionはデフォルトでは空になっていますので、自前でのルール構築ないし、
マーケットプレイスでセキュリティベンダーが提供するマネージドルール@<fn>{managed_rules}を購入して適用などを行なって
セキュリティ対策をしていく事になります。

* 今回の記事ではAWS WAFのログを可視化にする仕組みを構築する事が主題となりますので、
詳細なルール構築などについては割愛しています。

//footnote[managed_rules][https://aws.amazon.com/marketplace/search/results?x=0&y=0&searchTerms=waf]

== WAFログの分析基盤を作ってみよう。

AWS WAFは標準でCloudWatchと連携していますので、どのルールがどのくらい発生したかというようなメトリクス@<fn>{metrics}を導入後とくに設定などを行わなくてもcloudWatchでメトリクスを確認する事が
できますが、誤検知の検知/調査、意図した通りに攻撃に対して守備できているか？等を日々チューニング・把握しておく必要がある
となってくると、より詳細なログを抽出したり、データを可視化したり、分析してみたくなったりしますよね。(きっと)

AWS WAFのログはKinesisFirehoseをアウトプット先にできますので、firehoseがサポートしているsplunk,Redshiftには
容易にデータを流し込むことができますので、既にそういったサービスを使っているなら、そちらに集約してしまうという
選択肢もあります。
ただRedshiftの運用まではしたくないんだよなぁとか、まだSIEMは検討フェーズ、と言うケースもあると思いますので、
今回は、もう少し気楽に、従量課金+サーバーレスなSQL実行基盤であるAthenaとS3をベースにしたAWS WAFログ
を抽出・可視化できる分析基盤を構築していこうと思います。

//footnote[metrics][https://docs.aws.amazon.com/ja_jp/waf/latest/developerguide/monitoring-cloudwatch.html]

=== 分析基盤は後からでも作れるので、まずは保管からだけでも?

分析や可視化はユースケースありきで育てていくもの+お金がかかる物なので、AWSWAFのダッシュボードで十分でたまに
ログ見る補助があればいいな〜、くらいの分析したいニーズが浮かばないなぁということであれば、
保管だけしておくというだけでも良いと思います。

例えばcloudWatch MetricsとAlertで大まかな傾向や異常値の検知を行い、当該時間のログだけに対して
S3 Selectでのファイル内走査も可能になりますし、s3に保管さえしておけば後付でglue crawlerに食わせてathenaで分析する事も
可能ですので、流量と相談ですが、まず保管してライフサイクルを設定しておくだけでも
メリットになると考えています。


=== 従量課金なので費用にご注意...

次節から紹介する構成はs3,kinesis firehose,lambda,などなどログの流量に依存した
従量課金が発生しますので、流量が多いサービスで試す際は
各コンポーネントの料金を確認した上で自己責任でお願い致します。

==  構成全体像

全体像が見えた方が理解しやすいと思いますので、
まず大雑把な全体像を図示します。

//image[overview][]{
//}

大雑把に言うと以下のような処理の流れになります。

* 1. AWS WAFの標準機能(要設定)でfirehoseにデータを出力

* 2. firehoseからs3にデータを転送(データ変換をする場合はlambdaに転送)

* 3. GlueCrawlerから対象s3Bucketを定期的にクロールしてAthenaで使うGlue データカタログを作成/更新

* 4. AthenaでSQL実行 & QuickSightで可視化

図上は省略していますが、lambdaのエラーによるデータ欠損・その他障害に対しての保険として
originalログをs3に保管しても良いと思います。(お好みで)

次節からは各ステップの設定方法を順次説明していきます。
今回は全体の大雑把な流れの方をメインにしていきますので、
lambdaでの変換部分は省略させていただきます。（ごめんなさい。)

== AWS WAFのログをfirehose経由でs3に保管する

まずはなにはなくともログの保管を行います。
AWS WAFログはWAFを経由した全てのログをKinesis Firehoseを経由して
S3に吐き出す事が可能ですので、まずはログの保管の設定を行います。

=== 1. ログ保存用のS3BucketとkinesisFirehoseに処理用のストリームをあらかじめ作成します。


* 保存用のbucketを作成します。

* データ出力用のストリームを作成します。コンソールからKinesisの画面を開き、DataFirehoseの一覧画面からCreate delivery streamを選択します。

* 名前を入力して、次の画面に進んでください。(注意として、AWS WAFのログ出力先となるストリームはaws-waf-logsから始まる名前である必要があります)

* Process recordsはlambdaの使用やformatを変更しない場合特に設定不要なので、そのまま進みます。

* 作成したsa3Bucketをdestinationに入力し次の画面に進みます。

* compressionにGZIPを選択しIAM roleの項目にあるCreate new or chooseからIAMロール作成画面に進みIAMロールを作成します。

* review画面で設定内容を確認し、作成を完了します。

上記で設定が完了します。

=== 2. AWS WAFのページからログを吐き出したいACLを選択し、LoggingのタブからEnableLoggingをクリック

* 作成したKinesis firehose を選択してください。

* ログ保存時にマスクするデータを選択可能なので、マスクしたい情報を追加します。

//image[kinesis_2][]{
//}

=== 3. Loggingのタブから処理を行うKinesisFirehoseを紐づける。

//image[kinesis_3][]{
//}

=== 4. 完成!

この状態でWAFをかぶせたALB向けに通信を行うと、以下のようにs3にログが自動で作成されていきます。

//image[s3_save][]{
//}

=== 5. terraformでの設定サンプル

以下はterraformで定義する場合のサンプルコードとなります。

//listnum[][terraformによるAWS WAFとログ出力〜保存のサンプル][java]{
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

resource "aws_iam_role_policy_attachment" "firehose_waf_log_s3_access" {
  role       = "${aws_iam_role.waf_log_firehose.name}"
  policy_arn = "${aws_iam_policy.waf_log_s3_access.arn}"
}

resource "aws_iam_policy" "waf_log_s3_access" {
  name = "waf_log_s3_access"

  policy = <<EOF
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "s3:GetObject",
                "s3:PutObject"
            ],
            "Resource": [
                "arn:aws:s3:::sample-waf-log/*"
            ]
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

## firehose config

resource "aws_kinesis_firehose_delivery_stream" "sample_aws_waf_log" {
# aws-waf-logs-で名前を始める必要がある
  name        = "aws-waf-logs-delivery-stream" 
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

==  GlueCrawlerでAthenaで使うGlue データカタログを自動で作成/更新する

前節でS3へのデータ保管までが終わりましたので、Athenaから利用可能にする為に
Glue Crawlerを使ってGlue Data Catalogを生成します。
Glue Crawlerはs3 Bucketを指定するだけでS3内部のデータとディレクトリ構造から
自動でテーブルの作成やパーティションの自動更新などを行ってくれます。

=== GlueCrawlerの設定

AWSコンソール上でクローラを追加します。

* クローラ一覧からクローラを追加を選択

* クローラの名前を入力

* Data storesを選択

* S3を選択し、AWS WAF のログが格納されているs3パスを入力

* 別のデータストアの追加にいいえを選択

* IAMを作成を選択し名前を入力。

* オンデマンドで実行を選択

* 出力先のデータソースを入力、設定オプションで新規列のみ追加。

* データカタログからテーブルとパーティションを削除、を選択。

以上でクローラーが作成されます。

=== terraformレシピ（参考)

terraformで設定する場合は以下の通りです。

//listnum[][terraformでのGlue設定サンプル][java]{
resource "aws_glue_catalog_database" "waf_log" {
  name         = "sample-waflog-db"
}

resource "aws_glue_crawler" "waf_log_crawler" {
  database_name = "${aws_glue_catalog_database.waf_log.name}"
  name = "waf-log-crawler"
  role          = "${aws_iam_role.glue_role.arn}"
  schedule = "cron( * 12 * * ? *)"
  schema_change_policy = {
   delete_behavior = "DELETE_FROM_DATABASE"
  }
  #ログ保管先のbucketを指定 
  s3_target {
    path = "s3://${aws_s3_bucket.waf_log_bucket.bucket}" 
  }
}

resource "aws_iam_policy" "waf_log_s3_access" {
  name ="waf_log_s3_access"
  policy = <<EOF
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "s3:GetObject",
                "s3:PutObject"
            ],
            "Resource": [
                "arn:aws:s3:::sample-waf-log/*"
            ]
        }
    ]
}
EOF
}

resource "aws_iam_role_policy_attachment" "glue_crawler_access" {
  role       = "${aws_iam_role.glue_role.name}"
  policy_arn = "arn:aws:iam::aws:policy/service-role/AWSGlueServiceRole"
}

resource "aws_iam_role_policy_attachment" "glue_waf_log_s3_access" {
  role       = "${aws_iam_role.glue_role.name}"
  policy_arn = "${aws_iam_policy.waf_log_s3_access.arn}"
}

}//

=== Glue Crawlerの実行

一旦、手動で実行してテーブルを作成しましょう。
スケジュール実行の頻度については、お金と必要な鮮度・更新頻度からお好みが良いかと思います。

//image[exec_crawler][]{
//}

しばらく待つとテーブルが作成されます。

//image[glue_table][]{
//}

=== Athenaでクエリを発行しよう！

Athenaでクエリを実行できる準備が整いましたので、簡単なクエリを打ってみましょう！
こんな形でAWS Console上からクエリを発行することができます。

//image[athena][]{
//}

Athenaを利用する際の注意として、パーティションを指定しない場合、
s3上の全データの捜査になる為、コストアップ&検索速度がダウンしてしまいますので
検索クエリを投げる際は必ずパーティションを指定した方が良いです。
クエリの活用例などについてもAWS公式@<fn>{sample_queries}に例がありますので、こちらも参考までに
確認してみてください :)

//footnote[sample_queries][https://docs.aws.amazon.com/ja_jp/athena/latest/ug/waf-logs.html]

== ログの可視化

前節でSQLを利用したログの集計・抽出といった分析が可能となりました。
ちょっとした分析用途だけならこれで充分と言えば充分ですが、折角なので、同じAWSのサービスである
quicksightを使って簡単なグラフ化をしてみましょう。

注記:quicksightは月額課金が発生しますので、価格体系@<fn>{quicksight_cost}を確認の上お試しください。

//footnote[quicksight_cost][https://aws.amazon.com/jp/quicksight/pricing/]

* Quicksightを開く

* 新しい分析を選択

* 新しいデータセットを選択

* データソースにAthenaを選択

//image[quicksight_1][]{
//}

* 適当な名前をつけて作成

//image[quicksight_2][]{
//}

* 分析したいAthenaのテーブルを選択し、カスタムSQLを使用をクリック

//image[quicksight_3][]{
//}

* 例として国別にリクエストの集計を取る簡単なSQLを入力し、クエリの確認をクリック

//image[quicksight_4][]{
//}

* データセットの作成が完了しました。

//image[quicksight_5][]{
//}

* Visualizeを選択して、画面左から円グラフを選択。フィールドウェルのグループにcountry、値にcountをドラッグします。

//image[quicksight_6][]{
//}

無事円グラフ化ができました。
ここから先はquicksightの使い方になってしまうので割愛しますが、同じような手順でクエリをvisualize化できます。

蛇足ですが、外部サービスであるRedashもBackend DataSourceとしてAthenaを指定可能ですので、
既にRedash運用があるようなら、そちらを使って見るのもよいかなと思います。

== まとめ

AWS WAFとAWSコンポーネントの連携でログの抽出、可視化する方法・構成について紹介させて頂きました。
分析と可視化をどのレベルまで実現するかは、あくまで実運用とセットだと思いますが
まずはログを保管して、有事に見える状態を整えておく・手段として持っておくだけでもだいぶ安心感が出るかな〜と思いますので
ぜひ試してみてください！
