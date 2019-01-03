#include "Tracer.h"
#include "FreeImagePlus.h"
#include "glm/gtx/intersect.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <float.h>
#include "cl.hpp"
#include <OpenCL/opencl.h>

using namespace glm;
using namespace std;

CLCamera *CLCameraFromSCamera(SCamera m_camera){
    CLCamera *camera = new CLCamera;
    camera->m_pos.x = m_camera.m_pos.x;
    camera->m_pos.y = m_camera.m_pos.y;
    camera->m_pos.z = m_camera.m_pos.z;
    camera->m_forward.x = m_camera.m_forward.x;
    camera->m_forward.y = m_camera.m_forward.y;
    camera->m_forward.z = m_camera.m_forward.z;
    camera->m_up.x = m_camera.m_up.x;
    camera->m_up.y = m_camera.m_up.y;
    camera->m_up.z = m_camera.m_up.z;
    camera->m_right.x = m_camera.m_right.x;
    camera->m_right.y = m_camera.m_right.y;
    camera->m_right.z = m_camera.m_right.z;
    camera->m_resolution.x = m_camera.m_resolution.x;
    camera->m_resolution.y = m_camera.m_resolution.y;
    
    return camera;
}

CLSettings *CLSettingsFromSettings(Settings &settings){
    CLSettings *clSettings = new CLSettings;
    clSettings->cameraDistance = settings.cameraDistance;
    clSettings->blackholeRadius = settings.blackholeRadius;
    clSettings->blackholeMass = settings.blackholeMass;
    clSettings->diskCoef = settings.diskCoef;
    
    return clSettings;
}


void CTracer::setup(){
    
    // load image
    stars = LoadImageFromFile("data/starmap_16k_retouched_02.tif");
    disk = LoadImageFromFile("data/disk_32.png");

    if(!stars){
        cout << "Can not load star image" << endl;
        exit(1);
    }
        
    // prepare OpenCL
    int xRes = settings->xRes;
    int yRes = settings->yRes;
    int DATA_SIZE = xRes * yRes;
    
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    
    std::vector<cl::Device> devices;
    platforms[0].getDevices(CL_DEVICE_TYPE_ALL, &devices);
    
    
    //select device here!
    cl::Device device = devices[1];
    std::cout << device.getInfo<CL_DEVICE_NAME>() << std::endl;
    
    
    std::vector<cl::Device> contextDevices;
    contextDevices.push_back(device);
    context = cl::Context(contextDevices);
    queue = cl::CommandQueue(context, device);
    
    //Create memory buffers
    pInputVector1 = (int *)malloc(DATA_SIZE * sizeof(int));
    pInputVector2 = (int*)malloc(DATA_SIZE*sizeof(int));
    pOutputVector = (Result *)malloc(DATA_SIZE * sizeof(Result));
    
    
    for (int i = 0; i < yRes; i++) {
        for (int j = 0; j < xRes; j++) {
            pInputVector1[i*xRes + j] = i;
            pInputVector2[i*xRes + j] = j;
        }
    }
    
    cl_int error;
    clmInputVector1 = cl::Buffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR, DATA_SIZE * sizeof(int), pInputVector1, &error);
    if (error) {
        throw error;
    }
    clmInputVector2 = cl::Buffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR, DATA_SIZE * sizeof(int), pInputVector2, &error);
    if (error) {
        throw error;
    }
    clmOutputVector = cl::Buffer(context, CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR, DATA_SIZE * sizeof(Result), pOutputVector, &error);
    if (error) {
        throw error;
    }
    
    
    std::ifstream t("kernel.cl");
    std::stringstream buffer;
    buffer <<  t.rdbuf();
    
    std::string sourceCode = buffer.str();
    //sourceCode = "#define BLACKHOLE_RADIUS " + m_pScene->blackhole.m_radius
    cl::Program::Sources source(1, std::make_pair(sourceCode.c_str(), sourceCode.length()+1));
    cl::Program program = cl::Program(context, source);
    
    int buildRes = program.build(contextDevices);
    if (buildRes != CL_SUCCESS){
        throw "build error";
    }
    kernel = cl::Kernel(program, "TestKernel");
}

