# 服务器部署指南

## 环境要求
- Linux 系统
- g++ 编译器
- PHP 7.0+ 环境
- curl 工具

## 1. C++ 服务器部署

### 编译程序
```bash
g++ time_server.cpp -o server -lcurl

### 启动服务器
./server

## 2. PHP 环境配置

### 安装依赖
sudo apt update
sudo apt install php-curl


这里要用ifconfig 查看当前的linux是什么ip
执行ifconfig  把linux当前的ip复制一下 执行这个

启动php服务器 (替换实际IP)
php -S 192.168.1.175:8080


## 3.启动内网穿透
### 操作步骤
#### 关注「敲代码斯基」公众号

#### 获取穿透命令模板

#### 替换IP和端口参数


更改完成后即可启动内网穿透 （这一步的作用是 可以让不在同一个局域网的环境下访问这个ip地址和端口号 这样不用购买域名即可远程访问）
我是使用这个启动的 
curl https://v2.i996.me|bash -s tmhe67191



