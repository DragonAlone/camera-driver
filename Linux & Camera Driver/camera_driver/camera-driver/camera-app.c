#include "linux/videodev2.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/mman.h>
#include <assert.h>

int query_camera_device(int fd)
{
    int ret;
    struct v4l2_fmtdesc fmt;
    struct v4l2_capability cap;
    fmt.index = 0;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    while(1){
        ret = ioctl(fd,VIDIOC_ENUM_FMT,&fmt);
        if(ret < 0){
           break;
        }
        
        printf("{pixelfermat = %c%c%c%c},description = '%s'\n",
                     fmt.pixelformat & 0xff,(fmt.pixelformat >> 8)&0xff,
                     (fmt.pixelformat >> 16) & 0xff,(fmt.pixelformat >> 24)&0xff,
                     fmt.description);
        
         fmt.index ++ ;
    }
    
    ret = ioctl(fd,VIDIOC_QUERYCAP,&cap);
    if(ret < 0){
         perror("FAIL to ioctl VIDIOC_QUERYCAP");
         exit(EXIT_FAILURE);
    }
    
    if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)){
           printf("The Current device is not a video capture device\n");
           exit(EXIT_FAILURE);
        
     }

     if(!(cap.capabilities & V4L2_CAP_STREAMING)){
            printf("The Current device does not support streaming i/o\n");
            exit(EXIT_FAILURE);
     }
   
    
     return 0;
}

#define IMAGE_WIDTH     640
#define IMAGE_HEIGHT    480

void init_camera_fmt(int fd)
{
    int ret;
    struct v4l2_format cam_fmt;

    cam_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam_fmt.fmt.pix.width = IMAGE_WIDTH;
    cam_fmt.fmt.pix.height= IMAGE_HEIGHT;
    cam_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    cam_fmt.fmt.pix.field = V4L2_FIELD_ANY;

    ret = ioctl(fd,VIDIOC_S_FMT,&cam_fmt);
    if(ret < 0){
        perror("Fail to ioctl:VIDIOC_S_FMT");
        exit(EXIT_FAILURE);
    }

    ret = ioctl(fd,VIDIOC_G_FMT,&cam_fmt);
    if(ret < 0){
        perror("Fail to ioctl:VIDIOC_S_FMT");
        exit(EXIT_FAILURE);
    }

    printf("Init camera image width:%d,height:%d\n",cam_fmt.fmt.pix.width,
                                                    cam_fmt.fmt.pix.height);
    printf("Init camera pixelformat V4L2_PIX_FMT_YUYV\n");

    return;
}

int request_camera_buffer(int fd,int n)
{
        int n_buffer;
        struct v4l2_requestbuffers reqbuf;

        bzero(&reqbuf,sizeof(reqbuf));
        reqbuf.count  = n;
        reqbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        reqbuf.memory = V4L2_MEMORY_MMAP;
        
        if(-1 == ioctl(fd,VIDIOC_REQBUFS,&reqbuf))
        {
                perror("Fail to ioctl 'VIDIOC_REQBUFS'");
                exit(EXIT_FAILURE);
        }
        
        printf("real request buffer : %d\n",reqbuf.count);
        
        return reqbuf.count;
}

typedef struct
{
        void *start;
        int length;
}user_buffer_t;

