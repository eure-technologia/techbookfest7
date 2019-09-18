
= Clojure meets SQL! - HugSQL入門

== はじめに

こんにちは、WebフロントエンドエンジニアのBOXP（@If_I_were_boxp）です。

さて、ここ最近は趣味でずっと某Clojure製サービス@<fn>{hitohub}の開発に勤しんでいたのですが、つい先日ようやくリリースすることができました。

そこで今回は、そのサービスで用いたライブラリの一部である「HugSQL@<fn>{hugsql}」を紹介していきます。

//footnote[hitohub][「Hito Hub」と言う名前の、主にVirtual Reality上で活動するアバターをターゲットにしたマッチングサービスです。ClojureScriptで作られたPWAとClojureで書かれたバックエンドで出来ていて、数多くのアバターの中から簡単に自分好みのアバターを探せるようになることを目的に作りました https://speakerdeck.com/boxp/qu-wei-detukuru-abataxabata-matutinguapuriworirisusitahua]
//footnote[hugsql][https://www.hugsql.org/]

== Clojureについて

まず本題に入る前に、そもそもClojureを知らない方もあるかと思うのでこちらから簡単に紹介します。

Clojureは、JavaやKotlinでおなじみのJVMの他、JavaScriptの実行環境や.NET上で動作するマルチプラットフォームなプログラミング言語の一種です。

以下のような特徴を持っています。@<fn>{merits}

=== 不変性

Clojureはいわゆる関数型言語と呼ばれるプログラミング言語の一つであり、Clojureを使ったプログラミングはほとんどの場合不変的なデータ構造と関数の組み合わせのみで完結させることができます。

これにより状態を持つ部分を明確に分離させることができ、並列処理や複雑な状態を抱えるアプリケーションの実装を簡潔に行うことができます。

=== マルチプラットフォーム

ClojureはJVM/JavaScipt/.NET上で動作するマルチプラットフォーム言語であり、一部を除いて全く同じコードをプラットフォームまたぎで共有することが可能です。

また、いざとなればプラットフォームが持つライブラリを呼び出すための機能も十分に持っているためJavaやJavaScriptにできることで出来ないことはほぼないと言えます。

=== 動的型付け

Clojureは動的型付けの言語で、静的型付け言語のようにコンパイル時に型の整合性をチェックするような仕組みはありません。

しかし、TypeScriptなどと同様に漸進的型付けによる型チェックを行うための公式ライブラリであるcore.typed@<fn>{core_typed}が存在するほか、
最近では動的にバリデーションを行う宣言的な仕様（spec）を書いてチェックさせるcore.spec@<fn>{core_spec}などがよく利用されています。

//footnote[core_typed][https://github.com/clojure/core.typed]
//footnote[core_spec][https://github.com/clojure/core.specs.alpha]

=== インタラクティブな開発環境

Clojureを使った一番オーソドックスな開発方法は、新しいコードブロックをREPL上で評価させ、そのまま実行して確認することを繰り返す「REPL駆動開発」になります。

Emacs+ciderなどREPLをエディタと連携させる環境も標準的に使われており、サイクルの早い開発が可能です。

=== Lisp

ClojureはLispの同じ独特な文法（S式）を採用しているLisp方言に属するプログラミング言語です。

この独特な文法によって、コードそのものをデータ構造と同じように組み替える強力なマクロが扱えるようになっています。

また丸括弧が多いためぱっと見とっつきにくい印象を持たれがちですが、その一方で覚える必要がある文法の種類や命令が少ないため習得しやすいと言ったメリットもあります。

=== まとめ

このように、Clojureは関数型言語の特徴を持ちながらも柔軟で開発サイクルが早く、少人数や個人での開発にとても向いている言語だと著者は考えています。
実際に先日著者がリリースしたサービスは開発者一人でも順当に開発を進めることができ、仕事やVRChatに明け暮れながらも最初に決めたリリース日通りにリリースさせることができました。

なおこの章はライブラリの紹介を目的としているため、はじめてClojureに触れる方はこの章を読む前にClojure公式の入門ガイド@<fn>{clojure_biginner_guide}を読んでおくことを強くおすすめ致します。

//footnote[merits][この特徴の説明は著者の経験に基づく主観を多く含むものです。公式な説明はこちらを参照ください https://clojure.org/about/rationale]
//footnote[clojure_biginner_guide][https://japan-clojurians.github.io/clojure-site-ja/guides/guides]

== HugSQLについて

HugSQLはClojureからMySQL/PostgreSQLなどのRelational DatabaseへSQLを発行するためのヘルパーライブラリです。

=== HugSQLの特徴とそのメリット/デメリット

このライブラリのもっとも特徴的な点はSQL文の記述方法で、SQL文を組み立てるためのDSLや関数が存在しません。
以下のような別ファイルとして配置した.sqlファイルを読み込んで実行するスタイルを取っています。

