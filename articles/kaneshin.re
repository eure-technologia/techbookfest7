= Run-time Initialization in Go

== はじめに

株式会社エウレカのkaneshin@<fn>{kaneshin}です。エウレカでは技術戦略やエンジニア組織のマネジメントについて策定や執行をしつつ、Goをメインとしたツールやアプリケーションを開発しています。

//footnote[kaneshin][@<href>{https://twitter.com/kaneshin0120}]

本章では、Goでコーディングをするときに知っておくと役立つ実行時の初期化について紹介します。

== 実行時の初期化

Goのスコープにはファイル単位でのスコープは存在せずパッケージのスコープで変数や関数が参照される仕様です。
実行時に変数を初期化してことによってランタイムの処理時間を効率化することが可能になりますが、@<code>{main}関数が実行されるまで処理時間が長くなってしまうことや、メモリのスコープが関数ではなくパッケージスコープとなることによって変数を参照するのに時間が掛かってしまうことがあります。
デメリットを理解した上で初期化をすることは効率性を向上させるために重要になります。

== パッケージスコープの変数宣言

パッケージスコープに変数を宣言させることは良いことなのか？
Goは並行処理される言語のためパッケージスコープの変数は基本的にシングルトンとなります。
@<code>{goroutine}間での処理を意識しながら開発と実行をしなくてはならなくなるため、密結合の温床であり依存性の高いコードとなるのであまり推奨されません。

しかし、パッケージスコープの変数は場合によって必要不可欠です。そのためパッケージスコープの変数を宣言するときは設計として意味を定義しておくことが重要です。

=== エラーの変数宣言

エラーは値です。エラーをハンドリングするためには値として宣言された変数で検証をすることがエラーの適切な扱いへの一歩となります。

//list[init_var_error][strings.Readerのエラーハンドリング][go]{
package main

import (
  "fmt"
  "io"
  "strings"
)

func main() {
  r := strings.NewReader("Hello")
  b := make([]byte, 8)
  for {
    n, err := r.Read(b)
    fmt.Printf("n = %v err = %v b = %v\n", n, err, b)
    if err == io.EOF {
      break
    }
  }
}
//}

//cmd{
$ go run main.go
n = 5 err = <nil> b = [72 101 108 108 111 0 0 0]
n = 0 err = EOF b = [72 101 108 108 111 0 0 0]
//}

これは@<code>{io}パッケージに宣言されている@<code>{io.EOF}でエラーをハンドリングしている例です。
@<code>{io}パッケージには@<code>{EOF}以外にも複数のエラーをパッケージスコープとして宣言しています。

//list[io_init_var_errors][ioパッケージのエラー][go]{
package io

import "errors"

var ErrShortWrite = errors.New("short write")
var ErrShortBuffer = errors.New("short buffer")
var EOF = errors.New("EOF")
var ErrUnexpectedEOF = errors.New("unexpected EOF")
var ErrNoProgress = errors.New("multiple Read calls return no data or error")
//}

パッケージスコープのエラーは定式的に@<code>{ErrNoProgress}のように@<code>{Err}をプレフィックスとすることが多いです。

このようにハンドリングする必要のあるエラーは関数内で生成して返却するのではなく、パッケージスコープとして宣言したものを返却することでパッケージの外からハンドリングすることが可能になります。

=== 高コストな処理を初期化する

Goの@<code>{regexp}パッケージは正規表現を扱うパッケージです。常に改善されているパッケージですが、関数内で何度も変数として初期化される場合は初期化のコストを実行直後に寄せてしまうことも可能です。

//list[regexp_benchmark][regexpパッケージのベンチマーク][go]{
package main

import (
  "regexp"
  "testing"
)

func BenchmarkRegexpMustCompile1(b *testing.B) {
  b.ResetTimer()
  for i := 0; i < b.N; i++ {
    re0 := regexp.MustCompile("[a-z]{3}")
    _ = re0.FindAllString(`Lorem ipsum dolor sit amet, consectetur..`, -1)
  }
}

var re = regexp.MustCompile("[a-z]{3}")

func BenchmarkRegexpMustCompile2(b *testing.B) {
  b.ResetTimer()
  for i := 0; i < b.N; i++ {
    _ = re.FindAllString(`Lorem ipsum dolor sit amet, consectetur..`, -1)
  }
}
//}

//cmd{
$ go test -bench . -benchmem
BenchmarkRegexpMustCompile1-16            495222              2413 ns/op            1618 B/op         23 allocs/op
BenchmarkRegexpMustCompile2-16            838984              1391 ns/op             289 B/op          9 allocs/op
//}

これは@<code>{regexp}を関数呼び出し時に生成するか、実行時に変数を初期化してしまうかをシミュレートしたベンチマークです。結果は実行時に初期化する方が2倍高速に処理ができます。このように@<code>{regexp}は正規表現が動的に変化しない場合はパッケージスコープの変数として宣言してしまうことで効率的に処理ができます。

@<code>{regexp}は内部処理ではスレッドセーフに設計されていることや検索結果をキャッシュしているため、実際にコーディングする場合は下記のように記述することを推奨します。

//list[regexp_practice][regexpのプラクティス][go]{
package main

import (
  "fmt"
  "regexp"
)

var re = regexp.MustCompile("[a-z]{3}")

func FindString(str string) []string {
  re := re.Copy()
  return re.FindAllString(str, -1)
}

func main() {
  fmt.Println(FindString("Hello, world"))
}
//}

//cmd{
$ go run main.
[ell wor]
//}

また、パッケージ変数として宣言している@<code>{re}変数を上書きされたくない場合は初期化時に関数実行をして隠蔽することもできます。

