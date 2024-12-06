Main purpose of this project is 
(1) to build a low-latency trading system following the code and steps from the book titled *Building Low Latency Applications with C++*, by Sourav Ghosh (1st edition); and 
(2) learn and express those lessons in the form of extra comments to the original files or additional markdown files.

My code gets built and executed in a Docker container.

To build the Docker container I use two files:
    - `Dockerfile`. 
    The file does four things:
        * Starts from an pre-existing image called `llapp`. This image, which I previously created, starts with an `ubuntu:22.04` image, and installs `gcc-11` and `cmake-3.23.2` on top of it.
        * Copies all files from `/home/cod-salmon/low-latency-app/inputs` into a new folder called `/usr/src`; then it sets `/usr/src` as the working directory.
        * Creates another folder `/usr/outputs`, where outputs from the code will be stored. The `compose.yaml` file will link the `/usr/outputs` from the container to a local folder (`/home/cod-salmon/low-latency-app/outputs`), from where the outputs can be accessed after the container is finished.
        * Builds according to the `CMakeLists.txt` file in `/home/cod-salmon/low-latency-app/inputs`, and keeps executables under `/usr/src/build`.
    - `compose.yaml`.
    The file does four things:
        * Builds a service called `low-latency-app` for which a container also called `low-latency-app` gets created.
        * Tells Docker to build the container using the input Dockerfile. 
        * Tells Docker to link `/usr/outputs` from the container to the local `/home/cod-salmon/low-latency-app/outputs`.
        * Tells Docker to run some command after building container.

and the following command (inside `/home/cod-salmon/low-latency-app`):

`docker compose up && docker compose build`
