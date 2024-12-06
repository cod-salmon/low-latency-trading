# This image has 
#       * gcc-11
#       * cmake-3.23.2 
#   burnt on it
FROM llapp:latest
# Copy all .h/.cpp and CMakeLists.txt files 
#   into working directory
COPY ./inputs/ /usr/src
WORKDIR /usr/src
# This is for bind mount
#   (to be able to access outputs after
#   docker container is finished processing)
RUN mkdir /usr/outputs
# Build executables according to CMakeLists.txt
#   - executables will be accessible inside the folder ./build
RUN mkdir ./build
RUN cmake -S . -B ./build
RUN make -C ./build






