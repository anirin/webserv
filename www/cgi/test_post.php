<?php
// エラー報告を有効化（デバッグ用）
error_reporting(E_ALL);
ini_set('display_errors', 1);

// CGI環境変数を取得
$requestMethod = getenv('REQUEST_METHOD');
$contentType = getenv('CONTENT_TYPE');
$contentLength = getenv('CONTENT_LENGTH');
$scriptName = getenv('SCRIPT_NAME');

header("Content-Type: text/html; charset=UTF-8");

// POSTボディの取得
$postBody = getenv('REQUEST_BODY');

// POSTデータのパース
$postParams = [];
if (!empty($postBody)) {
    parse_str($postBody, $postParams);
}

?>
<html>
<head>
    <title>POST Request Test</title>
</head>
<body>
    <h1>POST Request Processing</h1>
    
    <h2>Environment Variables:</h2>
    <pre>
REQUEST_METHOD: <?php echo $requestMethod; ?>

CONTENT_TYPE: <?php echo $contentType; ?>

CONTENT_LENGTH: <?php echo $contentLength; ?>

SCRIPT_NAME: <?php echo $scriptName; ?>
    </pre>
    
    <h2>Raw POST Body:</h2>
    <pre><?php echo htmlspecialchars($postBody); ?></pre>
    
    <h2>POST Parameters:</h2>
    <pre>
<?php
if (!empty($postParams)) {
    foreach ($postParams as $key => $value) {
        echo htmlspecialchars("$key: $value") . "\n";
    }
} else {
    echo "No POST parameters available.";
}
?>
    </pre>
    
    <p>Processing Complete</p>
</body>
</html>