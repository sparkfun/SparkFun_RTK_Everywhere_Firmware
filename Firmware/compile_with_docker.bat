docker build -t rtk_everywhere_firmware --no-cache-filter deployment .
docker create --name=rtk_everywhere rtk_everywhere_firmware:latest
docker cp rtk_everywhere:/RTK_Everywhere.ino.bin .
docker cp rtk_everywhere:/RTK_Everywhere.ino.elf .
docker cp rtk_everywhere:/RTK_Everywhere.ino.bootloader.bin .
docker container rm rtk_everywhere