std::tuple<vec3, float> CTracer::GetDiskColorByPosition(vec3 position){
    dvec2 disk_0 = dvec2(- settings->blackholeRadius * settings->diskCoef, -settings->blackholeRadius * settings->diskCoef);
    dvec2 disk_point = dvec2((position.y - disk_0.x) / (settings->blackholeRadius * settings->diskCoef * 2), (position.z - disk_0.y) / (settings->blackholeRadius * settings->diskCoef * 2));
    int pixel_y = trunc(disk_point.x * disk->getWidth());
    int pixel_x = trunc(disk_point.y * disk->getHeight());
    
    RGBQUAD *pixel = new RGBQUAD;
    
    if (disk->getPixelColor(pixel_x, pixel_y, pixel)) {
        
        float alpha = pixel->rgbReserved / 255.0;
        vec3 disk_color = vec3(pixel->rgbRed  / 255.0, pixel->rgbGreen/ 255.0, pixel->rgbBlue / 255.0);
        return std::make_tuple(disk_color, alpha);
    } else {
        throw "can't get pixel";
    }
}

vec3 CTracer::GetBackgroundColorByVector(vec3 velocity){
    double phi = atan2(velocity.x, velocity.y);
    double eta = asin(velocity.z);
    int stars_x = int(trunc((phi + M_PI) / (2 * M_PI) * stars->getWidth()));
    int stars_y = int(trunc((eta + M_PI_2) / M_PI * stars->getHeight()));
    RGBQUAD rgbquadcolor;
    if  (stars->getPixelColor(stars_x, stars_y, &rgbquadcolor) == false) {
        throw "no pixel color loaded";
    }
    return vec3(rgbquadcolor.rgbRed/255.0, rgbquadcolor.rgbGreen/255.0, rgbquadcolor.rgbBlue/255.0);
}


void CTracer::setCamera(void){
    m_camera.m_viewAngle = vec2(settings->xViewAngle, settings->xViewAngle * settings->yRes / settings->xRes);
    m_camera.m_pos = dvec3(settings->cameraX , settings->cameraY , settings->cameraZ);
    m_camera.m_resolution = uvec2(settings->xRes, settings->yRes);
    m_camera.m_pixels.resize(settings->xRes * settings->yRes);
    
    m_camera.m_forward = normalize(-m_camera.m_pos);
    m_camera.m_up = glm::dvec3(m_camera.m_forward.y, -m_camera.m_forward.x, m_camera.m_forward.z);
    if (m_camera.m_up.x < 0)
        m_camera.m_up *= -1;
    m_camera.m_right = normalize(cross(m_camera.m_forward, m_camera.m_up));
    
    m_camera.m_up *= settings->yRes;
    m_camera.m_right *= settings->xRes;
    m_camera.m_forward *= m_camera.m_resolution.x / (2 * tan(m_camera.m_viewAngle.x / 2));
}

