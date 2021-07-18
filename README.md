```c
8888888 8888888888  8 8888 b.             8 `8.`8888.      ,8'     ,o888888o.        d888888o.   
      8 8888        8 8888 888o.          8  `8.`8888.    ,8'   . 8888     `88.    .`8888:' `88. 
      8 8888        8 8888 Y88888o.       8   `8.`8888.  ,8'   ,8 8888       `8b   8.`8888.   Y8 
      8 8888        8 8888 .`Y888888o.    8    `8.`8888.,8'    88 8888        `8b  `8.`8888.     
      8 8888        8 8888 8o. `Y888888o. 8     `8.`88888'     88 8888         88   `8.`8888.    
      8 8888        8 8888 8`Y8o. `Y88888o8      `8. 8888      88 8888         88    `8.`8888.   
      8 8888        8 8888 8   `Y8o. `Y8888       `8 8888      88 8888        ,8P     `8.`8888.  
      8 8888        8 8888 8      `Y8o. `Y8        8 8888      `8 8888       ,8P  8b   `8.`8888. 
      8 8888        8 8888 8         `Y8o.`        8 8888       ` 8888     ,88'   `8b.  ;8.`8888 
      8 8888        8 8888 8            `Yo        8 8888          `8888888P'      `Y8888P ,88P' 


