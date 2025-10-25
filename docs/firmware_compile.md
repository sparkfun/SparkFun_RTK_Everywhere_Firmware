# Compiling Source

This is information about how to compile the RTK Everywhere firmware from source. This is for advanced users who would like to modify the functionality of the RTK products.

* [How SparkFun does it](#how-sparkfun-does-it)
* [Using Docker](#using-docker)
* [Compiling on Windows (Deprecated)](#compiling-on-windows-deprecated)

## How SparkFun does it

At SparkFun, we use GitHub Actions and a Workflow to compile each release of RTK Everywhere. We run the [compilation workflow](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/.github/workflows/compile-rtk-everywhere.yml) directly on GitHub. A virtual ubuntu machine installs the [Arduino CLI](https://github.com/arduino/arduino-cli/releases), installs the ESP32 Arduino core, patches the core in a couple of places, installs all the required libraries at the required version, converts the embedded HTML and Bootstrap JavaScript to binary zip format, and generates the firmware binary for the ESP32. That binary is either uploaded as an Artifact (by [non-release-build](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/.github/workflows/non-release-build.yml)) or pushed to the [SparkFun RTK Everywhere Firmware Binaries](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries) repo (by [compilation workflow](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/.github/workflows/compile-rtk-everywhere.yml)).

You are welcome to clone or fork this repo and do the exact same thing yourself. But you may need a paid GitHub Pro account to run the GitHub Actions, especially if you keep your clone / fork private.

If you run the [compilation workflow](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/.github/workflows/compile-rtk-everywhere.yml), it will compile the firmware and attempt to push the binary to the Binaries repo. This will fail as your account won't have the right permissions. The [non-release-build](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/.github/workflows/non-release-build.yml) is the one for you. The firmware binary will be attached as an Artifact to the workflow run. Navigate to Actions \ Non-Release Build, select the latest run of Non-Release Build, the binary is in the Artifacts.

You can then use the [SparkFun RTK Firmware Uploader](https://github.com/sparkfun/SparkFun_RTK_Firmware_Uploader) to upload the binary onto the ESP32.

## Using Docker

Installing the correct version of the ESP32 core and of each required Arduino library, is tedious and error-prone. Especially on Windows. We've lost count of the number of times code compilation fails on our local machines, because we had the wrong ESP32 core installed, or forgot to patch libbt or libmbedcrypto... It is much easier to sandbox the firmware compilation using an environment like [Docker](https://www.docker.com/).

Here is a step-by-step guide for how to install Docker and compile the firmware from scratch:

### Install Docker Desktop

* Head to [Docker](https://www.docker.com/) and create an account. A free "Personal" account will cover occasional compilations of the firmware
* Download and install [Docker Desktop](https://docs.docker.com/get-started/get-docker/) - there are versions for Mac, Windows and Linux. You may need to restart to complete the installation.
* Run the Desktop and sign in
* On Windows, you may see an error saying "**WSL needs updating** Your version of Windows Subsystem for Linux (WSL) is too old". If you do:
    * Open a command prompt
	* Type `wsl --update` to update WSL. At the time of writing, this installs Windows Subsystem for Linux 2.6.1
	* Restart the Docker Desktop
* If you are using Docker for the first time, the "What is a container?" and "How do I run a container?" demos are useful
    * On Windows, you may need to give Docker Desktop permission to access to your Network
	* You can Stop the container and Delete it when you are done

### Clone, fork or download the RTK Everywhere Firmware repo

To build the RTK Everywhere Firmware, you obviously need a copy of the source code.

If you are familiar with Git and GitHub Desktop, you can clone the RTK Everywhere Firmware repo directly into GitHub Desktop:

<figure markdown>
![Clone RTK Everywhere with GitHub Desktop](./img/CompileSource/Clone_Repo_To_GitHub_Desktop.png)
<figcaption markdown>
Clone RTK Everywhere with GitHub Desktop
</figcaption>
</figure>

If you want to _contribute_ to RTK Everywhere, and already have a GitHub account, you can Fork the repo:

<figure markdown>
![Fork RTK Everywhere](./img/CompileSource/Fork_Repo.png)
<figcaption markdown>
Fork a copy of RTK Everywhere
</figcaption>
</figure>

Clone your fork to your local machine, make changes, and send us a Pull Request. This is exactly what the SparkFun Team do when developing the code.

If you don't want to do either of those, you can simply Download a Zip copy of the repo instead. You will receive a complete copy as a Zip file. You can do this from the green **Code** button, or click on the icon below to download a copy of the main (released) branch:

[![Download ZIP](./img/CompileSource/Download_Zip.png)](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/archive/refs/heads/main.zip "Download ZIP (main branch)")

For the real Wild West experience, you can also download a copy of the `release_candidate` code branch. This is where the team is actively changing and testing the code, before it becomes a full release. The code there will _usually_ compile and will _usually_ work, but we don't guarantee it! We may be part way through implementing some breaking changes at the time of your download...

[![Download ZIP - release candidate](./img/CompileSource/Download_Zip.png)](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/archive/refs/heads/release_candidate.zip "Download ZIP (release_candidate branch)")

### Running the Dockerfile to compile the firmware

* Make sure you have Docker Desktop running
* Open a Command Prompt and `cd` into the SparkFun_RTK_Everywhere_Firmware folder
* Check you are in the right place. Type `dir` and hit enter. You should see the following files and folders:

```
    .gitattributes
    .github
    .gitignore
    docs
    Firmware
    Graphics
    Issue_Template.md
    License.md
    README.md
```

* `cd Firmware` and then `dir` again. You should see:

```
    Dockerfile
    readme.md
    RTKEverywhere.csv
    RTKEverywhere_8MB.csv
```

* The file we will be using is the `Dockerfile`.
* Type:

```
docker build -t rtk_everywhere_firmware .
```

* If you want to see the full build progress including the output of echo or ls, use:

```
docker build -t rtk_everywhere_firmware --progress=plain .
```

* If you rebuild completely from scratch, use:

```
docker build -t rtk_everywhere_firmware --progress=plain --no-cache .
```

## Compiling on Windows (Deprecated)

The SparkFun RTK Everywhere Firmware is compiled using Arduino CLI (currently [v1.0.4](https://github.com/arduino/arduino-cli/releases)). To compile:

1. Install [Arduino CLI](https://github.com/arduino/arduino-cli/releases).
2. Install the ESP32 core for Arduino:

		arduino-cli core install esp32:esp32@3.0.7

	!!! note
		Use v3.0.7 of the core.

	!!! note
		We use the 'ESP32 Dev Module' for pin numbering.

3. Obtain each of the libraries listed in [the workflow](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/.github/workflows/compile-rtk-everywhere.yml#L77) either by using git or the Arduino CLI [library manager](https://arduino.github.io/arduino-cli/0.21/commands/arduino-cli_lib_install/). Be sure to obtain the version of the library reflected in the [workflow](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/.github/workflows/compile-rtk-everywhere.yml#L77). Be sure to include the external libraries (You may have to enable external library support in the CLI).
4. RTK Everywhere uses a custom partition file. Download the [RTKEverywhere.csv](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/Firmware/RTKEverywhere.csv) or [RTKEverywhere_8MB.csv](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/Firmware/RTKEverywhere_8MB.csv) for the RTK Postcard.
5. Add *RTKEverywhere.csv* partition table to the Arduino partitions folder. It should look something like

		C:\Users\[user name]\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.0.7\tools\partitions\RTKEverywhere.csv

	This will increase the program partitions, as well as the SPIFFs partition to utilize the full 16MB of flash (8MB in the case of the Postcard).

6. Compile using the following command

		arduino-cli compile 'Firmware/RTK_Everywhere' --build-property build.partitions=RTKEverywhere --build-property upload.maximum_size=4055040 --fqbn esp32:esp32:esp32:FlashSize=16M,PSRAM=enabled

8. Once compiled, upload to the device using the following command where `[COM_PORT]` is the COM port on which the RTK device is located (ie `COM42`).

		arduino-cli upload -p [COM_PORT] --fqbn esp32:esp32:esp32:UploadSpeed=512000,FlashSize=16M 'Firmware/RTK_Everywhere'

If you are seeing the error:

> text section exceeds available space ...

You have either not replaced the partition file correctly or failed to include the 'upload.maximum_size' argument in your compile command. See steps 4 through 6 above.

!!! note
	There are a variety of compile guards (COMPILE_WIFI, COMPILE_AP, etc) at the top of RTK_Everywhere.ino that can be commented out to remove them from compilation. This will greatly reduce the firmware size and allow for faster development of functions that do not rely on these features (serial menus, system configuration, logging, etc).