//emlist[HugSQLが読み込む.sqlファイルの例][sql]{
-- :name insert-image :i! :n
-- :doc Imageを追加
insert into image (url)
values (:url)

-- :name update-image-by-id :! :1
-- :doc Imageの更新
update image
set id = :id
    url = :url
where id = :id
//}

このスタイルはもともと、Kris Jenkins氏によって作られたYesql@<fn>{yesql}をインスパイアして採用されたものですが、Yesqlはすでに開発が停止されているために実質的にはこの特徴を持つライブラリはHugSQLのみが選択肢になります。@<fn>{yesql_readme}

//footnote[yesql][https://github.com/krisajenkins/yesql]

YesqlのREADME@<fn>{yesql_rationale}によれば、DSLを用いずにSQLを分けることには以下のメリットがあると述べられています。

 * DSLを挟むことによる認知コストからの脱却
 * SQLへの豊富なエディタサポートによる恩恵
 * SQL解析ツールなどをそのまま使えることによる、職能を超えた相互運用性
 * EXPLAINなどの通常のSQLで使用されるパフォーマンス測定が利用可能になり、パフォーマンスチューニングが簡単化される
 * SQLファイルとしての再利用性の向上

また逆にDSLを利用すべきケースについても述べられており、複数のRelational Databaseへ向けて一つの複雑なクエリを透過的に利用したい場合は抽象化のためにDSLが必要になるのだとしています。

これはあくまで著者の意見ですが、Relational Databaseへの命令を素直にSQLファイルとして管理できる恩恵は無視できるものではなく、先述のデメリットを許容できない場合でない限りHugSQLは常に後悔の少ない選択肢になり得ると思います。

//footnote[yesql_readme][実際、YesqlのREADMEには開発がストップした旨とともに、HugSQLへの誘導が追記されています]
//footnote[yesql_rationale][https://github.com/krisajenkins/yesql#rationale]

== HugSQLの基本

ここからは、HugSQLの実際の利用方法について順を追って解説していきます。

まずHugSQLを利用するためには、HugSQLとHugSQLを各Relational Databaseへ接続するためのドライバーとなるライブラリを依存関係に追加する必要があります。

ビルドツールとしてLeiningen@<fn>{leiningen}を、接続するデータベースをMySQLとしている場合は以下のようにproject.cljへライブラリを追記することで利用できるでしょう。

//footnote[leiningen][https://leiningen.org/]

//emlist[HugSQLに必要な依存ライブラリを追加したproject.clj][clojure]{
...
  :dependencies [
		...
		;; hugsqlのライブラリ本体
                 [com.layerware/hugsql "0.4.9"]

 		 ;; MySQLのドライバーライブラリ
                 [mysql/mysql-connector-java "8.0.17"]

 		 ;; 必須ではないが、時刻データをClojureのデータ構造へ変換するために追記
                 [clj-time "0.15.1"]
		...
                 ]
...
//}

なお、この章では今後MySQLを接続先とした前提で書き進めて行きます。

=== Relational Databaseへの接続

HugSQLを使ってRelational Databaseへの操作を行うためには、JDBC（Java Database Connectivity）@<fn>{jdbc}を使った接続を行う必要があります。

ClojureからJDBCを利用するには、Clojure本体の標準ライブラリとして組み込まれているclojure.java.jdbc@<fn>{jdbc_adapter}が手っ取り早いためこちらを使って接続させましょう。

//footnote[jdbc][https://ja.wikipedia.org/wiki/JDBC]
//footnote[jdbc_adapter][他のjdbcアダプターを利用することもできます。 https://www.hugsql.org/#adapter]

それでは早速HugSQLを接続するための準備をしていきましょう。
HugSQLから実際にRelational Databaseへ操作を行うために必要なコードは、以下のようなhash-map一つで定義できます。

//emlist[Relational Database(MySQL)への接続に必要な定義][clojure]{
(def connection
  {:classname "com.mysql.jdbc.Driver" ;; ロードしたいドライバのクラス名
   :subprotocol "mysql"
   ;; データベースのホスト名,データベース名,接続文字セットの指定
   :subname "//127.0.0.1:3306/my-db?connectionCollation=utf8mb4_bin"
   :user "hoge"
   :password "pa55w0rd"})
//}

あとはHugSQLによるSQLの実行時にこのhash-mapを渡すだけで実行されます。

=== 基本的なSQLの実行方法

先述の通り、HugSQLではSQLを別のファイルへ記述し読み込むことで操作を行います。
SQLファイルは特定のフォルダからの相対パス指定で読み込むため、少なくとも@<code>{resources}フォルダか@<code>{src}フォルダ内のどこかに配置する必要があります。

