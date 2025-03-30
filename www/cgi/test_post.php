<?php
// CGI環境変数を取得
$requestMethod = getenv('REQUEST_METHOD');

// リクエストメソッドに応じた処理
if ($requestMethod === 'POST') {
    // POSTの場合: stdinからbodyを1行ずつ読み取る
    $body = '';
    while ($line = fgets(STDIN)) {
        $body .= $line; // 複数行対応のため連結
    }

    // 環境変数の確認
    $contentLength = getenv('CONTENT_LENGTH');
    $contentType = getenv('CONTENT_TYPE');

    // 出力
    echo "Request Method: $requestMethod\n";
    echo "Content Length: $contentLength\n";
    echo "Content Type: $contentType\n";
    echo "Received Body: $body\n";

    // bodyをパースして表示
    parse_str($body, $params);
    if (!empty($params)) {
        echo "Parsed Parameters:\n";
        foreach ($params as $key => $value) {
            echo "  $key: $value\n";
        }
    }
} elseif ($requestMethod === 'GET') {
    // GETの場合: stdinは使わず、クエリストリングを取得
    $queryString = getenv('QUERY_STRING');
    
    // 出力
    echo "Request Method: $requestMethod\n";
    echo "Content Length: 0\n"; // GETではbodyなし
    echo "Content Type: none\n";
    echo "Received Body: (none)\n";

    // クエリストリングをパースして表示
    parse_str($queryString, $params);
    if (!empty($params)) {
        echo "Parsed Parameters:\n";
        foreach ($params as $key => $value) {
            echo "  $key: $value\n";
        }
    }
} else {
    echo "Unsupported Request Method: $requestMethod\n";
}
?>