# This image has 
#       * gcc-11
#       * cmake-3.23.2 
#   burnt on it
FROM llapp:latest
# This makes sure that ninja executable is found
ENV PATH=/usr/src/:$PATH
# Copy all .h/.cpp and CMakeLists.txt files 
#   into working directory
COPY . /usr/src
WORKDIR /usr/src
# This is for bind mount
#   (to be able to access outputs after
#   docker container is finished processing)
RUN mkdir /usr/outputs
# Build executables according to CMakeLists.txt
#   - executables will be accessible inside the folder ./build
RUN mkdir ./cmake-build-release
RUN cmake -S . -B ./cmake-build-release
RUN make -C ./cmake-build-release

# To run jupyter and analyse data,
#   comment out cmake & make lines above
#   and uncomment this:
#RUN apt-get install -y python3
#RUN apt-get install -y python3-pip 
# requirements.txt contains modules
#   such as numpy, matplotlib and jupyter
#RUN pip install --no-cache-dir -r requirements.txt
# Set the default command to run Jupyter Notebook
#CMD ["jupyter", "notebook", "--ip=0.0.0.0", "--port=8888", "--no-browser", "--allow-root", "--NotebookApp.token=''"]







