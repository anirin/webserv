import telnetlib
import time

# サーバーのホストとポート
HOST = "127.0.0.1"
PORT = 8080

# Chunkedリクエストを一行ずつ分割
request_lines = [
    "POST /empty.html HTTP/1.1\r\n",
    "Host: 127.0.0.1:8080\r\n",
    "Transfer-Encoding: chunked\r\n",
    "\r\n",  # ヘッダーの終わり
    "4\r\n",  # 最初のチャンクのサイズ
    "Wiki\r\n",  # 最初のチャンクの内容
    "5\r\n",  # 2番目のチャンクのサイズ
    "pedia\r\n",  # 2番目のチャンクの内容
    "0\r\n",  # 最後のチャンク（終了）
]

try:
    # telnet接続を確立
    tn = telnetlib.Telnet(HOST, PORT)
    
    # 一行ずつ0.5秒間隔で送信
    for line in request_lines:
        print(f"送信: {line.rstrip()}")
        tn.write(line.encode('ascii'))
        time.sleep(0.5)  # 0.5秒待機
    
    
    # 応答を受信して表示
    response = tn.read_all().decode('ascii')
    print("サーバーからの応答:\n", response)
    
    # 接続を閉じる
    tn.close()

except ConnectionRefusedError:
    print(f"{HOST}:{PORT} への接続が拒否されました。サーバーが起動しているか確認してください。")
except Exception as e:
    print(f"エラーが発生しました: {e}")