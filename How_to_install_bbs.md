It's written by Danielfree, I just copy it from BBS. :-) HTH.


Danielfree (03电工，星光EE|Wonderwall|Sk8er Boi) 于 2007年11月11日20:24:33

本文以现有svn库中最新代码为例
编译环境: Ubuntu 7.10 up-to-date,
gcc version 4.1.3 20070929 (prerelease)  (Ubuntu 4.1.2-16ubuntu2)

1. checkout 代码
$svn checkout svn://61.129.42.9/trunk trunkcode
将svn库中的代码checkout 放在trunkcode目录下

如果在校内 可以直接用svn://10.8.225.9/trunk  具体请看置底svn代码库

daniel@mars:/bbscode/trunkcode$ svn info
Path: .
URL: svn://61.129.42.9/trunk
Repository Root: svn://61.129.42.9
Repository UUID: 240c92a7-fe34-0410-8f31-87f02ec2f212

Revision: 6
Node Kind: directory
Schedule: normal
Last Changed Author: danielfree
Last Changed Rev: 6
Last Changed Date: 2007-11-05 05:07:32 +0000 (Mon, 05 Nov 2007)

2. 建立bbs用户和组 设置uid和gid
sudo groupadd -g 9999 bbs
为BBS 创建一个系统的组( GROUP )

sudo adduser --home /home/bbs --uid 9999 --ingroup bbs --disabled-login bbs
为BBS 创建一个系统的帐号( USER )

sudo passwd bbs
为 bbs 这个帐号设定密码，这个密码不要让别人知道

默认BBS使用的用户是bbs, 组是bbs, uid和gid都是9999
如果要修改这些值，注意同时修改代码中telnet/include/config.h 中对应的值

3.
telnet/include目录下
编辑config.h文件  取消#define FDQUAN的注释 因为如果不用这个编译的话
用户上限是和光滑一样 自己内存不够大是不可能跑起来的

建议只是在个人电脑上编译调试的话， 在config.h
line 150:  #define MAXUSERS 25000 把这个值再改小一些
另外一个重要的参数是 line 151: #define MAXBOARD 500
这个应该是默认代码的bug 这个值应该是300
因为这个值和代码 bbshome/BOARDS这个文件的大小应该是严格对应的
而svn库中这个文件的大小应该是76800, 76800/300 =256
如果我没有记错的话-.-  一个版面的数据结构大小是256字节

3.
编译代码的话就在telnet目录下 make update

add by biggertigerLu on 21-04-10
debian lenny needs following packages:
make, gcc, libgd2-xpm-dev, automake, libtool, libfcgi-dev
./bootstrap
./configure --enable-debug --enable-www
make && make install

(如果是第一次编译则是make install, 以后修改了代码就直接make update)
会将编译好的二进制文件放到 /home/bbs/bin 下 ( 默认情况  路径修改参见config.h)
运行bbs的方法如下( debian/ubuntu ):
sudo /home/bbs/bin/miscd daemon
sudo /home/bbs/bin/bbsd 23
然后注册一下SYSOP和guest
然后就登录自己的23端口吧:)