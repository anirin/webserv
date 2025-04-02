<?php
header("Content-Type: text/plain; charset=UTF-8");

echo "Starting infinite loop...\n";

for (;;) {
    sleep(1000);
    echo "Looping...\n";
}
?>