この章では、@<code>{src}フォルダ直下に@<code>{sql}フォルダを作り、その中にsqlファイルを配置していくものとしましょう。
この時SQLファイルを実行するためにはまず、HugSQLによってSQLを読み込むことで実行のための関数を生成する必要があります。

説明のために、先程例として登場した以下の.sqlファイルを@<code>{sql/image.sql}に配置しましょう。

//emlist[sql/image.sql][sql]{
-- :name insert-image :i! :n
-- :doc Imageを追加
insert into image (url)
values (:url)

-- :name update-image-by-id :! :1
-- :doc Imageの更新
update image
set id = :id
    url = :url
where id = :id
//}

HugSQLに実行のための関数を生成させるには、HugSQLが持つ@<code>{def-db-fns}マクロを使います。

//emlist[src/db.clj][clojure]{
(ns techbookfest.db
  (:require [hugsql.core :refer [def-db-fns]]))

(def-db-fns "techbookfest/sql/image.sql")
//}

@<code>{def-db-fns}マクロにより、これで２つの関数が生成されました。@<code>{techbookfest.db/insert-image}と@<code>{techbookfest.db/update-image-by-id}の2つです。

この2つの関数を呼び出すためには、DBの接続情報とSQLに埋め込まれたパラメータ（:ではじまる文字列）を渡す必要があります。

//emlist[2つの関数の呼び出し例][clojure]{
(insert-image connection {:url "https://dummyimage.com/600x600/000/fff"})

(update-image-by-id connection {:id 1
				:url "https://dummyimage.com/600x600/fff/000"})
//}

これが最も基本的なHugSQLを使ったSQLの実行方法になります。

=== SQLファイルの基本的な記述方法

@<code>{def-db-fns}を使ったSQLの呼び出し中にも登場した通り、HugSQLで読み込むSQLファイルには通常のSQLの記述にプラスして、少し特殊な記述が埋め込まれています。
この特殊な記述について説明していきましょう。

先述も登場した以下のSQLを例に取り、一つ一つ解説していきます。

//emlistnum[sql/image.sql][sql]{
-- :name insert-image :i! :n
-- :doc Imageを追加
insert into image (url)
values (:url)
//}

==== 関数名

SQLの1行目にコメントとして書かれているもののうち、@<code>{:name insert-image}と書かれた部分はこのSQLがHugSQLによって関数化されるときの名前を示しています。

@<code>{:name}の代わりに@<code>{:name-}を用いることで、生成される関数をプライベート関数にすることもできます。

==== コマンド

SQL1行目の関数名のすぐ後ろ、@<code>{:i!}は実行するSQLの種類を指定するための値です。
主に以下の値を指定する事ができます。@<fn>{returning_execute}

//footnote[returning_execute][PostgreSQL限定で指定できる :returning-execute という値がありますが、この章ではMySQLを扱うため省いています。全てのコマンドを見たい場合は公式ドキュメントを参照してください https://www.hugsql.org/#command]

//table[command][コマンドに指定可能な値]{
値	省略形	説明
----------------------------
:query	:?	SELECT文で使用する。@<br>{}この値を設定することで結果の集合を受け取ることができる（デフォルト値）
:execute	:!	UPDATE・CREATE・ALTER文などで使用する。@<br>{}どんな文に対しても指定できる
:insert	:i!	INSERT文で使用する。@<br>{}この値を指定することでAUTO INCREMENTが利用可能になる
//}

また、この値を省略した場合はデフォルト値である@<code>{:query}が設定されます。

==== SQL実行結果の種類

SQL1行目のコマンドのすぐ後ろ、@<code>{:n}はSQLの実行結果の値の種類を指定するための値です。
指定可能な値は以下の通りです。

//table[result][指定可能な実行結果の種類]{
種類	省略形	説明
----------------------------
:one	:1	SELECT文で使用する。@<br>{}1行の結果だけを受け取りたい場合はこの値を指定する
:many	:*	SELECT文で使用する。@<br>{}複数行の結果を受け取りたい時にこの値を指定する
:affected	:n	INSERT・UPDATE・DELETE文などで使用する。@<br>{}影響を受けた行の数が結果として返るようになる
:raw	なし	SQLの実行結果をそのまま返す（デフォルト）
//}

また、この値を省略した場合はデフォルト値の@<code>{:raw}が設定されます。

==== ドキュメント

SQL二行目の@<code>{:doc Imageを追加}は、生成される関数に付与するClojureの@<code>{docstring}を指定します。

自分の書いたSQLの内容をREPLから確認したい時などのために記述しておくと良いでしょう。

==== パラメータ

SQL文の中に紛れている@<code>{:url}は、Clojureから渡してSQL文の中へ値を埋め込むことができるパラメータです。
基本的な型であれば、自動でSQLでのデータ型へ値を変換して埋め込んでくれます。

