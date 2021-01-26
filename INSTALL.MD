---
sidebar: auto
---

一个新安装的linux系统如何安装sisdb

1.* linux环境

以centos为例:

若已经安装了网络和其他基础组件可以直接跳转第二步：
   一般在安装时可以选择安装配置网络，那么1.1就不需要操作了.
   ***虚拟机的Network配置要选择 Bridged Adapter.

   1.安装网络

   ```shell
   vi /etc/sysconfig/network-scripts/ifcfg-enoxxxx
      *** 把ONBOOT=no修改为
      *** ONBOOT=yes
      *** 按esc : wq 回车
   service network restart
   yum -y install net-tools
   ifconfig 
   ```

   2.下载程序和安装环境
   此时可以利用其他终端远程过来

   ```shell
   ssh 192.168.1.xxx
   yum install -y git cmake gcc gcc-c++ gdb
   yum install -y nss curl libcurl
   cd /home
   git clone  https://github.com/seerline/sisdb.git
      *** 如果下载慢请修改 /etc/hosts 文件 增加两行
      *** 192.30.253.113 github.com
      *** 151.101.185.194 github.global.ssl.fastly.net
   cd sisdb
   make all
   ```

   3.测试运行
   
   ```shell
      *** 开启防火墙 
   firewall-cmd --permanent --add-port=7328-7329/tcp
   firewall-cmd --reload
   cd bin
      *** 启动服务端
   ./sisdb -f sisdb-server.conf
   ```


