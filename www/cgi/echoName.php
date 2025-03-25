<?php
// ヘッダーを設定
header('Content-Type: text/plain; charset=UTF-8');

// リクエストのbodyを取得
$input = file_get_contents('php://input');

// JSONをデコード
$data = json_decode($input, true);

// nameフィールドが存在すれば表示、なければエラー
if (isset($data['name'])) {
    echo htmlspecialchars($data['name']);
} else {
    echo "No name provided";
}
?>