タプルやリストなど特殊な型の値を渡す方法もありますが、種類が多いためこの章での説明は割愛します。
必要性を感じた時に公式ドキュメント（https://www.hugsql.org/#param-types）を参照ください。

== HugSQLの少し進んだ使い方

ここまでで最低限HugSQLを使うために必要な部分を説明してきました。
最後に、自分がリリースしたサービスで用いられたHugSQLの発展的な部分について説明していきます。

発展的な内容なので、とりあえず触ってみたいのであればこの部分は読み飛ばしてもらってもかまいません。

=== トランザクションの貼り方

HugSQLを使って実行する一連のSQLに対してトランザクションを貼りたい場合は、clojure.java.jdbcの@<code>{with-db-transaction}を利用します。

//emlist[トランザクションを貼った一連のSQL実行の例][clojure]{
(require '[clojure.java.jdbc :as jdbc])

(jdbc/with-db-transaction [tx connection]
  (delete-user_image tx
                     {:user_id 1
                      :image_id 1})
  (insert-user_image tx
                     {:user_id 1
                      :image_id 2}))
//}

@<code>{with-db-transaction}によって作られた@<code>{tx}を接続情報を持つhash-map（@<code>{connnection}）の代わりにHugSQLで生成した関数へ渡す事により、いずれかのSQL実行が失敗したときに@<code>{with-db-transaction}に入る前の状態へrollbackが行われます。

=== sqlvecフォーマット表示によるデバッグ方法

HugSQLはSQLを実際に実行する前に、渡されたパラメータとSQLファイルに基づいて一度@<code>{sqlvec}と呼ばれる形式を挟んでから実行します。
自分が書いたSQLを実行することなく、HugSQLから正しく読み取れる事を確認するためには、この@<code>{sqlvec}の状態のSQLを確認する事が有効である場合があります。

そのためHugSQLにはSQLを実行することなく@<code>{sqlvec}まで変換した状態で返す関数を生成するための@<code>{def-sqlvec-fns}マクロが存在します。

以下は先の節で登場した@<code>{sql/image.sql}をもとに@<code>{def-sqlvec-fns}で関数を生成した例です。

//emlist[def-sql-vec-fnsマクロによる関数の生成][clojure]{
(require '[hugsql.core :refer [def-sqlvec-fns]]])

(def-sqlvec-fns "techbookfest/sql/image.sql")

;; このようなsqlvecが返る↓
;; ["insert into image (url) values (?)", "https://dummyimage.com/600x600/000/fff"]
(insert-image-sqlvec {:url "https://dummyimage.com/600x600/000/fff"})
//}

@<code>{def-sqlvec-fns}を用いて生成した関数の関数名には必ず@<code>{-sqlvec}がappendixとして付き、@<code>{def-db-fns}と名前が衝突することはありません。
必要な時にREPLから実行し、適宜確認するのが良いでしょう。

=== Clojure式によるSQLの変形方法

クエリパラメータによるSQLの変形だけでは不十分な場合、HugSQLではSQLの中にClojureの式を埋め込んでSQLを変形させる機能を持っています。

以下はこの機能を使い、特定の行が持つ一部の列だけを更新するSQLを実現した例です。

//emlist[Clojure式によるSQL変形の例][clojure]{
-- :name update-user-by-id :! :n
-- :doc Userをparamsを元に更新
/* :require [hugsql.parameters :refer [identifier-param-quote]] */
update user
set
id = :id
/*~
(->> (-> params (dissoc :id))
     (map (fn [[key _]]
              (str ","
                   (identifier-param-quote (name key) options)
                   " = :"
                   (name key))))
     (apply str))
~*/
where id = :id
//}

Clojure式はこの例のように復数行の式を埋め込む@<code>{/*~ ~*/}ブロックや、単一行を埋め込む@<code>{--~}接頭詞によってSQLへ埋め込むことができます。

また、@<code>{:require}式によってClojure式で利用する名前空間へのアクセスが可能になっており、ライブラリも問題なく利用できます。

この例のClojure式の場合、渡されたパラメータのうち@<code>{:id}以外のものを全てSQL式の@<code>{SET = ?}へ変換することで特定の列を更新するSQLを実現しています。

== おわりに

この章ではHugSQLの入門編にあたる内容と、さらに少しだけ発展的な内容についても触れました。

SQLファイルを読み込んで実行するこのようなスタイルのヘルパーライブラリはClojureのYesqlやHugSQL以外ではなかなか見かけないように思います。

もし今回のテーマを通して少しでも興味が湧いたのであればぜひ、Clojureと一緒に触ってみてください！

== 参考文献

 * Clojure Rationale: https://clojure.org/about/rationale
 * Clojure Wikipedia: https://ja.wikipedia.org/wiki/Clojure
 * HugSQL: https://www.hugsql.org
