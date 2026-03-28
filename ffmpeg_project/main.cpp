#include <iostream>
#include <string>
#include <direct.h>
#include "WebcamCapture.h"



int main() {
    char* raw_cwd = _getcwd(nullptr, 0);  // allocate C-string
    if (!raw_cwd) {
        std::cerr << "Failed to get current working directory!" << std::endl;
        return -1;
    }
    std::string cwd(raw_cwd);
    std::cout << "Current working directory: " << cwd << std::endl;
    free(raw_cwd);
    
    WebcamCapture Cam("video=USB2.0 HD UVC WebCam", cwd);

    if (!Cam.Init()) return -1;
    if (!Cam.CaptureFrame()) return -1;
    if (!Cam.ConvertToRGB()) return -1;
    if (!Cam.SetupPNGEncoder()) return -1;
    if (!Cam.EncodeAndSave()) return -1;

    return 0;
}