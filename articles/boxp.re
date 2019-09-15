
= Clojure meets SQL! - HugSQL入門

== はじめに

こんにちは、WebフロントエンドエンジニアのBOXP(@If_I_were_boxp)です。

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

しかし、TypeScriptなどと同様に漸進的型付けによる型チェックを行うための公式ライブラリであるcore.typed(https://github.com/clojure/core.typed)が存在するほか、
最近では動的にバリデーションを行う宣言的な仕様(spec)を書いてチェックさせるcore.spec(https://github.com/clojure/core.specs.alpha)などがよく利用されています。

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

なおこの章はライブラリの紹介を目的としているため、はじめてClojureに触れる方はこの章を読む前にClojure公式の入門ガイド(https://japan-clojurians.github.io/clojure-site-ja/guides/guides)を読んでおくことを強くおすすめ致します。

//footnote[merits][この特徴の説明は著者の経験に基づく主観を多く含むものです。公式な説明はこちらを参照ください https://clojure.org/about/rationale]

== HugSQLについて

HugSQLはClojureからMySQL/PostgreSQLなどのRelational DatabaseへSQLを発行するためのヘルパーライブラリです。

各種DBへの接続にJDBC(Java Database Connectivity)@<fn>{jdbc}を利用するため、対応する

== HugSQLの基本
=== DBへの接続
=== SQLファイルの記述
=== SQLの実行

== HugSQLの少し進んだ使い方
=== 文字列以外のパラメータ
=== トランザクション
=== sqlvecフォーマット表示によるデバッグ
=== Clojure式

== おわりに

== 参考文献

- Clojure Rationale: https://clojure.org/about/rationale
- Clojure Wikipedia: https://ja.wikipedia.org/wiki/Clojure
