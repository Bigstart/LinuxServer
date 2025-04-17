此版本基本可以实现接收消息

1.编译 c++ 服务器
g++ time_server.cpp -o server  -lcurl
启动服务器

2.启动php服务器 要安装php 和php curl
sudo apt update
sudo apt install php-curl

php -S 192.168.1.175:8080
这里要用ifconfig 查看当前的linux是什么ip

3.启动内网穿透
在微信公众号 敲代码斯基 更改当前的linux的ip和端口号 
curl https://v2.i996.me|bash -s tmhe67191



