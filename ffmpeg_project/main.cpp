extern "C" {
#include <libavformat/avformat.h>
}

#include <iostream>

int main() {
    avformat_network_init();
    std::cout << "FFmpeg headers and libs work!" << std::endl;
    return 0;
}