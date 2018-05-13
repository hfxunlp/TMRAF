# TMRAF
Tweaked MediaTek Router Firmware Based on ASUS

提供部分型号固件(commit 8ec3a10)下载，仅供测试，下载地址如下:

链接:https://pan.baidu.com/s/1jJTvhPW 密码:ekw7

或 链接:https://eyun.baidu.com/s/3kXj8gv1 密码:o0X5

- 斐讯K2(PSG1218)

- 联想newifi mini(NEWIFI-MINI)

## 一、参考项目
[tweaked ASUS rt-n56u](https://bitbucket.org/padavan/rt-n56u)  
注：针对中国用户，将原项目DNS 8.8.8.8，替换为了阿里DNS 223.6.6.6。

## 二、文件说明

1. 目录结构同参考项目目录结构

2. 添加中文语言文件，修复页面显示问题

3. 添加部分机型适配文件

## 三、汉化文件使用方法

1. 编辑`scripts/install.sh`，修改DESTDIR为你的项目目录

默认 `DESTDIR=/media/Storage/workspace/router/rt-n56u`

2. 执行`scripts/install.sh`将文件复制到你的项目中

`bash scripts/install.sh`

3. 编辑你的项目`trunk`目录下的`.config`文件，修改语言配置项

`CONFIG_FIRMWARE_INCLUDE_LANG_CN=y`

4. 编译项目(参见五)

## 四、机型适配文件说明

1. 机型适配文件位于`trunk/configs/boards`目录下，每个文件夹对应一个机型。
注意：需手动将路由器适配文件复制到你的项目对应目录下

2. 把`trunk/configs/templates`下对应机型的config文件复制到`trunk`目录下重命名为`.config`

## 五、编译指令

`bash TMRAF/scripts/rebuild.sh`
