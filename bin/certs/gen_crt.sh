#!/bin/sh
#1、通过openssl生成私钥
openssl genrsa -out server.key 1024
#2、根据私钥生成证书申请文件csr
openssl req -new -key server.key -out server.csr
#3、这里根据命令行向导来进行信息输入：
openssl x509 -req -in server.csr -out server.crt -signkey server.key -days 3650
