<?php
// chat.php

// 检查是否有 POST 请求
if ($_SERVER["REQUEST_METHOD"] === "POST") {
    // 获取前端发送的消息
    if (isset($_POST['message'])) {
        $message = $_POST['message'];
        
        // 发送请求到 C++ 服务器
        $url = 'http://localhost:8081/send'; // C++ 服务器的地址和端口
        $post_data = http_build_query(['message' => $message]);
        
        // 使用 cURL 发送 POST 请求到 C++ 服务器
        $ch = curl_init();
        curl_setopt($ch, CURLOPT_URL, $url);
        curl_setopt($ch, CURLOPT_POST, 1);
        curl_setopt($ch, CURLOPT_POSTFIELDS, $post_data);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);

        // 执行 cURL 请求并获取返回结果
        $response = curl_exec($ch);
        curl_close($ch);
        
        //echo "收到的消息: " . htmlspecialchars($message);

        // 返回处理结果
         echo $response;
    } else {
        echo "未接收到消息。";
    }
} else {
    echo "只支持 POST 请求。";
}
?>