user_buffer_t *mmap_camera_buffer(int fd,int n)
{
    int ret;
    int i;
    user_buffer_t *usr_buf;
    
    usr_buf = calloc(n,sizeof(*usr_buf));
    if(usr_buf == NULL){
         fprintf(stderr,"Out of memory\n");
         exit(EXIT_FAILURE);
     }

    for(i = 0; i < n; i ++){
          struct v4l2_buffer buf;
                
          bzero(&buf,sizeof(buf));
          buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
          buf.memory = V4L2_MEMORY_MMAP;
          buf.index = i;
                
          ret = ioctl(fd,VIDIOC_QUERYBUF,&buf);
          if(ret < 0){
               perror("Fail to ioctl : VIDIOC_QUERYBUF");
               exit(EXIT_FAILURE);
          }
                
          usr_buf[i].length = buf.length;
          usr_buf[i].start = mmap(
                                    NULL,/*start anywhere*/
                                    buf.length,
                                    PROT_READ | PROT_WRITE,
                                    MAP_SHARED,
                                    fd,
                                    buf.m.offset
                              );
                              
          if(MAP_FAILED == usr_buf[i].start){
                perror("Fail to mmap");
                exit(EXIT_FAILURE);
           }
           
           ret = ioctl(fd,VIDIOC_QBUF,&buf);
           if(ret < 0){
                perror("Fail to ioctl 'VIDIOC_QBUF'");
                exit(EXIT_FAILURE);
           }
     }        

    return usr_buf;
}

int start_capturing(int fd)
{
       int ret;
       enum v4l2_buf_type type;
        
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ret  = ioctl(fd,VIDIOC_STREAMON,&type);
        if(ret < 0){
             perror("Fail to ioctl 'VIDIOC_STREAMON'");
             exit(EXIT_FAILURE);
        }

        return 0;
}

void save_to_image(int index,user_buffer_t *user_buffer)
{
    int n;
    FILE* fp;
    char filename[1024];

    sprintf(filename,"image%d.yuv",index);
    fp = fopen(filename,"w");
    if(!fp){
        perror("fail to fopen\n");
        exit(EXIT_FAILURE);
    }
    n = fwrite(user_buffer[index].start,sizeof(char),user_buffer[index].length,fp);
    if(n != user_buffer[index].length){
        perror("fail to write");
        exit(EXIT_FAILURE);
    }
    fclose(fp);
    return ;
}

int read_camera_frame(int fd,user_buffer_t *user_buffer,int n_buffers)
{
     int i;
     int ret;
     fd_set readfds;
     struct v4l2_buffer buf;
    
     FD_ZERO(&readfds);
     FD_SET(fd,&readfds);

     for(i = 0;i < n_buffers;i ++){  
        ret = select(fd + 1,&readfds,NULL,NULL,NULL);
        if(ret < 0){
            perror("fail to select");
            exit(EXIT_FAILURE);
        }
        bzero(&buf,sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
            
        ret = ioctl(fd,VIDIOC_DQBUF,&buf);
        if(ret < 0){
            perror("Fail to ioctl 'VIDIOC_DQBUF'");
            exit(EXIT_FAILURE);
        }

        assert(buf.index < n_buffers);

        printf("save image%d\n",buf.index);

        save_to_image(buf.index,user_buffer);

        ret = ioctl(fd,VIDIOC_QBUF,&buf);
        if(ret < 0){
                perror("Fail to ioctl 'VIDIOC_QBUF'");
                exit(EXIT_FAILURE);
        }
     }
    
     return 0;
}

void close_camera(int fd,user_buffer_t *user_buf,int n)
{
        unsigned int i;

        for(i = 0;i < n;i ++)
        {
                if(-1 == munmap(user_buf[i].start,user_buf[i].length))
                {
                        exit(EXIT_FAILURE);
                }
        }
        
        free(user_buf);
        
        close(fd);

        return;
}


int main(int argc,char *argv[]){

    user_buffer_t* user_buffer;
    int n_buffers;

    if(argc < 2){
        fprintf(stderr,"Usage:%s <camera device>\n",argv[0]);
        return -1;       
    }
    int fd = open(argv[1],O_RDWR);
    if(fd < 0){
        fprintf(stderr,"fail to open %s:%s\n",argv[1],strerror(errno));
        return -1;
    }
    printf("open camera %s success\n",argv[1]);

    query_camera_device(fd);
    init_camera_fmt(fd);

    n_buffers = request_camera_buffer(fd,5);

    user_buffer = mmap_camera_buffer(fd, n_buffers);

    start_capturing(fd);

    while(1){
        read_camera_frame(fd,user_buffer,n_buffers);
    }
    

    close_camera(fd,user_buffer,n_buffers);

    return 0;
}