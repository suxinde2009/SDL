SDL<br>
---
本项目源码在基于Rose的app源码树中位置
<app>-src
  |----<app>
  |----linker
  |----SDL   <--------------本项目
      |----libogg-1.3.0
      |----libvorbis-1.3.2
      |----SDL2_image-2.0.1
      |----SDL2_net-2.0.1
      |----SDL2_ttf-2.0.1
      |----SDL2-2.0.14

      
本意是把自个改的加到官方SDL，但得到回复说官方暂没有在GitHub开设的计划(http://forums.libsdl.org/viewtopic.php?t=11578 )，于是就有了这项目，毕竟我在写的Rose SDK就基于SDL，而app则基于Rose。

在内容上，此项目基本就是官方SDL的镜像，只是在它之上增加了自个的一些修改。以下各组件基于的版本。
* SDL2: 2.0.4     --160102
* SDL2_image: 2.0.1
* SDL2_mixer: 2.0.1
* SDL2_net: 2.0.1
* SDL2_ttf: 2.0.14
* libogg-1.3.0: iOS平台，编译SDL2_mixer时需要这个库。
* libvorbis-1.3.2: iOS平台，编译SDL2_mixer时需要这个库。

主要修改<br>
----
* 蓝牙(BLE)。（只支持iOS）
* 采集加速计(Accelerometer)。（只支持iOS）
* 采集陀螺仪(Gyroscope)。（只支持iOS）
* 采集声音。（只支持iOS）
* 播放声音。在iOS平台，让需要后台播放的app更省电。
* 修改了alpha混合计算方法。参考（http://www.freeors.com/bbs/forum.php?mod=viewthread&tid=23430&extra=page%3D1）

要查看详细修改内容可用目录比较工具，像UltraCompare。
