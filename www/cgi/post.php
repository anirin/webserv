
<?php
// CGIとしての動作を想定したシンプルなPHPスクリプト例

// レスポンスのContent-Typeを指定（文字化け防止のためUTF-8）
header('Content-Type: text/html; charset=UTF-8');

// HTMLの出力
echo "<!DOCTYPE html>\n";
echo "<html lang=\"ja\">\n";
echo "<head>\n";
echo "  <meta charset=\"UTF-8\">\n";
echo "  <title>PHP CGI Test</title>\n";
echo "</head>\n";
echo "<body>\n";
echo "  <h1>PHP CGI - POST Test</h1>\n";

// POSTメソッドが使われているかを判定
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    // 送られてきたPOSTデータがあるかを確認し、動的に出力
    if (!empty($_POST)) {
        echo "  <p>以下のPOSTデータを受信しました:</p>\n";
        echo "  <ul>\n";
        foreach ($_POST as $key => $value) {
            echo "    <li><strong>" . htmlspecialchars($key, ENT_QUOTES, 'UTF-8')
                 . "</strong> = " . htmlspecialchars($value, ENT_QUOTES, 'UTF-8')
                 . "</li>\n";
        }
        echo "  </ul>\n";
    } else {
        echo "  <p>POSTデータがありませんでした。</p>\n";
    }
} else {
    echo "  <p>このページはPOSTメソッドでアクセスすると動的コンテンツを表示します。</p>\n";
}

// フォーム(テスト用)の出力 -- 必要に応じて削除可能
echo "  <hr>\n";
echo "  <form action=\"index.php\" method=\"POST\">\n";
echo "    <label>サンプル入力: <input type=\"text\" name=\"sampleInput\"></label>\n";
echo "    <button type=\"submit\">送信</button>\n";
echo "  </form>\n";

echo "</body>\n";
echo "</html>\n";