//list[regexp_practice_closure][regexpのプラクティス][go]{
var FindString = func() func(string) []string {
  re := regexp.MustCompile("[a-z]{3}")
  return func(str string) []string {
    re := re.Copy()
    return re.FindAllString(str, -1)
  }
}()

func main() {
  fmt.Println(FindString("Hello, world"))
}
//}

//cmd{
$ go run main.
[ell wor]
//}

このように宣言することによって、パッケージスコープの変数を減らすことができます。また、@<code>{re}変数が関数スコープになるためメモリアクセスも効率化されています。

=== その他

他にも様々な条件下で有効に活用されるパッケージスコープの変数が存在します。もちろん、ここに紹介していることに限らないのでメモリ効率のためには臆せず使うことも考えてみてください。

==== メモリキャッシュ

メモリキャッシュをするときもパッケージスコープに@<code>{map}や@<code>{map}をメンバ変数とした@<code>{struct}を変数としてキャッシュにすることもあります。並行処理で使われることが前提になるため競合状態のハンドリングを忘れずに実装する必要があります。

==== 読み取り専用のテーブル

@<code>{unicode}パッケージには様々な文字に対応するためのUnicodeテーブル@<fn>{unicode_tables}が定義されています。
//footnote[unicode_tables][@<href>{https://golang.org/src/unicode/tables.go}]

== @<code>{init}関数

@<code>{init}関数は実行時の初期化で呼び出される特殊な関数です。実行されるタイミングですが、パッケージの変数が初期化された後に@<code>{init}関数が実行されます。

//list[init_function][init関数][go]{
package main

import (
  "fmt"
)

var str = func() string {
  fmt.Println("variable")
  return "Hello, world"
}()

func init() {
  fmt.Println("init")
}

func main() {
  fmt.Println(str)
}
//}

//cmd{
$ go run main.go
variable
init
Hello, world
//}

@<code>{main}パッケージに@<code>{init}関数が定義されている場合は@<code>{main}関数が実行される前に実行されます。

=== 複数の@<code>{init}関数

Goはパッケージスコープの仕様のため、別々のファイルに同じ関数名の関数を定義しても同じパッケージにそれぞれのファイルが存在していたらビルドは失敗します。しかし、この@<code>{init}関数は特殊な仕様でパッケージ内にいくつ定義してもビルドすることが可能です。

//list[multiple_init_function][複数のinit関数][go]{
package main

import (
  "fmt"
)

var str = func() string {
  fmt.Println("variable")
  return "Hello, world"
}()

func init() {
  fmt.Println("init1")
}

func init() {
  fmt.Println("init2")
}

func main() {
  fmt.Println(str)
}
//}

//cmd{
$ go run main.go
variable
init1
init2
Hello, world
//}

@<code>{init}関数はコンパイル時にユニークな名前を持った関数にリネームされています。

//list[multiple_init_function_renamed][init関数のリネーム][go]{
package main

import (
  "fmt"
  "runtime"
)

func init() {
  var pcs [1]uintptr
  runtime.Callers(1, pcs[:])
  fn := runtime.FuncForPC(pcs[0])
  fmt.Println(fn.Name())
}

func main() {}
//}

//cmd{
$ go run main.go
main.init.0
//}

=== @<code>{init}関数をインポート

@<code>{main}パッケージ内の@<code>{init}関数ではなく、別のパッケージに@<code>{init}関数が存在している場合です。

//list[import_init_function_example][init関数のインポート][go]{
package example

import (
  "fmt"
)

func init() {
  fmt.Println("example")
}
//}

この@<code>{example}パッケージを@<code>{main}パッケージにインポートします。

//list[import_init_function][init関数のインポート][go]{
package main

import (
  "fmt"
  "./example"
)

func init() {
  fmt.Println("init")
}

func main() {}
//}

//cmd{
$ go run main.go
example
init
//}

インポートされたパッケージに存在する@<code>{init}関数もしっかりと処理されて結果が表示されます。

=== @<code>{init}関数の実行順序

複数の@<code>{init}関数やインポートされた@<code>{init}関数の初期化（実行）順序です。

//list[import_init_function_example1][example1][go]{
package example1

import (
  "fmt"
)

func init() {
  fmt.Println("example1)
}
//}

//list[import_init_function_example2][example2][go]{
package example2

import (
  "fmt"
)

func init() {
  fmt.Println("example2)
}
//}

この@<code>{example1, example2}のパッケージを@<code>{main}パッケージにブランクインポートします。

//list[import_init_function][init関数のインポート][go]{
package main

import (
  "fmt"
  _ "./example1"
  _ "./example2"
)

func init() {
  fmt.Println("init1")
}

func init() {
  fmt.Println("init2")
}

func main() {}
//}

//cmd{
$ go run main.go
example1
example2
init1
init2
//}

結果は最初にインポートされている@<code>{example1}が実行され、その次に@<code>{example2}が実行されます。それらの処理が終わったあとにインポート元となる@<code>{main}パッケージの@<code>{init}関数が実行されています。これは実行における依存関係として正常に動作している証拠です。

さて、このときに@<code>{import}文に書かれた@<code>{example1}と@<code>{example2}の順番を変えると結果も逆転します。依存関係の順序として正しいですが、もしもこの順序の違いで不具合が発生してしまうことが生じてしまう場合は設計の見直しをおすすめします。@<code>{import}文の順序はなるべく順不同であっても同じような結果であるべきだからです。

== おわりに

Goにおける初期化について、パッケージスコープの変数と@<code>{init}関数について紹介しました。気にしなくてもコーディングすることは可能ですが、細かいところまでケアしてコードを書くことによってメモリや処理の最適化がされます。

#@# textlint-enable