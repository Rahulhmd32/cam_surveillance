#include "surveillance/cloud_vms_client.hpp"
#include "surveillance/camera.hpp"
#include "surveillance/ws_util.hpp"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <linux/videodev2.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {
std::atomic<bool> running{true};
void stop_handler(int) { running.store(false); }
void yuyv_to_rgb(const uint8_t* y, uint8_t* rgb, int w, int h) {
    auto c=[](int v){ return static_cast<uint8_t>(std::clamp(v,0,255)); };
    for(int i=0;i<w*h/2;++i,y+=4,rgb+=6){int y0=y[0],u=y[1]-128,y1=y[2],v=y[3]-128;
        rgb[0]=c(y0+(v*1402>>10));rgb[1]=c(y0-(u*344>>10)-(v*714>>10));rgb[2]=c(y0+(u*1772>>10));
        rgb[3]=c(y1+(v*1402>>10));rgb[4]=c(y1-(u*344>>10)-(v*714>>10));rgb[5]=c(y1+(u*1772>>10));}
}
void jpeg_write(void* ctx,void* data,int size){auto* o=static_cast<std::vector<uint8_t>*>(ctx);auto* p=static_cast<uint8_t*>(data);o->insert(o->end(),p,p+size);}
int64_t now_ms(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
}

int main(int argc,char** argv){
    std::string device=argc>1?argv[1]:"/dev/video3";
    surveillance::CloudConfig cloud;
    if(argc>2)cloud.device_id=argv[2]; if(argc>3)cloud.vehicle_id=argv[3]; if(argc>4)cloud.host=argv[4];
    constexpr int width=640,height=480,quality=60;
    std::signal(SIGINT,stop_handler);std::signal(SIGTERM,stop_handler);
    surveillance::Camera capture;
    if(!capture.open(device,width,height)){std::fprintf(stderr,"Cannot start camera %s\n",device.c_str());return 1;}
    surveillance::CloudVmsClient client(cloud);client.start();
    std::vector<uint8_t> rgb;uint64_t count=0;auto start=std::chrono::steady_clock::now();
    std::printf("Streaming %s as %s to %s:%d\n",device.c_str(),cloud.device_id.c_str(),cloud.host.c_str(),cloud.port);
    while(running.load()){surveillance::CameraFrame frame=capture.read();if(!frame.valid)continue;
        if(frame.format!=V4L2_PIX_FMT_YUYV){std::fprintf(stderr,"Unsupported camera format: 0x%08x\n",frame.format);continue;}
        if(frame.data.size()<size_t(frame.width)*frame.height*2){std::fprintf(stderr,"Short camera frame\n");continue;}
        rgb.resize(size_t(frame.width)*frame.height*3);yuyv_to_rgb(frame.data.data(),rgb.data(),frame.width,frame.height);
        std::vector<uint8_t> jpeg;stbi_write_jpg_to_func(jpeg_write,&jpeg,frame.width,frame.height,3,rgb.data(),quality);if(jpeg.empty())continue;
        ++count;float elapsed=std::chrono::duration<float>(std::chrono::steady_clock::now()-start).count();
        surveillance::CloudFrame out;out.image_b64=surveillance::base64_encode(jpeg.data(),jpeg.size());out.frame=count;out.fps=count/elapsed;out.ts_ms=now_ms();client.publish(std::move(out));}
    client.stop();capture.close();return 0;
}
