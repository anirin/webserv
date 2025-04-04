<?php
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    // 画像ファイルが送信されている場合
    if (isset($_FILES['file'])) {
        $file = $_FILES['file'];
        $fileData = file_get_contents($file['tmp_name']);
        $base64Image = base64_encode($fileData);
        
        // Content-Typeを画像の形式に設定（例：JPEG）
        header('Content-Type: text/plain');  // base64データなのでtext/plainに
        echo $base64Image;
    } else {
        // その他のPOSTデータ
        $body = $_POST['data'] ?? 'No data';
        echo "Received data: " . htmlspecialchars($body);
    }
}
?>
