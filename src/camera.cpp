#include "surveillance/camera.hpp"
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
namespace surveillance {
namespace {
int xioctl(int fd,unsigned long request,void* arg){int rc;do{rc=ioctl(fd,request,arg);}while(rc<0&&errno==EINTR);return rc;}
}
Camera::~Camera(){close();}
bool Camera::open(const std::string& device,uint32_t width,uint32_t height){
    fd_=::open(device.c_str(),O_RDWR|O_NONBLOCK);if(fd_<0)return false;
    v4l2_format fmt{};fmt.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;fmt.fmt.pix.width=width;fmt.fmt.pix.height=height;fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_YUYV;fmt.fmt.pix.field=V4L2_FIELD_ANY;
    if(xioctl(fd_,VIDIOC_S_FMT,&fmt)<0||fmt.fmt.pix.pixelformat!=V4L2_PIX_FMT_YUYV){close();return false;}
    v4l2_requestbuffers req{};req.count=4;req.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;req.memory=V4L2_MEMORY_MMAP;
    if(xioctl(fd_,VIDIOC_REQBUFS,&req)<0||req.count<2){close();return false;}
    buffers_.resize(req.count);
    for(uint32_t i=0;i<req.count;++i){v4l2_buffer b{};b.type=req.type;b.memory=req.memory;b.index=i;
        if(xioctl(fd_,VIDIOC_QUERYBUF,&b)<0){close();return false;}
        buffers_[i].size=b.length;buffers_[i].data=mmap(nullptr,b.length,PROT_READ|PROT_WRITE,MAP_SHARED,fd_,b.m.offset);
        if(buffers_[i].data==MAP_FAILED){buffers_[i].data=nullptr;close();return false;}
        if(xioctl(fd_,VIDIOC_QBUF,&b)<0){close();return false;}}
    v4l2_buf_type type=V4L2_BUF_TYPE_VIDEO_CAPTURE;if(xioctl(fd_,VIDIOC_STREAMON,&type)<0){close();return false;}return true;
}
bool Camera::read(std::vector<uint8_t>* frame){
    pollfd pfd{fd_,POLLIN,0};if(poll(&pfd,1,2000)<=0)return false;
    v4l2_buffer b{};b.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;b.memory=V4L2_MEMORY_MMAP;
    if(xioctl(fd_,VIDIOC_DQBUF,&b)<0)return false;
    auto* p=static_cast<uint8_t*>(buffers_[b.index].data);frame->assign(p,p+b.bytesused);
    return xioctl(fd_,VIDIOC_QBUF,&b)==0;
}
void Camera::close(){
    if(fd_>=0){v4l2_buf_type type=V4L2_BUF_TYPE_VIDEO_CAPTURE;xioctl(fd_,VIDIOC_STREAMOFF,&type);}
    for(auto& b:buffers_)if(b.data)munmap(b.data,b.size);buffers_.clear();if(fd_>=0)::close(fd_);fd_=-1;
}
}
