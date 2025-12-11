::Uncomment "docker builder prune -f" below to clear the build cache
::This will resolve issues with arduino-cli not being able to find the latest libraries
::docker builder prune -f
docker build -t rtk_everywhere_firmware --no-cache-filter deployment .
docker create --name=rtk_everywhere rtk_everywhere_firmware:latest
docker cp rtk_everywhere:/RTK_Everywhere.ino.bin .
docker cp rtk_everywhere:/RTK_Everywhere.ino.elf .
docker cp rtk_everywhere:/RTK_Everywhere.ino.bootloader.bin .
docker container rm rtk_everywhere
