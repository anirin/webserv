<?php
// 基本的なヘッダー設定
header("Content-Type: text/plain");

// 意図的に致命的エラーを発生させる
require_once("no_exisit.php");

// ここには到達しない
echo "このメッセージは表示されません";
?>