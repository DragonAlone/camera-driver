# camera-driver

#### Introduction

Porting OV2640 Camera onto IMX6ULL Development Board

#### V4L2 subsystem architecture diagram

![image-20240511150617013](C:\Users\13631\AppData\Roaming\Typora\typora-user-images\image-20240511150617013.png)


#### Installation Tutorial

In a Linux environment, execute "make arm_module" to compile it into a .ko module file. Power on the development board, execute 'insmod ov2640_driver.ko' to load the module, and then execute "./camera_app" to start capturing images.

#### Open-source library - mjepg_streamer 

http://www.ijg.org/files

https://github.com/jacksonliam/mjpg-streamer