vec3 CTracer::GetColorByResult(Result result){
    vec3 color;

    bool bDisk = false;
    
    if (result.type == 1) {
        // disk, blackhole
        if(bDisk){
        vec3 pos = vec3(result.intersect1.x, result.intersect1.y, result.intersect1.z);
        std::tuple<vec3, float> diskcolor = GetDiskColorByPosition(pos);
        float alpha = std::get<1>(diskcolor);
        color = alpha * std::get<0>(diskcolor) + (1 - alpha) * vec3(0);
        }else{
            color = vec3(0,0,0);
        }

    }else if (result.type == 2) {
        // blackhole
        
        color = vec3(0,0,0);
    } else if (result.type == 3) {
        // disk, background
        if(bDisk){
        vec3 pos = vec3(result.intersect1.x, result.intersect1.y, result.intersect1.z);
        std::tuple<vec3, float> diskcolor = GetDiskColorByPosition(pos);
        vec3 velocity = vec3(result.intersect2.x, result.intersect2.y, result.intersect2.z);
        vec3 backcolor = GetBackgroundColorByVector(velocity);
        float alpha = std::get<1>(diskcolor);
        color = alpha * std::get<0>(diskcolor) + (1 - alpha) * backcolor;
        }else{
            vec3 velocity = vec3(result.intersect2.x, result.intersect2.y, result.intersect2.z);
            vec3 backcolor = GetBackgroundColorByVector(velocity);
            color = backcolor;
        }

    } else if (result.type ==4) {
        // background
        color = GetBackgroundColorByVector(vec3(result.intersect1.x, result.intersect1.y, result.intersect1.z));
    } else if (result.type == 5) {
        
        // disk, disk, blackhole
        if(bDisk){
            vec3 pos1 = vec3(result.intersect1.x, result.intersect1.y, result.intersect1.z);
            std::tuple<vec3, float> diskcolor1 = GetDiskColorByPosition(pos1);
            vec3 pos2 = vec3(result.intersect2.x, result.intersect2.y, result.intersect2.z);
            std::tuple<vec3, float> diskcolor2 = GetDiskColorByPosition(pos2);
            float alpha1 = std::get<1>(diskcolor1);
            float alpha2 = std::get<1>(diskcolor2);
            color = alpha1 * std::get<0>(diskcolor1) + alpha2 * std::get<0>(diskcolor2) + (1 - alpha1 - alpha2) * vec3(0);
        }else{
            color = vec3(0,0,0);
        }
    } else if (result.type == 6) {
        if(bDisk){
            vec3 pos1 = vec3(result.intersect1.x, result.intersect1.y, result.intersect1.z);
            std::tuple<vec3, float> diskcolor1 = GetDiskColorByPosition(pos1);
            vec3 pos2 = vec3(result.intersect2.x, result.intersect2.y, result.intersect2.z);
            std::tuple<vec3, float> diskcolor2 = GetDiskColorByPosition(pos2);
            vec3 pos3 = vec3(result.intersect3.x, result.intersect3.y, result.intersect3.z);
            std::tuple<vec3, float> diskcolor3 = GetDiskColorByPosition(pos3);
            
            float alpha1 = std::get<1>(diskcolor1);
            float alpha2 = std::get<1>(diskcolor2);
            //float alpha3 = std::get<1>(diskcolor3);
            color = alpha1 * std::get<0>(diskcolor1) + alpha2 * std::get<0>(diskcolor2) + (1 - alpha1 - alpha2) * std::get<0>(diskcolor3);
   
        }else{
            color = vec3(0,0,0);
        }
        // disk, disk, disk
    } else if (result.type == 7) {
        // disk, disk, background
        if(bDisk){
            vec3 pos1 = vec3(result.intersect1.x, result.intersect1.y, result.intersect1.z);
            std::tuple<vec3, float> diskcolor1 = GetDiskColorByPosition(pos1);
            vec3 pos2 = vec3(result.intersect2.x, result.intersect2.y, result.intersect2.z);
            std::tuple<vec3, float> diskcolor2 = GetDiskColorByPosition(pos2);
            vec3 velocity = vec3(result.intersect3.x, result.intersect3.y, result.intersect3.z);
            vec3 backcolor = GetBackgroundColorByVector(velocity);
            float alpha1 = std::get<1>(diskcolor1);
            float alpha2 = std::get<1>(diskcolor2);
            color = alpha1 * std::get<0>(diskcolor1) + alpha2 * std::get<0>(diskcolor2) + (1 - alpha1 - alpha2) * backcolor;
        }else{
            vec3 velocity = vec3(result.intersect3.x, result.intersect3.y, result.intersect3.z);
            vec3 backcolor = GetBackgroundColorByVector(velocity);
            color = backcolor;
        }
    }
    else {
        throw "Error getting result";
    }
    
    return color;
}