```
## 1. 项目背景
Tiny-OS操作系统内核是一个基于Intel x86平台的32位处理器的系统内核，可以运行在bochs（一个x86硬件平台的开源模拟器）的虚拟机中，使用的虚拟机开发环境为Ubuntu 20.04 + VMware workstation。

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210718202646756.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQ1ODc3NTI0,size_16,color_FFFFFF,t_70)

整体看Step-by-Step/SequenceChart/OS.drawio,那里比较详细，各个小功能看剩下的drawio。
这里的话把整体实现思路列举出来：

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210718182847336.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQ1ODc3NTI0,size_16,color_FFFFFF,t_70#pic_center)
### 2. 系统功能列表以及展示
| 系统功能列表 | 完成状态 |---------|系统功能列表 | 完成状态 |
|--|--|--|--|--|
| 多进程 | Done |---------| Shell系统交互 | Done |
| 内核多线程 | Done |---------| 常用的一些命令 | Done |
| 用户多线程 | Done |---------| 简易版本文件系统 | Done |
| 虚拟内存管理 | Done |---------| 完善文件系统 | TODO |
| 分页内存管理 | Done |---------| Pthread线程库 | TODO |
| 物理内存管理 | Done |---------| 进程的并发 | TODO |
| 管道通信 | Done |---------| GUI初步 | TODO |
| IDE硬盘驱动 | Done |---------| 蜂鸣器 | TODO |
| KeyBoard键盘驱动 | Done |---------| 应用程序 | TODO |
| 输入输出函数 | Done |---------| 一个Tiny版本的TCP/IP协议栈 | TODO |
| 异常处理 | Done |---------| CMAKE | TODO |







功能展示：

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210718200627488.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQ1ODc3NTI0,size_16,color_FFFFFF,t_70)![在这里插入图片描述](https://img-blog.csdnimg.cn/20210718200700594.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQ1ODc3NTI0,size_16,color_FFFFFF,t_70)

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210718200715326.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQ1ODc3NTI0,size_16,color_FFFFFF,t_70)![在这里插入图片描述](https://img-blog.csdnimg.cn/20210718200727201.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQ1ODc3NTI0,size_16,color_FFFFFF,t_70)

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210718200736407.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQ1ODc3NTI0,size_16,color_FFFFFF,t_70)![在这里插入图片描述](https://img-blog.csdnimg.cn/20210718200747909.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQ1ODc3NTI0,size_16,color_FFFFFF,t_70)

![在这里插入图片描述](https://img-blog.csdnimg.cn/2021071820075750.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQ1ODc3NTI0,size_16,color_FFFFFF,t_70)![在这里插入图片描述](https://img-blog.csdnimg.cn/20210718200803244.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQ1ODc3NTI0,size_16,color_FFFFFF,t_70)

##  3. 开发环境以及环境依赖
. Ubuntu linux:
 -  brew
 -  x86_64-elf-binutils
 -  x86_64-elf-gcc
 -  nasm
 -  bochs
 -  bochs-x
 -  xorg-dev
 -  libgtk2.0-dev
 -  build-essential
 -  make
 -  bison 
 -  vscode and drawio插件


## 4. 如何运行
注意一定要安装 git-lfs，不然上传的disk80.img下载不下来
```sh
sudo apt-get install git     -y
sudo apt-get install git-lfs -y
cd Desktop/
git clone https://github.com/awei-lwj/Tiny-OS.git
cd Tiny-OS
./Begin.sh
```

---
出现这个界面之后，按6

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210718202046232.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQ1ODc3NTI0,size_16,color_FFFFFF,t_70)
按c
![在这里插入图片描述](https://img-blog.csdnimg.cn/20210718202123804.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQ1ODc3NTI0,size_16,color_FFFFFF,t_70)

然后再弹出的界面进行操作：

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210718202245647.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQ1ODc3NTI0,size_16,color_FFFFFF,t_70)

退出方式在命令行那里按ctrl+c,然后输入exit.

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210718202350669.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQ1ODc3NTI0,size_16,color_FFFFFF,t_70)

## 5. Tiny-OS的文件结构
Tiny-OS的文件结构：
![Tiny-OS的文件结构](https://img-blog.csdnimg.cn/20210718180313391.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQ1ODc3NTI0,size_16,color_FFFFFF,t_70)
| 目录 | 描述 |
|--|--|
| Step-by-Step | 实现思路，以及1.5G的参考书（百度网盘连接 | 
|  Step-by-Step/ReferenceBooks | 1.41G的电子书，百度网盘链接失效了踢我一脚 |
| Step-by-Step/SequenceChart | 时序条，每个功能的实现思路都放在这里，用vscode的drawio插件来查看 |
| bin | 用来存放二进制文件 |
| tool | bochs虚拟机用来装Tiny-OS |
| build | 用来存放重定向文件 |
| Begin.sh | 一键安装环境并且，运行等一下安装那里就知道了 |
| GodBlessYou.S | 佛祖保佑，永无bug |
| README.md | .................. |
| boot | 加载内核，从BIOS到kernel |
| command | 用户空间的外部命令 |
| device | 设备，时钟序列，键盘输入等 |
| fs | 文件系统 |
| kernel | 内核 |
| lib | 内核态和用户态所用的C库函数 |
| shell | 实现shell界面 |
| thread | 线程、进程 |
| userprog | 用户空间的线程 |


## 6. CONTRIBUTING

[awei-lwj](https://github.com/awei-lwj)

## 7. LICENSE
 [LICENSE.md](https://github.com/awei-lwj/Tiny-OS/blob/master/LICENSE)

## 8. 给awei倒杯冰阔落
最后，各位靓仔美女，能不能请我喝一杯冰阔落
![在这里插入图片描述](https://img-blog.csdnimg.cn/20210718204953214.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQ1ODc3NTI0,size_16,color_FFFFFF,t_70)
## 9. 最后给各位靓仔美女来波buff加成

```asm
;;-------------------------------------------------------------------;;
;;========================    Hello world!   ========================;;
;; God bless you                                  Far away from bug! ;;
;;                              _ooOoo_                              ;;
;;                             o8888888o                             ;;
;;                             88" . "88                             ;;
;;                             (| -_- |)                             ;;  
;;                              O\ = /O                              ;;
;;                          ____/`---'\____                          ;;
;;                          . ' \\| |// ' .                          ;; 
;;                         / \\||| : |||// \                         ;;
;;                        /_||||| -+- |||||_\                        ;;
;;                       | | | \\\ - /// | | |                       ;;
;;                       | \_| ''\---/'' |_/ |                       ;;
;;                        \ .-\__ `-` ___/-. /                       ;;
;;                     ___'. .' /--.--\ '. .'___                     ;;
;;                  ."" '< '.___\_<|>_/___.' >' "".                  ;;
;;                 | | : `- \`.;`\ _ /`;.`/ -` : | |                 ;;         
;;                    \ \ `-.\_ __\ /__ _/ .-` / /                   ;;
;;           ======`-.____`-.___\_____/___.-`____.-'======           ;;
;;                              `=---='                              ;;
;;           .............................................           ;;
;;           佛祖保佑                                永无BUG           ;;
;;                                                                   ;;
;;===================================================================;;
;;                          _   _                    __              ;;
;;        __ _   _ __ ___  (_) | |_  _   _   ___    / _|  ___        ;;
;;       / _` | | '_ ` _ \ | | | __|| | | | / _ \  | |_  / _ \       ;;
;;      | (_| | | | | | | || | | |_ | |_| || (_) | |  _|| (_) |      ;;
;;       \__,_| |_| |_| |_||_|  \__| \__,_| \___/  |_|   \___/       ;;
;;                                                                   ;;
;;===================================================================;;
;;+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++;;
;;-------------------------------------------------------------------;;




```
