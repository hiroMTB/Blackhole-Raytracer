#include "Tracer.h"
#include "stdio.h"
#include "cl.hpp"
#include <iostream>
#include <FreeImage.h>
#include "Types.h"

using namespace std;

int main(int argc, char** argv)
{
    CTracer tracer;
    CScene scene;
    
    int xRes = 1920;  // Default resolution
    int yRes = 1080;
    
    Settings *settings;
    Settings *defaultSettings;
    
    float xViewAngle = 2;
    float blackholeMass = 8.57e+36;
    float diskCoef = 8;
    float cameraX = 1e11*0.2;  // 1e11*2.5;
    float cameraY = 1;  // 11e10;
    float cameraZ = 0;
    
    defaultSettings = new Settings(xRes,
                            yRes,
                            xViewAngle,
                            cameraX,
                            cameraY,
                            cameraZ,
                            blackholeMass,
                            diskCoef);
    
    if(argc == 2) // There is input file in parameters
    {
        FILE* file = fopen(argv[1], "r");
        if(file){
            int xResFromFile = 0;
            int yResFromFile = 0;
            float xViewAngleFromFile = 0;
            float cameraXFromFile = 0;
            float cameraYFromFile = 0;
            float cameraZFromFile = 0;
            float blackholeMassFromFile = 0;
            float diskCoefFromFile = 0;
            if(fscanf(file,
                      "%d %d %f %f %f %f %f %f",
                      &xResFromFile,
                      &yResFromFile,
                      &xViewAngleFromFile,
                      &cameraXFromFile,
                      &cameraYFromFile,
                      &cameraZFromFile,
                      &blackholeMassFromFile,
                      &diskCoefFromFile) == 8)
            {
                xRes = xResFromFile;
                yRes = yResFromFile;
                settings = new Settings(xResFromFile,
                                    yResFromFile,
                                    xViewAngleFromFile,
                                    cameraXFromFile,
                                    cameraYFromFile,
                                    cameraZFromFile,
                                    blackholeMassFromFile,
                                    diskCoefFromFile);
                delete defaultSettings;
            }
            else {
                settings = defaultSettings;
                printf("Invalid config format! Using default parameters.\r\n");
            }
            
            fclose(file);
        }else{
            settings = defaultSettings;
            printf("Invalid config path! Using default parameters.\r\n");
        }
    }
    else{
        settings = defaultSettings;
        printf("No config! Using default parameters.\r\n");
    }
    tracer.m_pScene = &scene;
    tracer.settings = settings;
    tracer.setup();
    
    for(int i=0; i<1000; i++)
    {
        
        try {
            tracer.RenderImage();
            
        } catch (char const *a) {
            std::cout << "Error: " << a << std::endl;
            return -1;
        } catch (int a) {
            std::cout << "Error # " << a << std::endl;
            return -1;
        }
        char m[255];
        sprintf(m, "./render/%04d.png", i);
        tracer.SaveImageToFile(m);
        
        cout << m << endl;
        
        double adder = 2e11 / 1000.0;
        tracer.settings->cameraX += adder*0.5;
        tracer.settings->cameraY += adder;
        //tracer.settings->cameraZ *= 1.02;
        //tracer.settings->xViewAngle += 0.0001;

    }
    
    delete settings;
}
