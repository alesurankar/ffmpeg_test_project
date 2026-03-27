this is an ffmpeg test project

1. download from here: https://www.gyan.dev/ffmpeg/builds/

2. install: ffmpeg-release-full-shared.7z

3. extract into your libraries folder

4. check if it is visible:  ffmpeg -list_devices true -f dshow -i dummy

5. bin/ copy all dll's into your build folder (Debug or Release)

6. Properties/C++/General/Additional Include Directories add: Libraries\ffmpeg\include

7. Properties/Linker/General/Additional Library Directories add: Libraries\ffmpeg\lib

8. Properties/Linker/Input/Additional Dependencies add: 
avformat.lib
avcodec.lib
avutil.lib
swscale.lib
swresample.lib
avdevice.lib