void CTracer::RenderImage(){

    int xRes = settings->xRes;
    int yRes = settings->yRes;
    int DATA_SIZE = xRes * yRes;
    setCamera();
    
    int iArg = 0;
    cl_int error;
    
    clCamera = CLCameraFromSCamera(m_camera);
    clmCamera = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(*clCamera), clCamera);
    clSettings = CLSettingsFromSettings(*settings);
    clmSettings = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(*clSettings), clSettings);

    
    kernel.setArg(iArg++, clmInputVector1);
    kernel.setArg(iArg++, clmInputVector2);
    kernel.setArg(iArg++, clmOutputVector);
    kernel.setArg(iArg++, DATA_SIZE);
    kernel.setArg(iArg++, clmCamera);
    kernel.setArg(iArg++, clmSettings);
    
    int res = queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(DATA_SIZE), cl::NullRange);
    if (res != CL_SUCCESS)
        throw "was not run";
    error = queue.finish();
    if (0 != error) {
        throw "erro";
    }
    
    int readRes = queue.enqueueReadBuffer(clmOutputVector, CL_TRUE, 0, DATA_SIZE * sizeof(Result), pOutputVector);
    
    if (readRes != 0) {
        throw "can't read result";
    }
    
    
    for(int i = 0; i < yRes; i++){
        for(int j = 0; j < xRes; j++)
        {
            Result result = pOutputVector[i*xRes + j];
            m_camera.m_pixels[i*xRes + j] = GetColorByResult(result);
        }
    }
    
    /*
    
    for(int i = 0; i < yRes; i++) {
        std::cout << i << std::endl;
        for(int j = 0; j < xRes; j++)
        {
            SRay ray = MakeRay(uvec2(j,i), 0.5);
            m_camera.m_pixels[i * xRes + j] = TraceRay(ray);
        }
    }
    */
}

void CTracer::SaveImageToFile(std::string fileName){
    int width = m_camera.m_resolution.x;
    int height = m_camera.m_resolution.y;
    
    fipImage *image = new fipImage(FIT_BITMAP, width, height, 24);
    
    int pitch = image->getScanWidth();
    unsigned char* imageBuffer = (unsigned char*)image->accessPixels();
    
    if (pitch < 0)
    {
        imageBuffer += pitch * (height - 1);
        pitch = - pitch;
    }
    
    int i, j;
    int imageDisplacement = 0;
    int textureDisplacement = 0;
    
    for (i = 0; i < height; i++)
    {
        for (j = 0; j < width; j++)
        {
            vec3 color = m_camera.m_pixels[textureDisplacement + j];
            
            imageBuffer[imageDisplacement + j * 3] = clamp(color.b, 0.0f, 1.0f) * 255.0f;
            imageBuffer[imageDisplacement + j * 3 + 1] = clamp(color.g, 0.0f, 1.0f) * 255.0f;
            imageBuffer[imageDisplacement + j * 3 + 2] = clamp(color.r, 0.0f, 1.0f) * 255.0f;
        }
        
        imageDisplacement += pitch;
        textureDisplacement += width;
    }
    
    image->save(fileName.c_str());
    delete image;
}

fipImage* CTracer::LoadImageFromFile(std::string fileName){
    fipImage *image = new fipImage;
    image->load(fileName.c_str());
    return image;
}


CTracer::~CTracer(){
    if(stars) stars->clear();
    if(disk) disk->clear();
    
    if(pInputVector1) delete pInputVector1;
    if(pInputVector2) delete pInputVector2;
    if(clSettings) delete clSettings;
    if(pOutputVector) delete pOutputVector;
    if(clCamera) delete clCamera;
    //if(settings) delete settings;
    
}
